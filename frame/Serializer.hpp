/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_SERIALIZER_HPP
#define FCGI_SERIALIZER_HPP

#include "TmDatetime.hpp"

namespace fcgi_frame
{
class SessionBase;

class Serializer
{
public:
    enum TYPE { INPUT, OUTPUT };
    Serializer(SessionBase *, std::string&, TYPE);
    ~Serializer() { if(file_id) close(file_id); };

    void read(int &val) { ::read(file_id, &val, sizeof(int)); }
    void read(std::string&);
    void read(TmDatetime &val) { ::read(file_id, &val.dtv, sizeof(struct tm)); }
    void read(bool &val);

    void write(const int &val) { ::write(file_id, &val, sizeof(int)); }
    void write(const std::string &);
    void write(const TmDatetime &val) { ::write(file_id, &val.dtv, sizeof(struct tm)); }
    void write(bool val);

private:
    int file_id;
};

} // namespace fcgi_frame

#endif