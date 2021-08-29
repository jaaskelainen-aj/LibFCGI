/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_SESSIONBASE_HPP
#define FCGI_SESSIONBASE_HPP

#include "SessionMgr.hpp"
#include "AppStr.hpp"

namespace fcgi_frame
{

class SessionBase
{
public:
    SessionBase();
    SessionBase(const SessionBase&);
    virtual ~SessionBase();

    void clear(); // => kplogin=0;
    unsigned long getAge();
    uint64_t getKey() { return pack.value; }
    void catSID(char *buffer) { memcpy(buffer,sid,FRAME_SID+1); }
    const char *getHandlerName();
    virtual bool save(FILE*);
    virtual bool load(FILE*);
    virtual int getGroupCount() { return 0; }
    virtual int getGroup(int) { return 0; }

    bool hasRights(uint32_t query) { return (query&sb_rights)==query?true:false; }
    bool anyRights(uint32_t query) { return (query&sb_rights)>0?true:false; }
    uint32_t getRights() { return sb_rights; }
    const bool isLogin() { return sb_rights>0?true:false; }

    void setSID() { memcpy(sid, sm->getSID(), FRAME_SID); sid[FRAME_SID]=0; }
    bool isSID() { return sid[0] ? true:false; } // && strncmp(SID_VOID,base->sid,7)
    const char *getSID() { return sid; }

    const char* getCookieSID();
    const char* getCookieLang();
    const char* getCookieLogin();
    const char* dropCookieSID();
    const char* dropCookieLang();

    APPSTR_LC getLocale() { return appstr->getLCode(); }
    const char* getLanguage() { return appstr->getLName(); }
    void dump(int sysl_facility);

    AppStr *appstr;
    int locale_ndx;
    bool old;
    fcgi_driver::PACK64 pack;

protected:
    uint32_t sb_rights;

private:
    friend class SessionMgr;
    friend class Framework;
    bool refresh(unsigned long max_sec);
    void clearSID() { memset(sid,0,sizeof(sid)); }

    char   sid[FRAME_SID+1];
    time_t start;
    static SessionMgr *sm;
    static const char *SID_VOID;
    static char login_cookie[64];
};

} // namespace fcgi_frame

#endif