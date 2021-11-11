/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stdint.h>
#include <string.h>
#include <stdexcept>

#include "../driver/fcgidriver.hpp"
#include "PostData.hpp"

using namespace fcgi_driver;

namespace fcgi_frame {

// -------------------------------------------------------------------------------------------------
PostData::PostData()
{
    value_buffer = 0;
    vb_len = 0;
    count = 0;
    dummy = 0;
    memset(key_array, 0, sizeof(key_array));
    memset(value_array, 0, sizeof(value_array));
}
// -------------------------------------------------------------------------------------------------
PostData::~PostData()
{
    if (value_buffer)
        delete[] value_buffer;
}
// -------------------------------------------------------------------------------------------------
void
PostData::set(const char* data)
{
    STATE state = KEY;
    PACK64 pack;
    int ch_ndx;
    size_t dlen;
    if (data) {
        dlen = strlen(data);
        if (dlen > FRAME_POSTDATA)
            throw std::runtime_error("PostData::set - Max data size exceeded.");
        // Reserve memory if needed.
        // \TODO: some memory would be saved if we take into account that keys are removed.
        if (dlen > vb_len) {
            if (value_buffer)
                delete[] value_buffer;
            value_buffer = new char[dlen];
            vb_len = dlen;
        }
    }

    // Initialize buffers and pointers
    memset(value_buffer, 0, vb_len);
    memset(key_array, 0, sizeof(key_array));
    memset(value_array, 0, sizeof(value_array));
    count = 0;
    if (!data)
        return;
    char* valptr = value_buffer;
    char* preval = value_buffer;
    pack.value = 0;
    count = 0;
    ch_ndx = 0;

    // Process the data
    const char* ptr = data;
    while (ptr < data + dlen + 1) {
        switch (state) {
        case KEY:
            if (*ptr == '=') {
                while (ch_ndx < 8)
                    pack.str[ch_ndx++] = 0;
                state = VALUE;
            }
            if (ch_ndx < 8)
                pack.str[ch_ndx++] = *ptr;
            break;
        case VALUE:
            if (*ptr == 0 || *ptr == ';' || *ptr == '&') {
                key_array[count] = pack.value;
                value_array[count] = preval;
                preval = valptr + 1;
                count++;
                state = KEY;
                ch_ndx = 0;
                if (count == FRAME_POSTKEYS)
                    throw std::runtime_error("PostData::set - Max key count exceeded.");
            } else if (*ptr == '%') {
                *valptr = (char)hex2byte(ptr + 1);
                ptr += 2;
            } else if (*ptr == '+')
                *valptr = ' ';
            else
                *valptr = *ptr;
            valptr++;
            break;
        }
        ptr++;
    }
}
// -------------------------------------------------------------------------------------------------
const char*
PostData::get(const char* key)
{
    size_t ndx;
    PACK64 pack;
    if (!count)
        return &dummy;
    for (ndx = 0; ndx < 8; ndx++) {
        if (*key)
            pack.str[ndx] = *key++;
        else
            pack.str[ndx] = 0;
    }
    for (ndx = 0; ndx < count; ndx++) {
        if (key_array[ndx] == pack.value)
            return value_array[ndx];
    }
    return &dummy;
}
// -------------------------------------------------------------------------------------------------
const char*
PostData::get(int ndx)
{
    if (ndx < 0 || ndx >= FRAME_POSTKEYS)
        return &dummy;
    return value_array[ndx];
}
// -------------------------------------------------------------------------------------------------
void
PostData::add(const char* key, const char* value)
{
    size_t ndx;
    PACK64 pack;
    // Check if we have room for this new value
    if (count + 1 == FRAME_POSTKEYS)
        throw std::runtime_error("PostData::add - Max key count exceeded.");
    size_t vlen = strlen(value);
    size_t cursize = strlen(value_array[count - 1]) + (value_array[count - 1] - value_buffer);
    if (vlen + cursize > FRAME_POSTDATA)
        throw std::runtime_error("PostData::add - Max data size exceeded.");
    // If not, reserve more and swap buffers.
    if (vlen + cursize > vb_len) {
        vb_len = vlen + cursize + 20;
        char* newbuffer = new char[vb_len];
        memset(newbuffer, 0, vb_len);
        memcpy(newbuffer, value_buffer, cursize);
        delete[] value_buffer;
        value_buffer = newbuffer;
    }
    // Copy value
    char* end = value_buffer + cursize + 1;
    strcpy(end, value);
    value_array[count] = end;
    // Make the key
    for (ndx = 0; ndx < 8; ndx++) {
        if (*key)
            pack.str[ndx] = *key++;
        else
            pack.str[ndx] = 0;
    }
    // Search if this key already exists.
    for (ndx = 0; ndx < count; ndx++) {
        if (key_array[ndx] == pack.value)
            break;
    }
    // If it did: remove the earlier key.
    if (ndx < count)
        key_array[ndx] = 0;
    // Store the new key
    key_array[count] = pack.value;
    count++;
}

} // namespace fcgi_frame