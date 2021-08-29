/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Source code inspired and based on original work of Peter Simons and Ralph Babel
 * See https://www.nongnu.org/fastcgi/
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <cstring>
#include <cstdio>
#include <cassert>
#include <cpp4scripts.hpp>

#include "../fcgisettings.h"
#include "Driver.hpp"
#include "Request.hpp"

FILE *trace=0;

using namespace std;
using namespace c4s;

namespace fcgi_driver
{

const char *VERSION = "FCGI Driver v 2.7.0";
const char* Driver::version()
{
    return VERSION;
}

// -------------------------------------------------------------------------------------------------
NameValue::NameValue() { memset(byte,0,sizeof(byte)); name_len=0; value_len=0; size=0; }

void NameValue::init()
{
    size=0;
    name_len=0;
    value_len=0;
    if(byte[0] >> 7 == 0) {
        name_len = byte[0];
        size++;
    }else{
        name_len |= (byte[0]&0x7f)<<24;
        name_len |= byte[1]<<16;
        name_len |= byte[2]<<8;
        name_len |= byte[3];
        size+=4;
    }
    if(byte[size] >> 7 == 0) {
        value_len = byte[size];
        size++;
    }else{
        value_len |= (byte[size++]&0x7f)<<24;
        value_len |= byte[size++]<<16;
        value_len |= byte[size++]<<8;
        value_len |= byte[size++];
    }
}

//void NameValue::dump(std::ostream &os)
//{
//    os << "DEBUG: NameValue - bytes:"<<std::ostream::hex;
//    for(int ndx=0; ndx<8; ndx++)
//        os << " 0x"<<(unsigned short)byte[ndx];
//    os << std::ostream::dec<<"; name="<<name_len<<"; value="<<value_len<<"; size="<<size<<"; total="<<name_len+value_len+size<<'\n';
//}

// -------------------------------------------------------------------------------------------------
Driver::Driver(PageArbiter *_arb, size_t paramsize, uint32_t _req_count)
    : arbiter(_arb)
{
    plimit_hash_list = 0;
    Request::input_size=0x11000; // Currently this is forced to high enough value.
    Request::param_size=paramsize;
    Request::driver = this;
    req_count = _req_count > DRIVER_POLL_FD ? DRIVER_POLL_FD : _req_count;
    requests = new Request*[req_count];
    for(uint32_t ndx=0; ndx<req_count; ndx++)
        requests[ndx]=new Request();
    served_count = 0;
    clock_gettime(CLOCK_REALTIME, &start_time);
#ifdef UNIT_TEST
    char trname[28];
    sprintf(trname,"trace_%d.log", getpid());
    trace = fopen(trname,"a");
    //trace = fopen("trace_driver.log","a");
    time_t now = time(0);
    struct tm *local = localtime(&now);
    fprintf(trace,"=== New driver started %02d-%02d-%02d %02d:%02d:%02d\n",
            local->tm_year+1900, local->tm_mon+1, local->tm_mday,
            local->tm_hour, local->tm_min, local->tm_sec);
#endif
}
// -------------------------------------------------------------------------------------------------
Driver::~Driver()
{
    for(uint32_t ndx=0; ndx<req_count; ndx++)
        delete requests[ndx];
    delete[] requests;
    if(upload_log.is_open())
        upload_log.close();
#ifdef UNIT_TEST
    if(trace)
        fclose(trace);
#endif
}
// -------------------------------------------------------------------------------------------------
bool Driver::setFileDir(const char *dest_dir)
/*! We need to have a place to store the uploaded files.
  \param dest_dir Full path to destination directory.
 */
{
    // Ensure that we have slash ad the end.
    upload_path = dest_dir;
    std::string::reverse_iterator last = upload_path.rbegin();
    if(*last != '/')
        upload_path += '/';
    if(upload_path.size() > REQ_MAX_URI-REQ_MAX_FILENAME) {
        upload_path = "./";
        CS_PRINT_ERRO("Driver::setFileDir - upload path too long.");
        return false;
    }
    std::string logname(upload_path);
    logname += "fcgi-upload.log";
    // We test the write permissions by opening upload log into this dir.
    upload_log.open(logname.c_str(), std::ios::out|std::ios::app);
    return upload_log.good();
}
// -------------------------------------------------------------------------------------------------
int Driver::getFreeRequestCount()
{
    int count=0;
    uint32_t ndx;
    for(ndx=0; ndx<req_count; ndx++) {
        if(requests[ndx]->getState() == RQS_WAIT) {
            count++;
        }
    }
    return count;
}
// -------------------------------------------------------------------------------------------------
void Driver::createRequest(pollfd *newfd)
{
    uint32_t ndx;
    // Check whether we have open request with this fd.
    for(ndx=0; ndx<req_count; ndx++) {
        if(requests[ndx]->getState() == RQS_WAIT) {
            requests[ndx]->setPollFd(newfd); // => RQS_PARAMS
#ifdef UNIT_TEST
            char tbuf[128];
            time_t now = time(0);
            struct tm *tm = localtime(&now);
            strftime(tbuf, sizeof(tbuf), "--\nDriver::createRequest - %F %T\n", tm);
            fputs(tbuf, trace);
#endif
            return;
        }
    }
    TRACE("Driver::createRequest - Out of requests (%d / %d)!\n", ndx, req_count);
    CS_PRINT_CRIT("Driver::createRequest - Out of requests!!");
}
// -------------------------------------------------------------------------------------------------
Request* Driver::findRequest(uint32_t fd)
{
    // Get the request for this FD
    uint32_t ndx;
    for(ndx=0; ndx<req_count; ndx++) {
        if(requests[ndx]->getFd() == fd)
            break;
    }
    if(ndx==req_count) {
        // Request was either already closed or aborted but remains in Schedulers list
        TRACE("Driver::findRequest - fd=%d not found.\n",fd);
        return 0;
    }
    return requests[ndx];
}
// -------------------------------------------------------------------------------------------------
size_t Driver::fillPollFd(pollfd *pfd, size_t max)
{
    size_t px=0;
    for(uint32_t ndx=0; ndx<req_count && ndx<max; ndx++) {
        if(requests[ndx]->isRead() || requests[ndx]->isWrite() ) {
            memcpy(&pfd[px++], &requests[ndx]->pfd, sizeof(pollfd));
        }
    }
    return px;
}

// -------------------------------------------------------------------------------------------------
void Driver::dumpHex(void *data, size_t len, std::ostream &stm)
{
    union {
        char ch;
        uint16_t val;
    }pack;
    pack.val = 0;
    stm<<std::hex;
    for(size_t ndx=0; ndx<len; ndx++) {
        pack.ch = *((char*)data);
        stm << pack.val;
    }
    stm<<std::dec;
}

// -------------------------------------------------------------------------------------------------
// Returns true when at whole message has been read in.
void Driver::read(Request *req)
{
    static char copybuf[0x11000];
    ssize_t rb;
    size_t wb;

    // Read the request fd
    size_t rbcap = req->rbin.capacity();
    ssize_t max = (ssize_t) (rbcap < sizeof(copybuf) ? rbcap:sizeof(copybuf));
    rb = ::read(req->getFd(),copybuf,max);
    if(rb==-1) {
        if(errno != EAGAIN) {
            TRACE("Driver::read %d - read error: %s",req->getFd(), strerror(errno));
        }
        return;
    }
    if(!rb)
        return;
    wb = req->rbin.write(copybuf,rb);
    if(wb!=(size_t)rb) {
        TRACE("Driver::read(%d) - unable to store required bytes: rbcap=%ld; rb=%ld; wb=%ld",req->getFd(),rbcap,rb,wb);
        req->end(500);
    }
    TRACE("Driver::read(%d) - raw data %ld bytes\n", req->getFd(), rb);
}

// -------------------------------------------------------------------------------------------------
void Driver::work()
{
    Header hp;
    uint32_t msg_total;
    uint16_t msg_len;

    // for requests
    for(uint32_t ndx=0; ndx<req_count; ndx++) {
        // We must be in reading mode
        if(requests[ndx]->state != RQS_PARAMS &&
           requests[ndx]->state != RQS_STDIN)
            continue;
        // Bail out if we do not have the header yet, read some more.
        if(requests[ndx]->rbin.size() < sizeof(Header))
            continue;
        // Peek the header and check it
        requests[ndx]->rbin.peek(&hp, sizeof(Header));
        if (hp.version != 1) {
            TRACE("Driver::work(%d) - Warning: unsupported protocol version %d\n",requests[ndx]->getFd(), hp.version);
            requests[ndx]->end(501);
            continue;
        }
        msg_total = sizeof(Header) + hp.content_length.get() + hp.padding_length;
        msg_len   = hp.content_length.get();
        requests[ndx]->id = hp.request_id.get();
        TRACE("driver::read - header version=%d; type=%d; id=%d; padding=%d; length=%d; rbin.size=%ld\n",
              hp.version, hp.type, requests[ndx]->id, hp.padding_length, msg_len, requests[ndx]->rbin.size());
        if(msg_total > requests[ndx]->rbin.size()) {
            continue; // Message data is not completely in yet. Wait for some more.
        }
        // Process the message.
        try {
            requests[ndx]->rbin.discard(sizeof(Header));
            switch (hp.type) {
            case TYPE_BEGIN_REQUEST:
                requests[ndx]->processBeginRequest(served_count);
                served_count++;
                break;

            case TYPE_ABORT_REQUEST:
                requests[ndx]->abort();
                break;

            case TYPE_PARAMS:
                if(msg_len == 0) {
                    TRACE("Driver::work(%d) - calling exec\n",requests[ndx]->getFd() );
                    arbiter->matchPage(requests[ndx]);
                    if(requests[ndx]->handler)
                        requests[ndx]->handler->exec(requests[ndx]);
                    else {
                        TRACE("Request::process_params - Arbiter was not able to find handler for this request.\n");
                        requests[ndx]->end(400);
                    }
                }
                requests[ndx]->processParams(plimit_hash_list, msg_len);
                break;

            case TYPE_DATA:
                TRACE("Driver::work - Req type DATA not supported by responder. Ignored. fd=%d\n", requests[ndx]->getFd() );
                requests[ndx]->rbin.discard(msg_len);
                break;

            case TYPE_STDIN:
                requests[ndx]->processStdin(msg_len);
                break;

            default:
                TRACE("Driver::work(%d) - unknown package of type:%d\n",
                      requests[ndx]->getFd(), hp.type);
                requests[ndx]->rbin.discard(msg_len);
            }
            if(hp.padding_length) {
                requests[ndx]->rbin.discard(hp.padding_length);
            }
        }
        catch(const std::runtime_error &re) {
            TRACE("driver::work - runtime exception: %s\n", re.what() );
        }
        catch(...){
            TRACE("driver::work(%d) - unknown exception\n", requests[ndx]->getFd() );
        }
    }
}
// -------------------------------------------------------------------------------------------------
void Driver::freeDormantRequests()
{
    uint32_t count=0, closed=0;
    uint32_t ndx;
    size_t left;
    pollfd pfdarray[DRIVER_POLL_FD];
    Request *eof_reqs[DRIVER_POLL_FD];

    // Collect all requests that could be closed
    for(ndx=0; ndx<req_count; ndx++) {
        if(requests[ndx]->getState() == RQS_EOF) {
            left = requests[ndx]->getOutPending();
            if(left>0) {
                TRACE("Driver::freeDormantRequests - At eof and %ld bytes pending.\n", left);
            }
            pfdarray[count].fd = requests[ndx]->pfd.fd;
            pfdarray[count].events = POLLOUT;
            pfdarray[count].revents = 0;
            eof_reqs[count] = requests[ndx];
            count++;
        }
    }
    // check if we are ready to perform output => all bytes sent. Can be closed.
    if(!count)
        return;
    int rc = poll(pfdarray, count, 0); // timeout 0 =>  return immediately.
    if (rc == -1) {
        TRACE("Driver::freeDormantRequests - poll failed:%s.\n", strerror(errno));
        return;
    }
    for(ndx=0; ndx<count; ndx++) {
        if( (pfdarray[ndx].revents & POLLOUT) > 0) {
            eof_reqs[ndx]->setPollFd(0);
            closed++;
        }
    }
    if(count) {
        TRACE("Driver::freeDormantRequests - at eof %d, closed %d\n", count, closed);
    }
}

} // namespace fcgi_driver