/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Source code inspired and based on original work of Peter Simons and Ralph Babel
 * See https://www.nongnu.org/fastcgi/
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <iostream>
#include <fstream>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <cpp4scripts.hpp>

#include "../fcgisettings.h"
#include "Driver.hpp"
#include "Request.hpp"

const uint64_t t64_GET = 0x544547;
const uint64_t t64_POST = 0x54534f50;
const uint64_t t64_PUT = 0x545550;
const uint64_t t64_PATCH = 0x4843544150;
const uint64_t t64_DELETE = 0x4554454c4544;

extern FILE* trace;

using namespace std;
using namespace c4s;

namespace fcgi_driver {

size_t Request::input_size = 0;
size_t Request::param_size = 0;
Driver* Request::driver = 0;

// -------------------------------------------------------------------------------------------------
void
Handler::abort(Request*)
{
    // We quietly ignore unimplemented abort handlers. Logged by request.
}
// -------------------------------------------------------------------------------------------------
void Handler::event(HandlerEvent)
{
    // We quietly ignore unimplemented event handlers.
}

// -------------------------------------------------------------------------------------------------
void
Request::log(const char* str)
{
    TRACE(str);
}
// -------------------------------------------------------------------------------------------------
Request::Request()
  : rbin(Request::input_size)
  , params(Request::param_size)
{
    role = RESPONDER;
    fd_spool = -1;
    mp_count = 0;
    memset(uploads, 0, sizeof(uploads));
    upload_ndx = 0;
    clear();
}
Request::Request(const Request& orig)
  : rbin(Request::input_size)
  , params(Request::param_size)
{
    int ndx;
    role = orig.role;
    fd_spool = -1;
    clear();
    memset(uploads, 0, sizeof(uploads));
    for (ndx = 0; ndx < orig.upload_ndx; ndx++) {
        uploads[ndx] = new UploadFile(*orig.uploads[ndx]);
    }
    upload_ndx = ndx;
}

// -------------------------------------------------------------------------------------------------
Request::~Request()
{
    // This will close the files if necessary
    clear();
}
// -------------------------------------------------------------------------------------------------
void
Request::clear()
{
#ifdef UNIT_TEST
    if (app_data) {
        TRACE("Request::clear - (%d) app_data is still defined. Forgot to cleanup?\n", id);
    }
#endif
    memset(&pfd, 0, sizeof(pollfd));
    id = 0;
    handler = 0;
    app_data = 0;
    html_type = HTML_GET;
    state = RQS_WAIT;
    bound_len = 0;
    stdin_len = 0;
    spool_size = 0;
    app_status = 0;
    rbpos = rbout;
    rbsend = rbout;
    stdout_count = 0;
    flags.clear();
    memset(boundary, 0, sizeof(boundary));
    memset(uri, 0, sizeof(uri));
    if (fd_spool >= 0) {
        close(fd_spool);
        fd_spool = -1;
    }
    rbin.clear();
    params.clear();
    for (int ndx = 0; ndx < REQ_MAX_UPLOADS; ndx++) {
        if (uploads[ndx])
            delete uploads[ndx];
        uploads[ndx] = 0;
    }
    upload_ndx = 0;
}
// -------------------------------------------------------------------------------------------------
void
Request::setPollFd(pollfd* fd)
{
    if (fd) {
        memcpy(&pfd, fd, sizeof(pollfd));
        state = RQS_PARAMS;
    } else {
        TRACE("Request::setPollFd (%d) - closing socket %d\n", id, pfd.fd);
        close(pfd.fd); // !!! Close the accepted sockect !!!
        clear();
    }
}
// -------------------------------------------------------------------------------------------------
void
Request::write(const char* data, uint16_t length)
{
    // Check the validity
    if (state != RQS_STDIN && state != RQS_OPEN) {
        TRACE("Request::write (%d) - Attempt to write into closed request:%d.\n", id, pfd.fd);
        return;
    }
    if (length == 0)
        length = strlen(data);
    uint16_t max = REQ_MAX_OUT - (rbpos - rbout);
    if (max < length) {
        TRACE("Request::write (%d) - Not enough room for %d bytes, %d max. Flushing!\n", id, length,
              max);
        flush();
    }
    memcpy(rbpos, data, length);
    rbpos += length;
    TRACE("Request::write (%d) - length=%d total=%d\n", id, length, (int)(rbpos - rbout));
    pfd.events |= POLLOUT;
}
// -------------------------------------------------------------------------------------------------
bool
Request::writeFd(int fd)
{
    uint16_t max;
    ssize_t br, total;
    // Check the validity
    if (state != RQS_STDIN && state != RQS_OPEN) {
        TRACE("Request::writeFd (%d) - Attempt to write into closed request:%d\n", id, pfd.fd);
        return false;
    }
#ifdef UNIT_TEST
    ssize_t original_max;
    original_max = REQ_MAX_OUT - (rbpos - rbout);
#endif
    total = 0;
    do {
        max = REQ_MAX_OUT - (rbpos - rbout);
        if (max == 0) {
            flush();
            max = REQ_MAX_OUT;
        }
        br = ::read(fd, rbpos, max);
        if (br == -1) {
            TRACE("Request::writeFd (%d) - source file %d read error %d.\n", id, pfd.fd, errno);
            clearRbOut();
            return false;
        }
        rbpos += br;
        total += br;
    } while (br);
    pfd.events |= POLLOUT;
    TRACE("Request::writeFd (%d) - from %d; max %ld; in fd %ld; pending %d\n", id, pfd.fd,
          original_max, total, (int)(rbpos - rbout));
    flush();
    return true;
}
// -------------------------------------------------------------------------------------------------
void
Request::flush()
{
    struct pollfd pf;
    int rounds = 0;

    if (getOutPending() == 0) {
        TRACE("Request::flush - nothing to send!\n");
        return;
    }
    pfd.events |= POLLOUT; // for isWrite()
    state = RQS_FLUSH;

    pf.fd = pfd.fd;
    pf.events = POLLOUT;
    pf.revents = 0;
    // stdout_count is updated in send!
    while (isWrite() && rounds < 5) {
        if (rounds > 0) {
            poll(&pf, 1, 200);
        }
        send();
        rounds++;
    }
    // rewind buffer
    clearRbOut();
    stdout_count = 0;
    TRACE("Request::flush (%d) - fd %d with %d rounds.\n", id, pfd.fd, rounds);
}
// -------------------------------------------------------------------------------------------------
void
Request::send()
{
    size_t send_size = 0;
    ssize_t bw;

    send_size = rbpos > rbsend ? rbpos - rbsend : 0;
    if (!send_size) {
        // Out of data to send. See if we are about to finish.
        if (state == RQS_OPEN) {
            Header header(TYPE_STDOUT, id, 0);
            memcpy(rbout, &header, sizeof(header));
            rbpos = rbout + sizeof(header);
            EndRequestMsg erm(id, app_status, REQUEST_COMPLETE);
            memcpy(rbpos, &erm, sizeof(erm));
            rbsend = rbout;
            rbpos += sizeof(erm);
            send_size = rbpos - rbout;
            state = RQS_END;
            pfd.events |= POLLOUT;
            TRACE("Request::send (%d) - END size=%ld\n", id, send_size);
        } else if (state == RQS_END) {
            TRACE("Request::send (%d) - All done, setting EOF 1.\n", id);
            pfd.events &= ~POLLOUT;
            state = RQS_EOF;
            return;
        } else if (state == RQS_FLUSH) {
            pfd.events &= ~POLLOUT;
            state = RQS_OPEN;
            return;
        } else {
            TRACE("Request::send (%d) - nothing to do\n", id);
            return;
        }
    } else if (stdout_count == 0) {
        // Http status can be set only with headers i.e. first stdout
        if (app_status > 0) {
            // Prepend the status line.
            char stat_line[64];
            size_t stat_len = sprintf(stat_line, "Status: %d\r\n", app_status);
            size_t now = rbpos - rbout;
            if (now + stat_len < REQ_MAX_OUT) {
                memmove(rbout + stat_len, rbout, now);
                memcpy(rbout, stat_line, stat_len);
                send_size += stat_len;
            }
            app_status = 0;
        }
        // Send normal header
        Header header(TYPE_STDOUT, id, send_size);
        bw = ::write(pfd.fd, &header, sizeof(Header));
        if (bw < 0) {
            if (errno != EAGAIN) {
                TRACE("Request::send (%d) - write error 1. errno=%d\n", id, errno);
                clearRbOut();
                state = RQS_OPEN;
                app_status = 500;
                return;
            }
            TRACE("Request::send (%d) - Retry header\n", id);
            pfd.events |= POLLOUT;
            return; // try again
        }
        TRACE("Request::send (%d) - header=%ld data=%ld\n", id, sizeof(header), send_size);
    }
    bw = ::write(pfd.fd, rbsend, send_size);
    if (bw < 0) {
        if (errno != EAGAIN) {
            // OK. We are in trouble. Cannot write any more.
            TRACE("Request::send (%d) - write error 2. errno=%d\n", id, errno);
            if (state == RQS_END) {
                state = RQS_EOF;
                return;
            }
            clearRbOut();
            app_status = 500;
            state = RQS_OPEN;
            return;
        }
        TRACE("Request::send (%d) - errno=AGAIN. size=%ld\n", id, send_size);
        pfd.events |= POLLOUT;
        stdout_count++;
        return;
    } else {
        TRACE("Request::send (%d) - size = %ld; bw=%ld\n", id, send_size, bw);
    }
    stdout_count++;
    rbsend += bw;
}
// -------------------------------------------------------------------------------------------------
void
Request::end(uint32_t _app_status)
{
    if (state != RQS_OPEN) {
        TRACE("Request::end (%d) - Attempt to end closed request.\n", id);
        return;
    }
    app_status = _app_status;
    if (stdout_count == 0 && rbout == rbpos) { // nothing to send
        TRACE("Request::end (%d) - status %d. Nothing to send!\n", id, app_status);
    } else {
        TRACE("Request::end (%d) - status %d with %ld bytes\n", id, app_status, getOutPending());
    }
    pfd.events |= POLLOUT;
}
// -------------------------------------------------------------------------------------------------
void
Request::processTestSpool()
{
    processSpool();
}
// -------------------------------------------------------------------------------------------------
bool
Request::openMPSpool(const char* data, int len)
{
    char spool_name[30], spool_path[255];
    // Open spool file
    spool_path[0] = 0;
    if (driver->cache_path.size())
        strcpy(spool_path, driver->cache_path.c_str());
    sprintf(spool_name, "req-spool_%d", id);
    strcat(spool_path, spool_name);
    fd_spool = open(spool_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd_spool == -1) {
        CS_VAPRT_ERRO(
            "Request::openMPSpool - unable to open spool file for multipart imput. Errno %d.",
            errno);
        end(500);
        return false;
    }
    // Note! preceeding \r\n combination is counted into boundary even though it is not specified
    // boundary line.  We add these bytes here manually.. and write the initial boundary to disk.
    ::write(fd_spool, "\r\n", 2);
    flags.set(FLAG_MULTIP | FLAG_SPOOLING);
    boundary[0] = '\r';
    boundary[1] = '\n';
    boundary[2] = '-';
    boundary[3] = '-';
    strncpy(boundary + 4, data, len);
    bound_len = len + 4;
    boundary[bound_len] = 0;
    spool_size = 0;
    TRACE("Request::openMPSpool - multipart boundary=%s\n", boundary + 2);
    return true;
}
// -------------------------------------------------------------------------------------------------
void
Request::writeSpool(uint16_t msg_len, const char* msg)
{
    // If msg is null, rbin is used !

    if (fd_spool > 0) {
        TRACE("Request::writeSpool (%d) - disc spool %d bytes\n", id, msg_len);
        if (msg)
            ::write(fd_spool, msg, msg_len);
        else
            rbin.read_into(fd_spool, msg_len);
        spool_size += msg_len;
        return;
    }
    if (spool_size + msg_len < REQ_MAX_MEMSTDIN) {
        TRACE("Request::writeSpool (%d) - memory spool %d bytes\n", id, msg_len);
        if (msg)
            strncpy(stdin_buffer + spool_size, msg, msg_len);
        else
            rbin.read(stdin_buffer + spool_size, msg_len);
        spool_size += msg_len;
        return;
    }
    // We have run out of memory buffer room. Open disk spool
    char spool_name[30], spool_path[255];
    spool_path[0] = 0;
    if (driver->cache_path.size())
        strcpy(spool_path, driver->cache_path.c_str());
    sprintf(spool_name, "req-spool_%d_%d", id, mp_count);
    strcat(spool_path, spool_name);
    fd_spool = open(spool_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd_spool == -1) {
        CS_VAPRT_ERRO("Request::writeSpool - unable to open spool file for stdin imput. Errno %d.",
                      errno);
        end(500);
        return;
    }
    mp_count++;
    ::write(fd_spool, stdin_buffer, spool_size);
    if (msg)
        ::write(fd_spool, msg, msg_len);
    else
        rbin.read_into(fd_spool, msg_len);
    spool_size += msg_len;
    TRACE("Request::writeSpool (%d) - moved memory spool to disk with %ld bytes total.\n", id,
          spool_size);
}
// -------------------------------------------------------------------------------------------------
bool
Request::processSpool()
{
    if (spool_size == 0) {
        return true;
    }
    if (flags.is(FLAG_MULTIP)) {
        processMultipart();
        return true;
    }
    if (flags.is(FLAG_BODYDATA)) {
        processBodyData();
        return true;
    }
    TRACE("Request::processSpool - processing %ld bytes of stdin parameter data (fd=%d)\n",
          spool_size, fd_spool);
    // If we have memory spool
    if (fd_spool == -1) {
        try {
            params.init_push(spool_size);
            for (size_t ndx = 0; ndx < spool_size; ndx++) {
                params.push_back(stdin_buffer[ndx]);
            }
            params.end_push();
            spool_size = 0;
        } catch (const runtime_error& re) {
            params.clear();
            end(500);
#ifdef UNIT_TEST
            fprintf(trace,
                    "Request::processSpool - Unable to process stdin form, syntax error in:\n");
            fwrite(stdin_buffer, 1, spool_size, trace);
            fprintf(trace, "\n");
#else
            CS_PRINT_WARN("Request::processSpool - Unable to process stdin form, "
                          "parameter syntax error 1. Aborting request.");
#endif
            return false;
        }
        return true;
    }
    // This is disc spooled data
    if (lseek(fd_spool, 0, SEEK_SET) == -1) {
        TRACE("Request::processSpool - Unable to rewind spool %d. (errno %d)\n", id, errno);
        CS_PRINT_WARN("Request::processSpool - Unable to rewind spool file");
        end(500);
        return false;
    }

    try {
        int br, total = 0;
        char buffer[2048];
        params.init_push(spool_size);
        while (total < (int)spool_size) {
            br = ::read(fd_spool, buffer, sizeof(buffer));
            if (!br)
                break;
            if (br == -1) {
                TRACE("Request::processSpool - error %d reading stdin spool \n", errno);
                break;
            }
            for (int ndx = 0; ndx < br; ndx++) {
                params.push_back(buffer[ndx]);
            }
            total += br;
        }
        params.end_push();
        spool_size = 0;
    } catch (const runtime_error& re) {
        params.clear();
        end(500);
#ifdef UNIT_TEST
        TRACE("Request::processSpool - Unable to process spooled data.\n");
#else
        CS_PRINT_WARN(
            "Request::processSpool - Unable to process stdin form, parameter syntax error 2. "
            "Aborting request.");
#endif
        return false;
    }
    close(fd_spool);
    return true;
}
// -------------------------------------------------------------------------------------------------
size_t
Request::parseMultipart(char* data, size_t dlen, ParseData* pd)
{
    char* ptr = data;
    char* end = ptr + dlen;
    char *fldbeg, *fldend, *crlf;
    size_t max, max2, bw;
    uint64_t key;

    switch (pd->mp_state) {
    case MP_BEGIN:
        TRACE("MP_BEGIN: %ld\n", pd->spool_offset);
        if (dlen < bound_len)
            return 0;
        fldbeg = (char*)memmem(ptr, end - ptr, boundary, bound_len);
        if (!fldbeg)
            return dlen - bound_len + 1;
        ptr = fldbeg + bound_len;
        if (ptr[0] == '-' && ptr[1] == '-') {
            pd->mp_state = MP_FINISH;
            ptr += 2;
            break;
        }
        if (ptr[0] != '\r' && ptr[1] != '\n') {
            TRACE("  cr-ln not found. Offset %ld\n", ptr - data);
            throw runtime_error("MP_BEGIN - Unable to find \\r\\n separator.");
        }
        ptr += 2;
        pd->mp_state = MP_HEADER;
        break;

    case MP_HEADER:
        TRACE("MP_HEADER: %ld\n", pd->spool_offset);
        crlf = ptr;
        // find the end of header
        while (crlf[0] != '\r' && crlf[1] != '\n' && crlf < end)
            crlf++;
        if (crlf == end)
            break;
        *crlf = 0;
        // Find the field name.
        fldbeg = strstr(ptr, "form-data; name=\"");
        if (!fldbeg) {
#ifdef UNIT_TEST
            fwrite("  Unknown part:", 1, 15, trace);
            fwrite(ptr, crlf - ptr, 1, trace);
            fwrite("\n", 1, 1, trace);
#endif
            ptr = crlf + 2;
            pd->mp_state = MP_BEGIN;
            break;
        }
        fldbeg += 17;
        fldend = strchr(fldbeg, '"');
        strncpy(pd->fldname, fldbeg, fldend - fldbeg);
        pd->fldname[fldend - fldbeg] = 0;
        TRACE("  Parameter: %s\n", pd->fldname);
        // Find possible file name
        fldbeg = strstr(fldend, "filename=\"");
        if (fldbeg) {
            fldbeg += 10;
            fldend = strchr(fldbeg, '"');
            if (fldend == fldbeg) {
                ptr = fldend + 3; // "\r\n
                pd->mp_state = MP_BEGIN;
                TRACE("  No filename given. Skipping file.\n");
                break;
            }
            // Copy only last part of client name
            memset(pd->extfilename, 0, REQ_MAX_FILENAME);
            if (fldend - fldbeg > REQ_MAX_FILENAME)
                fldbeg = fldend - REQ_MAX_FILENAME + 1;
            strncpy(pd->extfilename, fldbeg, fldend - fldbeg);
            ptr = fldend + 3; // quote + \r\n
            pd->mp_state = MP_FILETYPE;
            TRACE("  Filename: %s\n", pd->extfilename);
        } else {
            ptr = crlf + 2;
            pd->fldptr = pd->flddata;
            if (ptr[0] != '\r' && ptr[1] != '\n') {
                TRACE("  FLDDATA missing \\r\\n. Found [0]=%x [1]=%x\n", ptr[0], ptr[1]);
                throw runtime_error("MP_HEADER missing \\r\\n separator");
            }
            ptr += 2;
            if (!strncmp(ptr, boundary, bound_len))
                pd->mp_state = MP_BEGIN;
            else
                pd->mp_state = MP_FLDDATA;
        }
        break;

    case MP_FILETYPE:
        TRACE("MP_FILETYPE: %ld\n", pd->spool_offset);
        // We ignore the content type and skip the line
        fldend = ptr;
        while (fldend < end) {
            if (!strncmp(fldend, "\r\n\r\n", 4))
                break;
            fldend++;
        }
        // We need to have at least separator + 4 left
        if (fldend == end || dlen - (fldend - data) < bound_len) {
            break;
        }
        ptr = fldend + 4;
        // If next is boundary the file data does not exist.
        if (!strncmp(ptr, boundary, bound_len)) {
            pd->mp_state = MP_BEGIN;
            break;
        }
        // Open temporary transfer file for file data
        createXferFile(pd);
        pd->mp_state = MP_FILEDATA;
        TRACE("  File data offset: %ld\n", pd->spool_offset + (ptr - data));
        break;

    case MP_FILEDATA:
        if (dlen < bound_len)
            break;
        fldbeg = (char*)memmem(ptr, dlen, boundary, bound_len);
        if (!fldbeg) {
            bw = ::write(pd->fd_upload, ptr, dlen - bound_len + 1);
            pd->upfile->bytes += bw;
            ptr += bw;
            break;
        }
        // Write the data
        bw = ::write(pd->fd_upload, ptr, fldbeg - ptr);
        ::close(pd->fd_upload);
        pd->fd_upload = -1;
        pd->upfile->bytes += bw;
        TRACE("\n  final:%ld; total:%ld\n", bw, pd->upfile->bytes);
        ptr += bw;
        pd->mp_state = MP_BEGIN;
        // If we happened to write empty file:
        if (pd->upfile->bytes == 0) {
            unlink(pd->upfile->internal);
            // TODO: remove parameter pd->fldname
            TRACE("  No content in uploaded file. Stump removed.\n");
        }
        break;

    case MP_FLDDATA:
        TRACE("MP_FLDDATA: %ld\n", pd->spool_offset);
        if (dlen < bound_len)
            break;
        max = sizeof(pd->flddata) - (pd->fldptr - pd->flddata);
        if (!max) {
            ptr = end;
            TRACE("  Parameter data field full!\n");
            break;
        }
        if (max > dlen - bound_len)
            max = dlen - bound_len;
        fldbeg = (char*)memmem(ptr, dlen, boundary, bound_len);
        if (fldbeg) {
            // Boundary found.
            max2 = fldbeg - ptr;
            if (max > max2)
                max = max2;
            if (max == 0)
                throw runtime_error("MP_FLDDATA unexpected empty field.");
            pd->mp_state = MP_BEGIN;
        }
        memcpy(pd->fldptr, ptr, max);
        pd->fldptr += max;
        ptr += max;
        if (fldbeg) {
            *pd->fldptr = 0;
            key = params.add(pd->fldname, strlen(pd->fldname), pd->flddata);
            if (!key) {
#ifdef UNIT_TEST
                CS_VAPRT_WARN("Request::parseMultipart - Unable to add param %s with value %s",
                              pd->fldname, pd->flddata);
#else
                TRACE("  Parameter %s add failed.\n", pd->fldname);
#endif
            }
#ifdef UNIT_TEST
            else {
                fprintf(trace, "  Adding param %s (%lx) = ", pd->fldname, key);
                fwrite(pd->flddata, 1, max > 200 ? 200 : max, trace);
                fwrite("\n", 1, 1, trace);
            }
#endif
        }
        break;

    case MP_FINISH:
        TRACE("MP_FINISH\n  FINISH is inavalid state in parsing\n");
        break;
    } // end: switch(pd->mp_state)
    return ptr - data;
}
// -------------------------------------------------------------------------------------------------
void
Request::processMultipart()
{
    const size_t MAX_MP = 10000;
    static char mpdata[MAX_MP];
    char* mp_ptr;
    ssize_t brmax;
    size_t br, used, read_max, carry_size = 0;
    ParseData pd;

#ifdef UNIT_TEST
    TRACE("Request::processMultipart - parsing multipart %ld bytes\n", spool_size);
#else
    CS_VAPRT_TRCE("Request::processMultipart - parsing multipart %ld bytes", spool_size);
#endif
    // Rewind our spool file to the beginning
    if (lseek(fd_spool, 0, SEEK_SET) == -1) {
#ifdef UNIT_TEST
        TRACE("Request::processMultipart - Unable to rewind spool %d. (errno %d)\n", id, errno);
#else
        CS_PRINT_ERRO("Request::processMultipart - Unable to process multipart form");
#endif
        return;
    }
    brmax = ::read(fd_spool, mpdata, MAX_MP);
    if (brmax == -1) {
#ifdef UNIT_TEST
        TRACE("Request::processMultipart - Unable to read from spool %d. (errno %d)\n", id, errno);
#else
        CS_PRINT_ERRO("Request::processMultipart - Unable to process multipart form");
#endif
        return;
    }
    TRACE("Request::processMultipart - Initial read %ld bytes.\n", brmax);
    try {
        while (brmax) {
            br = brmax + carry_size;
            mp_ptr = mpdata;
            while (br) {
                used = parseMultipart(mp_ptr, br, &pd);
                if (pd.mp_state == MP_FINISH)
                    goto processMultipart_DONE;
                if (!used)
                    break;
                pd.spool_offset += used;
                br -= used;
                mp_ptr += used;
            }
            if (br && br < (size_t)brmax) {
                carry_size = MAX_MP - (mp_ptr - mpdata);
                memmove(mpdata, mp_ptr, carry_size);
                read_max = MAX_MP - carry_size;
                mp_ptr = mpdata + carry_size;
            } else {
                read_max = MAX_MP;
                mp_ptr = mpdata;
                carry_size = 0;
            }
            brmax = ::read(fd_spool, mp_ptr, read_max);
            TRACE("Request::processMultipart - more data: max=%ld; available=%ld; carry over %ld\n",
                  read_max, brmax, carry_size);
        }
    } catch (const runtime_error& re) {
#ifdef UNIT_TEST
        TRACE("Request::processMultipart - Syntax error: %s \n", re.what());
#else
        CS_PRINT_WARN("WARNING: Driver::process_multipart - Multipart syntax error.");
#endif
    }

processMultipart_DONE:
    TRACE(" << parsing multipart done with %ld params.\n", params.size());
    if (pd.fd_upload > 0) {
        ::close(pd.fd_upload);
        pd.fd_upload = -1;
    }
    ::close(fd_spool);
    fd_spool = -1;
}
// -------------------------------------------------------------------------------------------------
void
Request::processBodyData()
{
    ParseData pd;

    // Create temporary transfer file for body data
    strcpy(pd.fldname, "[body]");
    strcpy(pd.extfilename, params.get(HASH_CONTENT_TYPE));
    createXferFile(&pd);

    // Store the data
    if (fd_spool == -1) {
        // Store memory spool
        ::write(pd.fd_upload, stdin_buffer, spool_size);
        spool_size = 0;
        close(pd.fd_upload);
        return;
    }
    // Copy the spool file data into xfer file
    char buffer[2048];
    int br;
    // rewind
    if (lseek(fd_spool, 0, SEEK_SET) == -1) {
#ifdef UNIT_TEST
        TRACE("Request::processBodyData - Unable to rewind spool %d. (errno %d)\n", id, errno);
#else
        CS_PRINT_ERRO("Request::processSpool - Unable to rewind spool file");
#endif
        end(500);
        return;
    }
    while (spool_size > 0) {
        br = ::read(fd_spool, buffer, sizeof(buffer));
        if (!br)
            break;
        if (br == -1) {
            TRACE("Request::processSpool - error %d reading stdin spool \n", errno);
            break;
        }
        ::write(pd.fd_upload, buffer, br);
        spool_size -= br;
    }
    spool_size = 0;
    close(pd.fd_upload);
    close(fd_spool);
    fd_spool = -1;
}
// -------------------------------------------------------------------------------------------------
bool
Request::createXferFile(ParseData* pd)
{
    time_t now;
    struct tm* nowtm;
    char datestamp[20];
    static size_t filendx = 1;

    // Create temp file name
    now = time(0);
    nowtm = localtime(&now);
    strftime(datestamp, sizeof(datestamp), "%Y-%m-%d_%H%M", nowtm);
    // Record upload to internal log.
    if (driver && driver->upload_log.good()) {
        driver->upload_log << datestamp << '_' << filendx << '|';
        driver->upload_log << pd->fldname << '|';
        driver->upload_log << pd->extfilename << '\n';
    }
    if (upload_ndx < REQ_MAX_UPLOADS) {
        pd->upfile = new UploadFile();
        uploads[upload_ndx] = pd->upfile;
        strcpy(uploads[upload_ndx]->fldname, pd->fldname);
        if (driver) {
            sprintf(uploads[upload_ndx]->internal, "%supload%s_%ld", driver->upload_path.c_str(),
                    datestamp, filendx++);
        } else {
            sprintf(uploads[upload_ndx]->internal, "upload%s_%ld", datestamp, filendx++);
        }
        strcpy(uploads[upload_ndx]->external, pd->extfilename);
    } else {
#ifdef UNIT_TEST
        TRACE("Request::createXferFile - no space left for upload files.\n");
#else
        CS_PRINT_ERRO("Request::createXferFile - no space left for upload files.");
#endif
        if (driver)
            driver->upload_log << datestamp << " - uploads buffer full.\n";
        return false;
    }

    pd->fd_upload = open(uploads[upload_ndx]->internal, O_WRONLY | O_CREAT | O_TRUNC,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#ifdef UNIT_TEST
    if (pd->fd_upload == -1) {
        TRACE("  Unable to open upload file %s; errno %d\n", uploads[upload_ndx]->internal, errno);
    } else {
        TRACE("  Storing file: field %s - location %s\n", uploads[upload_ndx]->fldname,
              uploads[upload_ndx]->internal);
        upload_ndx++;
    }
#else
    if (pd->fd_upload == -1) {
        CS_VAPRT_ERRO("Request::parseMultipart - Unable to create upload file %s; errno %d",
                      uploads[upload_ndx]->internal, errno);
    } else {
        CS_VAPRT_TRCE("Request::createXferFile - storing file field %s into %s",
                      uploads[upload_ndx]->fldname, uploads[upload_ndx]->internal);
        upload_ndx++;
    }
#endif
    return true;
}

// -------------------------------------------------------------------------------------------------
void
Request::processBeginRequest(uint32_t ndx)
{
    BeginRequestMsg beg_req;

    id = ndx;
    TRACE("Request::processBeginRequest - %d; fd=%d\n", id, pfd.fd);
    rbin.read(&beg_req, sizeof(BeginRequestMsg));
    if ((beg_req.flags & FLAG_KEEP_CONN) > 0)
        flags.set(FLAG_KEEP);
    else
        flags.clear(FLAG_KEEP);

    role = (role_t)beg_req.role.get();
}

// -------------------------------------------------------------------------------------------------
void
Request::parseRequestMethod(NameValue* nv)
{
    char type[9];
    PACK64 pack;

    if (nv->value_len > sizeof(type) - 1) {
        TRACE("Request::parseRequestMethod - length too long:%d\n", nv->value_len);
        rbin.discard(nv->value_len);
        return;
    }
    rbin.read(type, nv->value_len);
    type[nv->value_len] = 0;
    pack.value = 0;
    strncpy(pack.str, type, nv->value_len);
    switch (pack.value) {
    case t64_GET:
        html_type = HTML_GET;
        break;
    case t64_POST:
        html_type = HTML_POST;
        break;
    case t64_PUT:
        html_type = HTML_PUT;
        break;
    case t64_DELETE:
        html_type = HTML_DELETE;
        break;
    case t64_PATCH:
        html_type = HTML_PATCH;
        break;
    default:
        // We should return 501 = not implemented in this case!
#ifdef UNIT_TEST
        TRACE("Request::parseRequestMethod - Unknown request method %s/%jx. Aborted.\n", type,
              pack.value);
#else
        CS_PRINT_WARN("Request::parseRequestMethod - Unknown request method. Aborted.");
#endif
        end(400);
        html_type = HTML_GET;
        return;
    }
    TRACE("Request::parseRequestMethod - method=%d\n", html_type);
}
// -------------------------------------------------------------------------------------------------
/* To save time in comparing parameter names we have pre-calculated 64bit has values for them.
 */
void
Request::processParams(uint64_t* plimit_hash_list, uint16_t msg_len)
{
    bool accepted = true;
    char name_buf[DRIVER_PARAMNAME], *valueptr;
    char content[REQ_MAX_BOUNDARY + 30];
    NameValue nv;
    uint64_t paramhash;
    size_t max;

    if (state != RQS_PARAMS)
        return;
    // Process message.
    uint16_t processed = 0;
    while (processed < msg_len) {
        rbin.peek(&nv, 8);
        nv.init();
        if (nv.name_len + nv.value_len >= rbin.size()) {
            TRACE("Request::process_params - more data needed. name=%d, value=%d, available=%ld\n",
                  nv.name_len, nv.value_len, rbin.size());
            return;
        }
        if (nv.name_len >= DRIVER_PARAMNAME - 1) {
            TRACE("Request::process_params - (Req %d) Name length %d exceeds buffer lenght. "
                  "Discarding this parameter.\n",
                  id, nv.name_len);
            rbin.discard(nv.name_len + nv.value_len + nv.size);
        } else {
            rbin.discard(nv.size);
            rbin.read(name_buf, nv.name_len);
            name_buf[nv.name_len] = 0;
            paramhash = fnv_64bit_hash(name_buf, nv.name_len);
            switch (paramhash) {
            case HASH_REQUEST_METHOD:
                parseRequestMethod(&nv);
                break;

            case HASH_QUERY_STRING:
#ifdef UNIT_TEST
            {
                char value_buf[150];
                memset(value_buf, 0, sizeof(value_buf));
                size_t max =
                    sizeof(value_buf) > nv.value_len ? nv.value_len : sizeof(value_buf) - 1;
                rbin.peek(value_buf, max);
                fwrite(value_buf, 1, max, trace);
                fwrite("\n", 1, 1, trace);
            }
#endif
                // continues on purpose
            case HASH_HTTP_COOKIE:
                if (nv.value_len == 0)
                    break;
                if (paramhash == HASH_HTTP_COOKIE)
                    flags.set(FLAG_COOKIE);
                else
                    flags.set(FLAG_QUERY);
                if (!rbin.push_to(&params, nv.value_len))
                    rbin.discard(nv.value_len);
                break;

            case HASH_CONTENT_TYPE:
                if (nv.value_len < sizeof(content)) {
                    // multipart/form-data; boundary=----------FyyGPOytPwC7HoBRAmGoO0
                    rbin.read(content, nv.value_len);
                    if (!nv.value_len)
                        break;
                    content[nv.value_len] = 0;
                    params.add(HASH_CONTENT_TYPE, content);
                    TRACE("Request::process_params - CONTENT TYPE: %s\n", content);
                    if (!strncmp(content, "multipart/form-data", 19)) {
                        char* boundary_ptr = strstr(content, "boundary=");
                        if (boundary_ptr) {
                            openMPSpool(boundary_ptr + 9,
                                        nv.value_len - (boundary_ptr - content) - 9);
                        } else {
#ifdef UNIT_TEST
                            TRACE("Request::process_params - multipart boundary not found - "
                                  "content:%s\n",
                                  content);
#else
                            CS_PRINT_ERRO(
                                "Request::process_params - multipart boundary not found.");
#endif
                        }
                    } else if (!strncmp(content, "text/markdown;", 14)) {
                        writeSpool(9, "MARKDOWN=");
                    } else if (strncmp(content, "application/x-www-form-urlencoded", 33)) {
                        flags.set(FLAG_BODYDATA);
                    }
                } else {
                    CS_VAPRT_WARN("Request::process_params - content type value too long (%d) "
                                  "for the internal buffer. Ignored.",
                                  nv.value_len);
                    rbin.discard(nv.value_len);
                }
                break;

            case HASH_REQUEST_URI:
                max = nv.value_len >= REQ_MAX_URI ? REQ_MAX_URI - 1 : nv.value_len;
                rbin.read(uri, max);
                uri[max] = 0;
                if (max < nv.value_len) {
                    rbin.discard(nv.value_len - max);
                    CS_VAPRT_WARN(
                        "Request::process_params - URI longer than reserverd space. URI len=%d",
                        nv.value_len);
                }
                break;

            case HASH_MENACON_SID:
                if (nv.value_len == 0)
                    break;
                valueptr = params.add(paramhash, nv.value_len);
                if (valueptr) {
                    rbin.read(valueptr, nv.value_len);
                    valueptr[nv.value_len] = 0;
                    flags.set(FLAG_MENACON_SID);
                    TRACE("Request::process_params - MenaconSID:%s\n", valueptr);
                } else
                    rbin.discard(nv.value_len);
                break;

            default:
                if (plimit_hash_list) {
                    uint64_t* phash;
                    for (phash = plimit_hash_list; *phash; phash++) {
                        if (*phash == paramhash)
                            break;
                    }
                    if (*phash)
                        accepted = true;
                    else {
                        accepted = false;
                        rbin.discard(nv.value_len);
                    }
                }
                if (nv.value_len > 0) {
                    if (accepted) {
                        valueptr = params.add(paramhash, nv.value_len);
                        rbin.read(valueptr, nv.value_len);
                        valueptr[nv.value_len] = 0;
                        TRACE("Request::process_params - param: %s = %s\n", name_buf, valueptr);
                    } else {
                        TRACE("Request::process_params - ignored: %s\n", name_buf);
                    }
                }
                if (!nv.value_len) {
                    TRACE("Request::process_params - Empty parameter value! Name=%s\n", name_buf);
                }
            }
        }
        processed += nv.name_len + nv.value_len + nv.size;
    }
    state = RQS_STDIN;
}

// -------------------------------------------------------------------------------------------------
void
Request::processStdin(uint16_t msg_len)
{
    if (!handler || state != RQS_STDIN) {
        rbin.discard(msg_len);
        return;
    }
    if (msg_len == 0) {
        // Input from server stopped. Do not poll anymore.
        pfd.events &= ~POLLIN;
        if (processSpool()) {
            // Notify the handler associated with this request.
            TRACE("Request::process_stdin (%d) - Calling Done\n", id);
            state = RQS_OPEN;
            handler->done(this);
        }
        return;
    }
    writeSpool(msg_len);
}
// -------------------------------------------------------------------------------------------------
void
Request::abort()
{
    TRACE("Request::abort (%d)\n", id);
    if (handler)
        handler->abort(this);
    end(500);
}

} // namespace fcgi_driver