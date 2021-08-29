/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */

#ifndef FCGI_HTMLBUFFER_HPP
#define FCGI_HTMLBUFFER_HPP

#include <sstream>
#include <iostream>

#include "../driver/Driver.hpp"
#include "../driver/Request.hpp"

namespace fcgi_frame
{

class HtmlBuffer
{
public:
    HtmlBuffer();
    virtual ~HtmlBuffer();
    void bufferCatch(fcgi_driver::Request *req) { pbuf->pubsetbuf( req->getOutBuffer(), req->getOutCapacity()); }
    void bufferRelease(fcgi_driver::Request *req) { req->setOutPos(html.tellp()); }

    // Flush if we are over 80% capacity
    void optionalFlush(fcgi_driver::Request *req);
    // Forced flush. (must before Includer::toRequest)
    void bufferFlush(fcgi_driver::Request *req);
    // Clear buffer from previous output. Note headers are cleared as well!
    void bufferClear();

    std::ostringstream html;
protected:
    std::stringbuf *pbuf;
};

} // namespace fcgi_frame

#endif