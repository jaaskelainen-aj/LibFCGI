/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FGCI_REQUEST_HPP
#define FGCI_REQUEST_HPP

#include <map>
#include <queue>
#include <vector>
#include <string>
#include <cstring>
#include <stdint.h>
#include <poll.h>

#include "fcgidriver.hpp"
#include "ParamData.hpp"

namespace fcgi_driver
{

const uint16_t REQ_MAX_FILENAME=96;
const uint16_t REQ_MAX_BOUNDARY=64;
const uint16_t REQ_MAX_URI=255;
const uint16_t REQ_MAX_MEMSTDIN=512;
const uint16_t REQ_MAX_OUT=0xCFFF;
const uint16_t REQ_MAX_UPLOADS=16;
const int REQ_MAX_FLDDATA=0x10000;

class NameValue;

struct UploadFile
{
    UploadFile() {
        memset(fldname, 0, sizeof(fldname));
        memset(internal, 0, sizeof(internal));
        memset(external, 0, sizeof(external));
        bytes = 0;
    }
    char fldname[DRIVER_MPFIELD];
    char internal[REQ_MAX_FILENAME];
    char external[REQ_MAX_FILENAME];
    size_t bytes;
};

struct ParseData {
    ParseData() {
        mp_state     = MP_BEGIN;
        fldptr       = flddata;
        fd_upload    = -1;
        upfile       = 0;
        spool_offset = 0;
    }
    char  fldname[DRIVER_MPFIELD];
    char  extfilename[REQ_MAX_FILENAME];
    // Field data has no limit in specification. Since values are stored in memory we limit them to 65k.
    char  flddata[REQ_MAX_FLDDATA];
    char* fldptr;
    mp_state_t mp_state;
    int        fd_upload;
    UploadFile *upfile;
    size_t     spool_offset;
};

enum flag_t {
    FLAG_KEEP=0x01,
    FLAG_QUERY=0x02,
    FLAG_COOKIE=0x04,
    FLAG_MULTIP=0x08,
    FLAG_SPOOLING=0x10,
    FLAG_BODYDATA=0x20,
    FLAG_MENACON_SID=0x40
};

const uint64_t HASH_REQUEST_METHOD = 0x6fcb71284f57af01UL;
const uint64_t HASH_QUERY_STRING   = 0xc6655389ef49bf38UL;
const uint64_t HASH_HTTP_COOKIE    = 0xaaaaec4758542619UL;
const uint64_t HASH_SCRIPT_FILENAME= 0xa0d212d2a8bdaa5fUL;
const uint64_t HASH_REMOTE_ADDR    = 0x506d3db4bc25ec78UL;
const uint64_t HASH_REQUEST_URI    = 0xe6431050dfbbb8d0UL;
const uint64_t HASH_CONTENT_TYPE   = 0x2ba91afc54dc5636UL;
const uint64_t HASH_USER_AGENT     = 0xb76bea8bf495d108UL;
const uint64_t HASH_MENACON_SID    = 0xd5b46ba8b918e326UL;

enum class HandlerEvent { IDLE, RELOAD };

class Handler
{
public:
    Handler() { }
    virtual ~Handler() { }
    virtual void exec(Request*) = 0;
    virtual void done(Request*) = 0;
    virtual void abort(Request*);
    virtual void event(HandlerEvent);
};

class Request
{
    friend class Driver;
public:
    /* TODO: STDERR messages end up in web server log file. The behaviour should be changed so that
       separate error message can be sent in the middle of regular output.
    */
    // enum ostream_type_t { STDOUT , STDERR };

    Request();
    Request(const Request &orig);
    ~Request();

    void clear();
    uint16_t getId() { return id; }
    uint32_t getFd() { return pfd.fd; }
    html_type_t getType() { return html_type; }
    req_state_t getState() { return state; }
    bool is(flag_t ft) { return flags.is(ft); }
    bool isRead() { return state == RQS_PARAMS || state == RQS_STDIN ? true : false; }
    bool isWrite() { return (pfd.events & POLLOUT) > 0 ? true:false; }
    void write(const char *, uint16_t len=0);
    bool writeFd(int fd);
    void flush(); // !USE SPARINGLY, BLOCKS THE SCHEDULER
    void end(uint32_t appStatus=0);
    void setStatus(uint32_t as) { app_status = as; }
    const char* getURI() { return uri; }

    int getUploadCount() { return upload_ndx; }
    UploadFile* getFirstUpload() {
        upload_ndx = 1;
        return uploads[0];
    }
    UploadFile* getNextUpload() {
        if(upload_ndx<REQ_MAX_UPLOADS && uploads[upload_ndx]) {
            UploadFile *uf = uploads[upload_ndx];
            upload_ndx++;
            return uf;
        }
        return 0;
    }

    size_t getOutReserved() { return rbpos - rbout; }
    size_t getOutPending() { return rbpos - rbsend; }
    static uint16_t getOutCapacity() { return REQ_MAX_OUT; }
    char* getOutBuffer() { return rbout; }               // Use ONLY with std::ostringstream !!
    void  setOutPos(size_t pos) { rbpos = rbout + pos; }

    void log(const char *str);
    void processTestSpool();

    RingBuffer rbin;      // Request data buffer
    ParamData  params;    // Request parameters
    Handler*   handler;   // Pointer to application handler
    void*      app_data;  // Used by application to store additional data

protected:
    void operator=(const Request &) { clear(); }
    void setPollFd(pollfd *fd);

    void processBeginRequest(uint32_t ndx);
    bool openMPSpool(const char *data, int len);
    void writeSpool(uint16_t msg_len, const char *msg=0);
    bool processSpool();
    void processStdin(uint16_t msg_len);
    size_t parseMultipart(char *data, size_t dlen, ParseData *pd);
    void processMultipart();
    void processBodyData();
    void processParams(uint64_t *hash_list, uint16_t msg_len);
    bool createXferFile(ParseData *);
    void clearRbOut() { rbpos=rbout; rbsend=rbout; }
    void abort();

    void send();
    void parseRequestMethod(NameValue *);

#ifdef UNIT_TEST
    bool openMPSpool(const char *fname, const char *mptag, int taglen);
#endif
    class Flags
    {
    public:
        Flags() { bits=0; }
        bool is(flag_t s) { return (bits&s)>0?true:false; }
        bool is(int s) { return (bits&s)==s?true:false; }
        void set(flag_t s) { bits |= s; }
        void set(int s) { bits |= s; }
        void clear(flag_t s) { bits &= ~s; }
        void clear() { bits=0; }
    private:
        int bits;
    } flags;

    html_type_t html_type;
    req_state_t state;
    pollfd   pfd;
    uint32_t id;
    role_t   role;
    char     rbout[REQ_MAX_OUT];          // output buffer
    char*    rbpos;                       // Current output position (for write-function);
    char*    rbsend;                      // Current output position (for send)
    char     boundary[REQ_MAX_BOUNDARY];  // Stores the multipart formdata separator.
    char     uri[REQ_MAX_URI];
    char     stdin_buffer[REQ_MAX_MEMSTDIN];
    uint16_t bound_len;                   // NUmber of actual bytes in boundary.
    uint16_t stdin_len;                   // Bytes in stdin part
    uint32_t app_status;
    size_t   spool_size;
    int      fd_spool;                    // Used by spooler and uploader in processing multipart forms
    int      mp_count;                    // Multi-part count
    uint32_t stdout_count;                // Number of times the rbout has been sent / single request
    UploadFile* uploads[REQ_MAX_UPLOADS]; // Request uploads.
    int      upload_ndx;                  // Index of next upload.
    static Driver *driver;
    static size_t input_size, param_size;
};

} // namespace fcgi_driver

#endif