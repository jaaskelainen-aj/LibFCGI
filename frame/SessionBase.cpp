/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <time.h>
#include <stdio.h>
#include <syslog.h>
#include <string>
#include <fstream>

#include "Framework.hpp"
#include "SessionBase.hpp"

using namespace std;

namespace fcgi_frame
{

static char g_cookie[128];
SessionMgr* SessionBase::sm=0;
const char *SessionBase::SID_VOID = "deleted";
char SessionBase::login_cookie[64];

// -------------------------------------------------------------------------------------------------
SessionBase::SessionBase()
{
    memset(sid,0,sizeof(sid));
    appstr     = 0;
    start      = time(0);
    pack.value   = 0;
    locale_ndx = 0;
    old        = false;
    sb_rights      = 0;
}
// -------------------------------------------------------------------------------------------------
SessionBase::~SessionBase()
{
    // Nothing to do.
}
// -------------------------------------------------------------------------------------------------
SessionBase::SessionBase(const SessionBase &orig)
{
    appstr = orig.appstr;
    strncpy(sid,orig.sid,sizeof(sid));
    sid[FRAME_SID]=0;
    start = orig.start;
    pack.value = orig.pack.value;
    old = orig.old;
    sb_rights = orig.sb_rights;
}
// -------------------------------------------------------------------------------------------------
bool SessionBase::refresh(unsigned long max_sec)
{
    time_t now = time(0);
    if(max_sec>0 && now-start > (time_t)max_sec) {
        int facility = Framework::get()->getSesMgr()->getFacility();
        if(facility)
            syslog(LOG_MAKEPRI(facility, LOG_NOTICE),
                   "SessionBase::refresh - timout - now:%ld; start=%ld; diff=%ld; max=%ld\n",
                   now,start,now-start,max_sec);
        old = true;
        sb_rights = 0;
        return false;
    }
    start = now;
    return true;
}
// -------------------------------------------------------------------------------------------------
void SessionBase::clear()
{
    sb_rights = 0;
}
// -------------------------------------------------------------------------------------------------
void SessionBase::dump(int facility)
{
    sid[FRAME_SID]=0;

    syslog(LOG_MAKEPRI(facility, LOG_NOTICE),
           "Session : SID = %s; key = %lx; sb_rights = %d; start = %ld; old = %s\n",
           sid,pack.value,sb_rights,start,(old?"true":"false"));
}
// -------------------------------------------------------------------------------------------------
unsigned long SessionBase::getAge()
{
    return (unsigned long) (time(0) - start);
}
// -------------------------------------------------------------------------------------------------
bool SessionBase::save(FILE *sf)
{
    fwrite(&start,sizeof(time_t), 1, sf);
    fwrite(&sb_rights,sizeof(uint32_t), 1, sf);
    return true;
}
// -------------------------------------------------------------------------------------------------
bool SessionBase::load(FILE *sf)
{
    fread(&start,sizeof(time_t), 1, sf);
    fread(&sb_rights,sizeof(uint32_t), 1, sf);
    return true;
}
// -------------------------------------------------------------------------------------------------
const char *SessionBase::getHandlerName()
{
    static char name[9];
    memcpy(name,pack.str,8);
    name[8]=0;
    return name;
}
// -------------------------------------------------------------------------------------------------
const char* SessionBase::getCookieSID()
{
    sprintf(g_cookie, "Set-Cookie: sid=%s; SameSite=Strict\r\n",sid);
    return g_cookie;
}
const char* SessionBase::getCookieLang()
{
    const char *lang = appstr->getLName();
    sprintf(g_cookie, "Set-Cookie: lang=%c%c; Expires=Thu, 31-Dec-2019 00:00:00 GMT; SameSite=Strict\r\n",lang[0], lang[1]);
    return g_cookie;
}
const char* SessionBase::getCookieLogin()
{
    if(sb_rights)
        sprintf(g_cookie, "Set-Cookie: %s=%d; SameSite=Strict\r\n", login_cookie, sb_rights);
    else
        sprintf(g_cookie, "Set-Cookie: %s=0; Max-Age=0; SameSite=Strict\r\n", login_cookie);
    return g_cookie;
}
const char* SessionBase::dropCookieSID()
{
    strcpy(g_cookie, "Set-Cookie: sid=deleted; Max-Age=0; SameSite=Strict\r\n"); // Expires=Thu, 01 Jan 1970 00:00:00 GMT
    return g_cookie;
}
const char* SessionBase::dropCookieLang()
{
    strcpy(g_cookie, "Set-Cookie: lang=**; Max-Age=0; SameSite=Strict\r\n");
    return g_cookie;
}
// -------------------------------------------------------------------------------------------------
} // namespace fcgi_frame