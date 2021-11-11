/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_PARAMDATA_HPP
#define FCGI_PARAMDATA_HPP

/* Special parameters
 * MARKDOWN = for markdown body data. Value = markdown text.
 */

#include "../fcgisettings.h"
#include "RingBuffer.hpp"

namespace fcgi_driver {

class ParamData : public RBCallBack
{
  public:
    ParamData(size_t initial_size);
    ~ParamData();
    // Adding parameters into the list.
    uint64_t add(const char* key, size_t keysize, const char* value);
    char* add(uint64_t hash, size_t valsize);
    uint64_t add(uint64_t key, const char* value);
    // From RBCallBack
    void init_push(size_t);
    void push_back(char ch);
    void end_push();
    bool isCallbackOn() { return callback_state == IDLE ? false : true; }
    // Get functions
    const char* get(const char* key, size_t keylen);
    const char* get(uint64_t);
    bool get(const char* key, size_t keylen, int& value);
    bool find(const char* key, size_t keylen);
    bool findFirst(const char* subkey, size_t* ndx);
    bool findNext(size_t* ndx);

#ifdef _DEBUG
    uint64_t getKey(size_t ndx);
    const char* getValue(size_t ndx);
#else
    uint64_t getKey(size_t ndx)
    {
        if (ndx < key_count)
            return key_array[ndx];
        return 0;
    }
    const char* getValue(size_t ndx)
    {
        if (ndx < key_count)
            return value_ptr[ndx];
        return &dummy;
    }
#endif
    size_t getUsedBytes() { return value_end - value_buffer; }
    size_t getMax() { return size_max; }
    size_t size() { return key_count; }
    void clear();

  protected:
    void resize(size_t add_size);

    char* value_buffer; //!< Always points to beginning of value buffer.
    char* value_end;    //!< Points to a place where new value can be added to
    size_t size_max;    //!< Size of the current value buffer.
    uint64_t key_array[DRIVER_PARAMKEYS];
    const char* value_ptr[DRIVER_PARAMKEYS];
    size_t key_count;
    char dummy;

    // Needed in RBCallBack functionality
    enum STATE
    {
        IDLE,
        KEY,
        VALUE,
        HEX
    };
    STATE callback_state;
    char key_buffer[DRIVER_PARAMNAME];
    char hex_buffer[3];
    size_t key_ndx, hex_ndx;
    size_t find_ndx;
    uint64_t find_key;
    char* value_start;
};

} // namespace fcgi_driver

#endif