/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_SESSIONMGR_HPP
#define FCGI_SESSIONMGR_HPP

#include <stdio.h>
#include <cpp4scripts.hpp>
#include "../driver/Request.hpp"

namespace fcgi_frame
{
class AppStr;
class MultiStr;
class SessionBase;

class SessionFactoryIF
{
public:
    virtual ~SessionFactoryIF() { }
    virtual SessionBase* createSession(FILE *session_file)=0;
    virtual int loginSession (fcgi_driver::Request *req)=0;
    virtual void logoutSession(fcgi_driver::Request *req)=0;
    virtual void saveSession(FILE *session_file, fcgi_driver::Request *req)=0;
};

class SessionMgr
{
public:
    SessionMgr(const std::string &exe_dir, const c4s::path &keyfile, SessionFactoryIF *, c4s::configuration *conf);
    ~SessionMgr();

    SessionBase* initializeSession(fcgi_driver::Request *req);
    const char* getSID();
    void closeSession(fcgi_driver::Request *req);
    int login(fcgi_driver::Request *req);
    void logout(fcgi_driver::Request *req);
    void setLanguage(fcgi_driver::Request *req, const char *lc, bool set_session);
    void purge();
    long getMaxAge() { return CONF_max_age; }
    long getFacility() { return CONF_facility; }

    AppStr* getDefaultLanguage() { return def_lang; }
    MultiStr *locales;

protected:
    SessionBase* find(const char *SID);
    void rmSessionFile(const char *SID);
    void loadKeys();

    unsigned char key_buf[FRAME_KEYS]{};
    unsigned int key_ndx;
    char sid_buf[FRAME_SID+1]{};

    std::string randfile;
    long CONF_max_age;
    long CONF_facility;
    c4s::path CONF_session_dir;

    SessionFactoryIF *sesfactory;
    AppStr *strings[FRAME_LOCALES]{};
    AppStr *def_lang;
};

} // namespace fcgi_frame

#endif