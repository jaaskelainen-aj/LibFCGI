/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Source code inspired and based on original work of Peter Simons and Ralph Babel
 * See https://www.nongnu.org/fastcgi/
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_DRIVERDRIVER_HPP
#define FCGI_DRIVERDRIVER_HPP

#include <map>
#include <queue>
#include <vector>
#include <string>
#include <ostream>
#include <fstream>
#include <fcntl.h>
#include <stdint.h>
#include <poll.h>
#include <string.h>

#include "RingBuffer.hpp"
#include "Request.hpp"

namespace fcgi_driver {

uint8_t const FLAG_KEEP_CONN = 1;

#pragma pack(push, 1)
struct B4Num
{
    uint8_t B3;
    uint8_t B2;
    uint8_t B1;
    uint8_t B0;
    B4Num()
    {
        B3 = 0;
        B2 = 0;
        B1 = 0;
        B0 = 0;
    }
    explicit B4Num(uint32_t val)
    {
        B3 = val >> 24;
        B2 = (val >> 16) & 0xff;
        B1 = (val >> 8) & 0xff;
        B0 = val & 0xff;
    }
    uint32_t get() const { return ((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0; }
};

struct B2Num
{
    uint8_t B1;
    uint8_t B0;
    B2Num()
    {
        B1 = 0;
        B0 = 0;
    }
    explicit B2Num(uint16_t val) { set(val); }
    void set(uint16_t val)
    {
        B1 = val >> 8;
        B0 = val & 0xff;
    }
    uint16_t get() const { return ((uint16_t)B1 << 8) + B0; }
};

struct Header
{
    uint8_t version;
    uint8_t type;
    B2Num request_id;
    B2Num content_length;
    uint8_t padding_length;
    uint8_t reserved;

    Header()
      : version(1)
      , type(TYPE_UNKNOWN)
      , padding_length(0)
      , reserved(0)
    {}
    Header(message_type_t t, uint16_t id)
      : version(1)
      , type(t)
      , request_id(id)
      , padding_length(0)
      , reserved(0)
    {}
    Header(message_type_t t, uint16_t id, uint16_t size)
      : version(1)
      , type(t)
      , request_id(id)
      , content_length(size)
      , padding_length(0)
      , reserved(0)
    {}
};

struct BeginRequestMsg
{
    B2Num role;
    uint8_t flags;
    uint8_t reserved[5];
    BeginRequestMsg()
      : flags(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }
};

struct EndRequestMsg : public Header
{
    B4Num app_status;
    uint8_t protocol_status;
    uint8_t reserved[3];

    EndRequestMsg()
      : protocol_status(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }
    EndRequestMsg(uint16_t id, uint32_t app_status_, protocol_status_t prot_status)
      : Header(TYPE_END_REQUEST, id, sizeof(EndRequestMsg) - sizeof(Header))
      , app_status(app_status_)
      , protocol_status(prot_status)
    {
        memset(this->reserved, 0, sizeof(this->reserved));
    }
};

#pragma pack(pop)

// .................................................................................................
struct NameValue
{
    NameValue();
    void init();
    uint8_t byte[8];
    uint32_t name_len;
    uint32_t value_len;
    size_t size;
};

struct PageArbiter
{
    virtual ~PageArbiter() = default;
    virtual bool matchPage(Request*) = 0;
};

class Driver
{
  public:
    Driver(PageArbiter* arb_, size_t ps, uint32_t reqcount);
    ~Driver();

    void createRequest(pollfd*);
    Request* findRequest(uint32_t fd);
    size_t fillPollFd(pollfd*, size_t max);
    void read(Request*);
    void write(Request* rq) { rq->send(); }
    void work();

    // bool haveActiveRequests();
    void limitParameters(uint64_t* plist) { plimit_hash_list = plist; }
    bool setFileDir(const char* dest_dir);
    void setCacheDir(const char* dest_dir) { cache_path = dest_dir; }

    static void dumpHex(void*, size_t, std::ostream&);
    int getFreeRequestCount();
    void freeDormantRequests();
    uint32_t getServedCount() { return served_count; }
    static const char* version();

  protected:
    friend class Request;

  private:
    // don't copy me
    Driver(Driver const&);
    Driver& operator=(Driver const&);
    // void process_begin_request(Request *req);
    // void process_params(Request*);
    // void process_stdin(Request*);
    // void process_unknown(Request *);
    // void process_multipart(Request *req, uint16_t len);

    Request** requests;
    uint32_t req_count;
    uint32_t served_count; // number of requests handled.
    PageArbiter* arbiter;
    uint64_t* plimit_hash_list;
    std::string upload_path;
    std::string cache_path;
    std::ofstream upload_log;
    struct timespec start_time;
};

} // namespace fcgi_driver

#endif