/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdexcept>
#include <cpp4scripts.hpp>

#include "../fcgisettings.h"
#include "fcgidriver.hpp"
#include "RingBuffer.hpp"
#include "ParamData.hpp"

extern FILE* trace;

using namespace std;
using namespace c4s;

namespace fcgi_driver {

// -------------------------------------------------------------------------------------------------
ParamData::ParamData(size_t initial_size)
{
    value_buffer = new char[initial_size];
    size_max = initial_size;
    memset(value_buffer, 0, size_max);
    dummy = 0;
    key_ndx = 0;
    clear();
}

// -------------------------------------------------------------------------------------------------
ParamData::~ParamData()
{
    delete[] value_buffer;
}
// -------------------------------------------------------------------------------------------------
void
ParamData::clear()
{
    key_count = 0;
    value_end = value_buffer;
    memset(key_array, 0, sizeof(key_array));
    memset(value_ptr, 0, sizeof(value_ptr));
    callback_state = IDLE;
}
// -------------------------------------------------------------------------------------------------
void
ParamData::resize(size_t add_size)
{
    size_t current = value_end - value_buffer;
    if (current + add_size < size_max)
        return;
    size_max = current + add_size + 1;
    size_max += size_max % 32;
    // if(size_max > FCGIMOD_MAX_PARAMDATA)
    //    throw std::runtime_error("ParamData::set - Max data size exceeded.");
    char* newbuffer = new char[size_max];
    memcpy(newbuffer, value_buffer, current);
    memset(newbuffer + current, 0, size_max - current);
    delete[] value_buffer;
    value_buffer = newbuffer;
    value_end = value_buffer + current;
}
// -------------------------------------------------------------------------------------------------
void
ParamData::init_push(size_t addsize)
{
    callback_state = KEY;
    resize(addsize);
    memset(key_buffer, 0, sizeof(key_buffer));
    key_ndx = 0;
    value_start = value_end;
}
// -------------------------------------------------------------------------------------------------
void
ParamData::push_back(char ch)
{
    switch (callback_state) {
    case KEY:
        if (ch == '=') {
            callback_state = VALUE;
            key_buffer[key_ndx] = 0;
        } else if (key_ndx < DRIVER_PARAMNAME - 1 && ch != ' ') {
            key_buffer[key_ndx] = ch;
            key_ndx++;
        }
        break;
    case VALUE:
        if (ch == ';' || ch == '&') {
            end_push();
            memset(key_buffer, 0, sizeof(key_buffer));
            key_ndx = 0;
            callback_state = KEY;
        } else if (ch == '%') {
            callback_state = HEX;
            hex_ndx = 0;
        } else if (ch == '+')
            *value_end++ = ' ';
        else
            *value_end++ = ch;
        break;
    case HEX:
        hex_buffer[hex_ndx++] = ch;
        if (hex_ndx == 2) {
            hex_buffer[2] = 0;
            *value_end++ = hex2byte(hex_buffer);
            callback_state = VALUE;
        }
        break;
    case IDLE:
        return;
    }
}
// -------------------------------------------------------------------------------------------------
void
ParamData::end_push()
{
    if (callback_state != VALUE) {
        char tbuffer[128];
        sprintf(tbuffer,
                "ParamData::end_push - Key-value-pair not completed. State=%d, key_len=%ld\n",
                callback_state, key_ndx);
        CS_PRINT_ERRO(tbuffer);
        TRACE(tbuffer);
        value_end = value_start;
        callback_state = IDLE;
        throw runtime_error("ParamData::end_push - Syntax error.");
    }
    if (key_count >= DRIVER_PARAMKEYS) {
        CS_PRINT_ERRO("ParamData::end_push - Max key count reached.");
        throw runtime_error("ParamData::end_push - No more room.");
    }
    // Get the key
    uint64_t k64 = fnv_64bit_hash(key_buffer, key_ndx);
    // Finalize the value
    *value_end++ = 0;
    key_array[key_count] = k64;
    value_ptr[key_count] = value_start;
    key_count++;
    TRACE("ParamData::end_push - key=%s; hex=%lx\n", key_buffer, k64);
    TRACE("ParamData::end_push - value=%s\n", value_start);
    value_start = value_end;
    callback_state = IDLE;
}
// -------------------------------------------------------------------------------------------------
uint64_t
ParamData::add(const char* key, size_t keysize, const char* value)
{
    if (!key || !keysize || key_count >= DRIVER_PARAMKEYS)
        return false;
    uint64_t k64 = fnv_64bit_hash(key, keysize);
    return add(k64, value);
}
// -------------------------------------------------------------------------------------------------
char*
ParamData::add(uint64_t hash, size_t valsize)
{
    if (!valsize || key_count >= DRIVER_PARAMKEYS)
        return 0;
    // Reserve space for value
    resize(valsize);
    value_ptr[key_count] = value_end;
    char* rv = value_end;
    value_end += valsize + 1;
    // Store the key
    key_array[key_count] = hash;
    key_count++;
    if (key_count >= DRIVER_PARAMKEYS)
        CS_PRINT_WARN("WARNING: ParamData::add - Max key count reached.");
    return rv;
}
// -------------------------------------------------------------------------------------------------
uint64_t
ParamData::add(uint64_t key, const char* value)
{
    if (!key || !value || key_count >= DRIVER_PARAMKEYS)
        return 0;
    size_t add_size = strlen(value);
    resize(add_size);
    // Copy value
    strcpy(value_end, value);
    value_ptr[key_count] = value_end;
    value_end += add_size + 1;
    // Store the new key
    key_array[key_count] = key;
    key_count++;
    if (key_count >= DRIVER_PARAMKEYS)
        CS_PRINT_WARN("WARNING: ParamData::add - Max key count reached.");
    return key;
}
// -------------------------------------------------------------------------------------------------
const char*
ParamData::get(const char* key, size_t keylen)
{
    if (!key_count || !key || !key[0] || !keylen)
        return &dummy;
    uint64_t hash = fnv_64bit_hash(key, keylen);
    return get(hash);
}
// -------------------------------------------------------------------------------------------------
bool
ParamData::get(const char* key, size_t keylen, int& value)
{
    size_t ndx;
    char* dummy;
    if (!key_count || !key || !key[0] || !keylen)
        return false;
    uint64_t k64 = fnv_64bit_hash(key, keylen);
    for (ndx = 0; ndx < key_count; ndx++) {
        if (key_array[ndx] == k64) {
            value = strtol(value_ptr[ndx], &dummy, 10);
            return true;
        }
    }
    return false;
}
// -------------------------------------------------------------------------------------------------
bool
ParamData::find(const char* key, size_t keylen)
{
    if (!key_count || !key || !key[0] || !keylen)
        return false;
    uint64_t k64 = fnv_64bit_hash(key, keylen);
    for (size_t ndx = 0; ndx < key_count; ndx++) {
        if (key_array[ndx] == k64)
            return true;
    }
    return false;
}
// -------------------------------------------------------------------------------------------------
const char*
ParamData::get(uint64_t k64)
{
    size_t ndx;
    if (!key_count)
        return &dummy;
    for (ndx = 0; ndx < key_count; ndx++) {
        if (key_array[ndx] == k64)
            return value_ptr[ndx];
    }
    return &dummy;
}

#ifdef _DEBUG
// -------------------------------------------------------------------------------------------------
uint64_t
ParamData::getKey(size_t ndx)
{
    if (ndx < key_count)
        return key_array[ndx];
    return 0;
}
// -------------------------------------------------------------------------------------------------
const char*
ParamData::getValue(size_t ndx)
{
    if (ndx < key_count)
        return value_ptr[ndx];
    return &dummy;
}
#endif

// -------------------------------------------------------------------------------------------------
bool
ParamData::findFirst(const char* subkey, size_t* ndx)
{
    PACK64 pack;
    pack.value = 0;
    find_ndx = 0;
    strncpy(pack.str, subkey, 7);
    find_key = pack.value;
#ifdef UNIT_TEST
    fprintf(trace, "ParamData::findFirst - key %lX\n", find_key);
#endif
    return findNext(ndx);
}

bool
ParamData::findNext(size_t* ndx)
{
    if (!ndx)
        return false;
    uint64_t mask = 0xFF;
    for (int x = 0; x < 8; x++) {
        if ((find_key & ~mask) == 0)
            break;
        mask = (mask << 8) | 0xFF;
    }
#ifdef UNIT_TEST
    fprintf(trace, "ParamData::findNext - mask %lX\n", mask);
#endif
    while (find_ndx < key_count) {
        if (((key_array[find_ndx] & mask) ^ find_key) == 0) {
            *ndx = find_ndx;
            find_ndx++;
            return true;
        }
        find_ndx++;
    }
    return false;
}

} // namespace fcgi_driver