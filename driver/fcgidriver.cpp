/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stddef.h>
#include <cstring>

#include "../fcgisettings.h"
#include "fcgidriver.hpp"

namespace fcgi_driver
{

// -------------------------------------------------------------------------------------------------
// http://www.isthe.com/chongo/src/fnv/hash_64.c
uint64_t fnv_64bit_hash(CC str, size_t len, uint64_t salt)
{
    union {
        char ch[8];
        uint64_t key;
    } pack;
    uint64_t hash = salt;
    unsigned char *s = (unsigned char *)str;
    if(!str || !len)
        return 0;
    if(len<9) {
        for(size_t ndx=0; ndx<8; ndx++){
            if(ndx<len) pack.ch[ndx]=str[ndx];
            else pack.ch[ndx]=0;
        }
        return pack.key;
    }
    while (len) {
        hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) + (hash << 8) + (hash << 40);
        hash ^= (uint64_t)*s++;
        len--;
    }
    return hash;
}
// -------------------------------------------------------------------------------------------------
bool isHex(char ch)
{
    if(ch>='0' && ch<='9') return true;
	ch &= 0xDF;
    if(ch>='A' && ch<='F') return true;
    if(ch=='X') return true;
    return false;
}
// -------------------------------------------------------------------------------------------------
void char2hex(uint8_t ch, char *hex)
{
    uint8_t hc = 'a';
    uint8_t byte = (ch&0xf0)>>4;
    hex[0] = byte>9 ? byte+hc-10 : byte+'0';
    byte = ch&0x0f;
    hex[1] = byte>9 ? byte+hc-10 : byte+'0';
}
// -------------------------------------------------------------------------------------------------
uint8_t hex2byte(CC hex)
{
    uint8_t ch, result=0;
    if(!hex)
        return 0;
    ch = *hex|0x20;
    if(ch>='a' && ch<='f') ch=ch-'a'+10;
    else if(ch>='0' && ch<='9') ch=ch-'0';
    else return 0;
    result = 0xf&ch;
    result<<=4;
    ch = *(hex+1)|0x20;
    if(ch>='a' && ch<='f') ch=ch-'a'+10;
    else if(ch>='0' && ch<='9') ch=ch-'0';
    else return 0;
    result |= 0xf&ch;
    return result;
}
// -------------------------------------------------------------------------------------------------
uint16_t hex2short(CC hex)
{
    unsigned char ch;
    uint16_t result=0,ndx=0;
    if(!hex)
        return 0;
    while(*hex) {
        if(*hex>='0' && *hex<='9')
            ch=*hex-'0';
        else {
            ch = *hex&0xDF;
            if(ch>='A' && ch<='F')
                ch=*hex-'A'+10;
            else if(ch=='X' && ndx==1) {
                hex++; ndx=0;
                continue;
            }
            else if(ndx==0 && (ch==' ' || ch=='\t')) {
                hex++;
                continue;
            }
            else return result;
        }
        if(ndx)
            result<<=4;
        result |= 0xf&ch;
        hex++;
        ndx++;
    }
    return result;
}
// -------------------------------------------------------------------------------------------------
void trim(char *tgt)
{
    char *end = tgt+strlen(tgt)-1;
    while( (*end==' ' || *end=='\n' || *end=='\r' || *end=='\t') && end!=tgt)
        *end--=0;
}

} // namespace fcgi_driver