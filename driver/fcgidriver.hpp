/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_DRIVER_HPP
#define FCGI_DRIVER_HPP

#include <stdint.h>
#include <string>

typedef const char* CC;
inline CC CS(const std::string &s) { return s.c_str(); }

namespace fcgi_driver
{

class Request;
class Driver;

// Max polled file descriptors => max request count.

enum message_type_t
{ TYPE_BEGIN_REQUEST     =  1
  , TYPE_ABORT_REQUEST     =  2
  , TYPE_END_REQUEST       =  3
  , TYPE_PARAMS            =  4
  , TYPE_STDIN             =  5
  , TYPE_STDOUT            =  6
  , TYPE_STDERR            =  7
  , TYPE_DATA              =  8
  , TYPE_GET_VALUES        =  9
  , TYPE_GET_VALUES_RESULT = 10
  , TYPE_UNKNOWN           = 11
};
enum protocol_status_t
{ REQUEST_COMPLETE = 0
  , CANT_MPX_CONN    = 1
  , OVERLOADED       = 2
  , UNKNOWN_ROLE     = 3
};
enum role_t
{ RESPONDER  = 1
  , AUTHORIZER = 2
  , FILTER     = 3
};

enum html_type_t
{ HTML_NONE = 0
  , HTML_GET = 1
  , HTML_POST = 2
  , HTML_PUT = 3
  , HTML_DELETE = 4
  , HTML_PATCH = 5
};
enum req_state_t {
    RQS_WAIT,   // Waiting for new connection
    RQS_PARAMS, // Reading request, first part
    RQS_STDIN,  // Reading request, second part
    RQS_OPEN,   // Serving request
    RQS_FLUSH,  // Flushing buffers
    RQS_END,    // Handler completed response, sending last items.
    RQS_EOF     // All done.
};
enum mp_state_t {
    MP_BEGIN,
    MP_HEADER,
    MP_FLDDATA,
    MP_FILETYPE,
    MP_FILEDATA,
    MP_FINISH
};
union PACK64 {
    char str[8];
    uint64_t value;
};

uint64_t fnv_64bit_hash(CC str, size_t size, uint64_t salt=0L);

bool     isHex(char ch);
void     char2hex(uint8_t ch, char *hex);
uint8_t  hex2byte(CC hex);
uint16_t hex2short(CC hex);
void     trim(char *tgt);
    
} // namespace fcgi_driver

#ifdef UNIT_TEST
  #define TRACE(fmt, ...) fprintf(trace, fmt __VA_OPT__(,) __VA_ARGS__)
#else
  #define TRACE(...) ((void)0)
#endif

#endif