/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stdexcept>
#include <cstring>
#include "MultiStr.hpp"

using namespace std;

namespace fcgi_frame {

// -------------------------------------------------------------------------------------------------
void
MultiStr::clean()
{
    length = 0;
    ndx = 0;
    token_count = 0;
    memset(offsets, 0, sizeof(offsets));

}
// -------------------------------------------------------------------------------------------------
bool
MultiStr::set(const char* orig, const char* token, const char* white)
/*! Writes over possibly existing old string and breaks given new string into sub strings.
  \param orig Original string to be broken into substrings divided by tokens.
  \param token One to more characters that are used as separators.
  \param white Characters that should be considered as whitespace and removed.*/
{
    const char *ptr, *ti, *wi;
    char* back;

    enum STATE
    {
        BEGIN,
        WORD
    };

    STATE state;
    if (white) {
        state = BEGIN;
        ndx = 0;
    } else {
        state = WORD;
        offsets[0] = multi;
        ndx = 1;
    }

    // Copy and tokenize
    char* tgt = multi;
    for (ptr = orig; *ptr; ptr++) {
        switch (state) {
        case BEGIN:
            for (wi = white; wi && *wi; wi++) {
                if (*wi == *ptr)
                    break;
            }
            if (!wi || !*wi) {
                offsets[ndx++] = tgt;
                for (ti = token; ti && *ti; ti++) {
                    if (*ti == *ptr) {
                        *(tgt++) = 0;
                        break;
                    }
                }
                if (!*ti) {
                    state = WORD;
                    if (tgt - multi >= MAX_MS_LEN-1) {
                        clean();
                        return false;
                    }
                    *(tgt++) = *ptr;
                }
            }
            break;
        case WORD:
            for (ti = token; ti && *ti; ti++) {
                if (*ptr == *ti) {
                    *tgt = 0;
                    // clear trailing whitespace
                    for (back = tgt - 1; back > offsets[ndx - 1]; back--) {
                        for (wi = white; wi && *wi; wi++) {
                            if (*wi == *back)
                                break;
                        }
                        if (wi && *wi) {
                            *back = 0;
                            tgt = back;
                        } else
                            break;
                    }
                    tgt++;
                    state = BEGIN; // white?BEGIN:WORD;
                    break;
                }
            }
            if (!ti || !*ti) {
                if (tgt - multi >= MAX_MS_LEN-1) {
                    clean();
                    return false;
                }
                *(tgt++) = *ptr;
            }
            break;
        }
    }
    *tgt = 0;

    token_count = ndx;
    if (state == WORD) {
        for (back = tgt - 1; back > offsets[ndx - 1]; back--) {
            for (wi = white; wi && *wi; wi++) {
                if (*wi == *back)
                    break;
            }
            if (wi && *wi)
                *back = 0;
            else
                break;
        }
        length = back-multi;
    } else
        length = tgt-multi;
    return true;
}

// -------------------------------------------------------------------------------------------------
const char*
MultiStr::getFirst()
/*! Returns pointer to first substring if any.
   \retval char* first available substring.
*/
{
    ndx = 0;
    if (!length)
        return 0;
    return getNext();
}

// -------------------------------------------------------------------------------------------------
const char*
MultiStr::getNext()
/*! Returns next substring.
   \retval char* next substring, NULL if nothing left.
*/
{
    if (ndx == token_count)
        return 0;
    return offsets[ndx++];
}
// -------------------------------------------------------------------------------------------------
const char* MultiStr::operator[](size_t idx) const
{
    static char empty[1] = { 0 };
    if (!length)
        return multi;
    if (idx >= token_count) 
        return empty;
    return offsets[idx];
}

} // namespace fcgi_frame