/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stdexcept>
#include <string.h>
#include "MultiStr.hpp"

namespace fcgi_frame
{

// -------------------------------------------------------------------------------------------------
MultiStr::MultiStr()
/*! Constructor initializes internal variables to null.*/
{
    length  = 0;
    max_len = 0;
    multi   = 0;
    offsets = 0;
    ndx     = 0;
    token_count = 0;
    reserved_offsets=0;
}
// -------------------------------------------------------------------------------------------------
MultiStr::~MultiStr()
/*! Releases memory reserved for multi string and offset arrays. */
{
    if(multi) delete[] multi;
    if(offsets) delete[] offsets;
}

// -------------------------------------------------------------------------------------------------
MultiStr::MultiStr(const char *orig, const char *token, const char *white)
/*! Initializes internal attributes and then calls set().
  \param orig Original string to be broken into substrings divided by tokens.
  \param token One to more characters that are used as separators.
  \param white Characters that should be considered as whitespace and removed.*/
{
    length  = 0;
    max_len = 0;
    multi   = 0;
    offsets = 0;
    ndx     = 0;
    token_count = 0;
    reserved_offsets=0;
    set(orig,token,white);
}

// -------------------------------------------------------------------------------------------------
void MultiStr::set(const char *orig, const char *token, const char *white)
/*! Writes over possibly existing old string and breaks given new string into sub strings.
  \param orig Original string to be broken into substrings divided by tokens.
  \param token One to more characters that are used as separators.
  \param white Characters that should be considered as whitespace and removed.*/
{
    const char *ptr,*ti,*wi;
    char *back;
    char *tgt;

    // Reserve space for the set
    size_t next_len = strlen(orig)+1;
    if(length) {
        if(next_len>max_len) {
            delete[] multi;
            length = next_len;
            max_len = next_len;
            multi = new char[next_len];
        }
    }
    else {
        length = next_len;
        max_len = next_len;
        multi = new char[next_len];
    }
    memset(multi,0,next_len);

    // Calculate tokens
    token_count=1; // beginning is stored with 0 offset so we start at one.
    for(ptr=orig; *ptr; ptr++) {
        for(ti=token; *ti; ti++) {
            if(*ptr == *ti) {
                token_count++;
                break;
            }
        }
    }
    // Reserve space for the offsets
    if(reserved_offsets) {
        if(token_count>reserved_offsets) {
            delete[] offsets;
            offsets = new const char*[token_count];
            reserved_offsets = token_count;
        }
    }
    else {
        reserved_offsets = token_count;
        offsets = new const char*[token_count];
    }
    memset(offsets,0,reserved_offsets*sizeof(char*));

    enum STATE { BEGIN, WORD };
    STATE state;
    if(white) {
        state = BEGIN;
        ndx = 0;
    }
    else {
        state = WORD;
        offsets[0] = multi;
        ndx=1;
    }

    // Copy and tokenize
    tgt = multi;
    for(ptr=orig; *ptr; ptr++) {
        switch(state) {
        case BEGIN:
            for(wi=white; wi&&*wi; wi++) {
                if(*wi == *ptr) break;
            }
            if(!wi || !*wi) {
                offsets[ndx++] = tgt;
                for(ti=token; ti&&*ti; ti++) {
                    if(*ti == *ptr) {
                        *(tgt++) = 0;
                        break;
                    }
                }
                if(!*ti) {
                    state = WORD;
                    *(tgt++) = *ptr;
                }
            }
            break;
        case WORD:
            for(ti=token; ti&&*ti; ti++) {
                if(*ptr == *ti) {
                    *tgt = 0;
                    // clear trailing whitespace
                    for(back = tgt-1; back>offsets[ndx-1]; back--) {
                        for(wi=white; wi&&*wi; wi++) {
                            if(*wi == *back)
                                break;
                        }
                        if(wi&&*wi) {
                            *back = 0;
                            tgt = back;
                        }
                        else break;
                    }
                    tgt++;
                    state = BEGIN; // white?BEGIN:WORD;
                    break;
                }
            }
            if(!ti || !*ti)
                *(tgt++) = *ptr;
            break;
        }
    }

    token_count = ndx;
    if(state==WORD) {
        for(back = tgt-1; back>offsets[ndx-1]; back--) {
            for(wi=white; wi&&*wi; wi++) {
                if(*wi == *back)
                    break;
            }
            if(wi&&*wi) *back=0;
            else break;
        }
    }
}

// -------------------------------------------------------------------------------------------------
const char* MultiStr::getFirst()
/*! Returns pointer to first substring if any.
   \retval char* first available substring.
*/
{
    ndx=0;
    if(!length)
        return 0;
    return getNext();
}

// -------------------------------------------------------------------------------------------------
const char* MultiStr::getNext()
/*! Returns next substring.
   \retval char* next substring, NULL if nothing left.
*/
{
    if(ndx==token_count)
        return 0;
    return offsets[ndx++];
}
// -------------------------------------------------------------------------------------------------
const char* MultiStr::operator[](size_t idx) const
{
    static char empty[1]= { 0 };
    if(!length) return empty;
    if(idx>=token_count) return empty;
    return offsets[idx];
}

} // namespace fcgi_frame