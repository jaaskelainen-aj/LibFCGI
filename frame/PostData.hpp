/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_POSTDATA_HPP
#define FCGI_POSTDATA_HPP

#include "../fcgisettings.h"

namespace fcgi_frame {

class PostData
{
  public:
    PostData();
    ~PostData();
    void set(const char* data);
    const char* get(const char* key);
    const char* get(int);
    void add(const char* key, const char* value);
    // void writeAsCookies(fcgi_driver::Request *req);
    size_t size() { return count; }

  protected:
    enum STATE
    {
        KEY,
        VALUE
    };
    char* value_buffer;
    size_t vb_len;
    uint64_t key_array[FRAME_POSTKEYS];
    const char* value_array[FRAME_POSTKEYS];
    size_t count;
    char dummy;
};

} // namespace fcgi_frame

#endif