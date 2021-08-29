/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_ERRORLIST_HPP
#define FCGI_ERRORLIST_HPP

#include <ostream>
#include <sstream>
#include <string.h>

namespace fcgi_frame
{
class AppStr;

class ErrorList
{
public:
    ErrorList(int i) { items=i; errtbl = new uint16_t[items]; clear(); }
    ~ErrorList() { delete[] errtbl; }
    void clear() { memset(errtbl, 0, sizeof(uint16_t)*items); cursor=0; }
    uint16_t first() { cursor=0; return next(); }
    uint16_t next();
    uint16_t& operator[] (const uint16_t ndx);
    void append(uint16_t val);
    uint16_t size();
    void print(std::ostringstream &html, AppStr *appstr, const char *clsname);
    void getText(AppStr *appstr,std::string &buffer);
    void getJSON(AppStr *appstr,std::ostream &);

protected:
    int items;
    int cursor;
    uint16_t *errtbl;
};

} // namespace fcgi_frame

#endif