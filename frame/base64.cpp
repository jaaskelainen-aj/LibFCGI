/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * From http://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
*/

#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "base64.h"

const char url64_table[] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_"
};
const char smtp64_table[] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/"
};
size_t base64_mod_table[] = {0, 2, 1};

namespace fcgi_frame
{

inline bool is_base64(unsigned char c)
{
  return (isalnum(c) || (c == '-') || (c == '_'));
}
// -------------------------------------------------------------------------------------------------
size_t base64_size(size_t len)
{
    size_t max=len*4/3;
    return max+(4-max%4);
}
// -------------------------------------------------------------------------------------------------
size_t base64_fit(size_t len)
{
    size_t fit=len*3/4;
    return fit-(4-fit%4);
}
// -------------------------------------------------------------------------------------------------
size_t base64_encode(char *target, size_t t_len, const unsigned char *data,
                            size_t in_len, Base64Type bt)
{
    size_t i=0, j=0, max;
    uint32_t octet_a, octet_b, octet_c;
    uint32_t triple;

    max = base64_size(in_len);
    if(max > t_len)
        return 0;

    const char *encode_table;
    char filler;
    if(bt == Base64Type::URL) {
        encode_table = url64_table;
        filler = '~';
    } else {
        encode_table = smtp64_table;
        filler = '=';
    };
    while( i < in_len ) {

        octet_a = i < in_len ? (unsigned char)data[i++] : 0;
        octet_b = i < in_len ? (unsigned char)data[i++] : 0;
        octet_c = i < in_len ? (unsigned char)data[i++] : 0;

        triple = (octet_a << 0x10) | (octet_b << 0x08) | octet_c;

        target[j++] = encode_table[(triple >> 3 * 6) & 0x3F];
        target[j++] = encode_table[(triple >> 2 * 6) & 0x3F];
        target[j++] = encode_table[(triple >> 1 * 6) & 0x3F];
        target[j++] = encode_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < base64_mod_table[in_len % 3]; i++)
        target[max-1-i] = filler;
    target[max]=0;

    return max;
}
// -------------------------------------------------------------------------------------------------
size_t base64_decode(unsigned char *target, size_t t_len, const char *data,
                            Base64Type bt)
{
    size_t i=0, j=0;
    uint32_t sextet_a, sextet_b, sextet_c, sextet_d, triple;
    static char decode_table[256] = { 0 };

    const char *encode_table;
    char filler;
    if(bt == Base64Type::URL) {
        encode_table = url64_table;
        filler = '~';
    } else {
        encode_table = smtp64_table;
        filler = '=';
    };

    if( decode_table[0] == 0 ) {
        memset(decode_table, 0, sizeof(decode_table));
        for (int i = 0; i < 64; i++)
            decode_table[(unsigned char) encode_table[i]] = i;
    }

    size_t input_length = strlen(data);
    if (input_length % 4 != 0)
        return 0;

    size_t output_length = input_length / 4 * 3;
    if (data[input_length - 1] == filler) output_length--;
    if (data[input_length - 2] == filler) output_length--;
    if (output_length > t_len)
        return 0;

    while (data[i]) {
        sextet_a = data[i] == filler ? 0 & i++ : decode_table[(int)data[i++]];
        sextet_b = data[i] == filler ? 0 & i++ : decode_table[(int)data[i++]];
        sextet_c = data[i] == filler ? 0 & i++ : decode_table[(int)data[i++]];
        sextet_d = data[i] == filler ? 0 & i++ : decode_table[(int)data[i++]];

        triple = (sextet_a << 3 * 6) | (sextet_b << 2 * 6) | (sextet_c << 1 * 6) | (sextet_d << 0 * 6);

        if (j < output_length) target[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < output_length) target[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < output_length) target[j++] = (triple >> 0 * 8) & 0xFF;
    }
    target[output_length]=0;
    return output_length;
}

} // namespace fcgi_frame