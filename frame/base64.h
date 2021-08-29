/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_BASE64_H
#define FCGI_BASE64_H

namespace fcgi_frame
{

enum class Base64Type { URL, SMTP };

size_t base64_size(size_t len);
size_t base64_fit(size_t len);
size_t base64_encode(char *target, size_t t_len, const unsigned char* bytes_to_encode, size_t in_len, Base64Type bt);
size_t base64_decode(unsigned char *target, size_t t_len, const char* encoded_string, Base64Type bt);

} // namespace fcgi_frame


#endif