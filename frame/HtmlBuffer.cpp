/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include "HtmlBuffer.hpp"

namespace fcgi_frame {

HtmlBuffer::HtmlBuffer()
{
    pbuf = html.rdbuf();
}

HtmlBuffer::~HtmlBuffer() {}

void
HtmlBuffer::optionalFlush(fcgi_driver::Request* req)
{
    if (fcgi_driver::Request::getOutCapacity() * 0.80 < (double)html.tellp())
        bufferFlush(req);
}

void
HtmlBuffer::bufferFlush(fcgi_driver::Request* req)
{
    req->setOutPos(html.tellp());
    req->flush();
    html.seekp(0, std::ios_base::beg);
    html.clear();
}

void
HtmlBuffer::bufferClear()
{
    html.seekp(0, std::ios_base::beg);
    html.clear();
}

} // namespace fcgi_frame