/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

#include <cpp4scripts.hpp>

#include "Framework.hpp"
#include "AppStr.hpp"
#include "MultiStr.hpp"
#include "base64.h"

using namespace std;
using namespace c4s;

namespace fcgi_frame {

// -------------------------------------------------------------------------------------------------
SessionMgr::SessionMgr(const string& exe_dir,
                       const c4s::path& keyfile,
                       SessionFactoryIF* SIF,
                       c4s::configuration* conf)
{
    string loc_list, session_dir, session_path;
    uint64_t val;

    sesfactory = SIF;
    key_ndx = 0;
    memset(strings, 0, sizeof(strings));
    memset(key_buf, 0, sizeof(key_buf));
    def_lang = 0;

    randfile = keyfile.get_path();

    if (!conf->get_value("LibFCGI", "SessionMaxAge", val)) {
        CONF_max_age = 30 * 60;
        syslog(LOG_WARNING,
               "SessionMgr::SessionMgr - Missing config option 'LibFCGI / SessionMaxAge'. Using default %ld", CONF_max_age);
    } else {
        CONF_max_age = val;
    }

    if (!conf->get_value("LibFCGI", "SessionDir", session_dir)) {
        session_dir = "sessions/";
        syslog(LOG_WARNING, "SessionMgr::SessionMgr - Missing config option 'LibFCGI / SessionDir'");
    }
    if (conf->get_value("LibFCGI", "SessionLogFacility", val)) {
        if (val >= 0 && val <= 7)
            CONF_facility = LOG_LOCAL0 + (val << 3);
        else {
            CONF_facility = LOG_LOCAL4;
            syslog(LOG_WARNING,
                   "SessionMgr::SessionMgr - Unknown session syslog facility. Using LOCAL4");
        }
    } else {
#ifdef _DEBUG
        syslog(LOG_NOTICE, "SessionMgr::SessionMgr - Session logging disabled.");
#endif
        CONF_facility = 0;
    }

    session_path = exe_dir;
    session_path += session_dir;
    session_path += "session_key_offset";
    CONF_session_dir.set(session_path);

    loadKeys();

    // Optional: If locales option is not found the language files will not be loaded.
    if (!conf->get_value("LibFCGI", "Locales", loc_list)) {
        syslog(LOG_WARNING, "SessionMgr::SessionMgr - Missing config option 'LibFCGI / Locales'");
    } else {
        if (!loc_list.empty())
            locales = new MultiStr(loc_list.c_str(), ",", " ");
        else {
            syslog(LOG_WARNING, "SessionMgr::SessionMgr - 'Web / Locales' is empty!");
            locales = 0;
        }
    }
    if (locales) {
        bool first = true;
        int ndx = 0;
        for (const char* locid = locales->getFirst(); locid && ndx < FRAME_LOCALES;
             locid = locales->getNext()) {
            c4s::path locpath(exe_dir, locid, ".utf8");
            syslog(LOG_INFO, "SessionMgr::SessionMgr - Loading locales from %s", locpath.get_pp());
            strings[ndx] = new AppStr(locpath.get_pp(), locid);
            if (first) {
                // Default language is the first language string set.
                def_lang = strings[ndx];
                first = false;
            }
            ndx++;
        }
        syslog(LOG_INFO, "SessionMgr::SessionMgr - def_lang %s\n", def_lang ? "OK" : "False");
    }

    // Form hash values for often used parameter searches.
    hash_sid   = fcgi_driver::fnv_64bit_hash("sid", 3);
    hash_page  = fcgi_driver::fnv_64bit_hash("pg", 2);
    hash_fn    = fcgi_driver::fnv_64bit_hash("fn", 2);
    hash_lang  = fcgi_driver::fnv_64bit_hash("lang", 4);    
}

// -------------------------------------------------------------------------------------------------
SessionMgr::~SessionMgr()
{
    purge();

    for (int ndx = 0; ndx < FRAME_LOCALES; ndx++) {
        if (strings[ndx])
            delete strings[ndx];
    }
    if (locales)
        delete locales;
}
// -------------------------------------------------------------------------------------------------
const char*
SessionMgr::getSID()
{
    struct
    {
        time_t tt;
        uint64_t key;
    } pack;

    if (key_ndx >= FRAME_KEYS)
        loadKeys();
    pack.tt = time(0);
    memcpy(&pack.key, key_buf + key_ndx, FRAME_KEY_SIZE);
    key_ndx += FRAME_KEY_SIZE;
    base64_encode(sid_buf, FRAME_SID, (const unsigned char*)&pack, sizeof(pack), Base64Type::URL);
    sid_buf[FRAME_SID] = 0;
    if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
               "SessionMgr::getSID - key_ndx:%d time:%ld SID:%s\n", key_ndx, pack.tt, sid_buf);
    return sid_buf;
}
// -------------------------------------------------------------------------------------------------
SessionBase*
SessionMgr::initializeSession(fcgi_driver::Request* req)
{
    SessionBase* base = 0;
    CC sid = 0;

    if (req->is(fcgi_driver::FLAG_COOKIE)) {
        sid = req->params.get(hash_sid);
        if (CONF_facility)
            syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                   "SessionMgr::initializeSession - Cookie sid:%s\n", sid);
    } else if (req->is(fcgi_driver::FLAG_LIBFCGI_SID)) {
        sid = req->params.get(fcgi_driver::HASH_LIBFCGI_SID);
        if (CONF_facility)
            syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                   "SessionMgr::initializeSession - Header sid:%s\n", sid);
    }
    if (!sid || !sid[0]) {
        base = sesfactory->createSession(0);
        base->setSID();
        if (CONF_facility) {
            syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                   "SessionMgr::initializeSession - No cookies persent. New sid:%s\n", base->sid);
        }
        CC client_dn = req->params.get(fcgi_driver::HASH_SSL_CLIENT_DN);
        if(client_dn[0]) {
            sesfactory->loginClientCert(base, client_dn);
        }
    } else {
        CONF_session_dir.set_base(sid);
        CONF_session_dir.set_ext(".ses");
        FILE* sfile = fopen(CONF_session_dir.get_path().c_str(), "rb");
        if (sfile) {
            base = sesfactory->createSession(sfile);
            strncpy(base->sid, sid, FRAME_SID);
            base->sid[FRAME_SID] = 0;
            req->app_data = base;
            fclose(sfile);
            // Check if we are too old
            time_t now = time(0);
            if (CONF_facility)
                syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                       "SessionMgr::initializeSession - Session age:%fh\n",
                       (now - base->start) / 60.0 / 60.0);
            if (now - base->start > CONF_max_age) {
                base->old = true;
                sesfactory->logoutSession(req);
                rmSessionFile(sid);
                base->setSID();
                if (CONF_facility)
                    syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                           "SessionMgr::initializeSession - session expired. New sid: %s\n",
                           base->sid);
            }
            base->start = now;
        } else {
            base = sesfactory->createSession(0);
            base->setSID();
            if (CONF_facility)
                syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                       "SessionMgr::initializeSession - File not found: %s - new: %s\n", sid,
                       base->sid);
        }
    }
    req->app_data = base;
    // ....................................................
    // Parse often used parameters into session base.
    //
    base->page.value = 0;
    base->fn.value = 0;
    CC page = req->params.get(hash_page);
    CC fn = req->params.get(hash_fn);
    if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_DEBUG),
               "SessionMgr::initializeSession - page: %s; fn: %s\n", page, fn);
    strncpy(base->page.str, page, 8);
    strncpy(base->fn.str, fn, 8);

    // CS_VAPRT_DEBU("Framework::initializeRequest - page key = %lx",base->pack.key);
    const char* lang = req->params.get(hash_lang);
    if (lang[0])
        setLanguage(req, lang, false);
    else {
        base->appstr = def_lang;
        base->locale_ndx = 0;
    }

    return base;
}

// -------------------------------------------------------------------------------------------------
void
SessionMgr::closeSession(fcgi_driver::Request* req)
{
    SessionBase* base = (SessionBase*)req->app_data;
    if (base->isSID()) {
        CONF_session_dir.set_base(base->sid);
        CONF_session_dir.set_ext(".ses");
        FILE* sfile = fopen(CONF_session_dir.get_path().c_str(), "wb");
        if (!sfile && base->isSID()) {
            if (CONF_facility)
                syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                       "SessionMgr::closeSession - Unable to store session with sid %s\n",
                       base->sid);
            CS_VAPRT_ERRO("SessionMgr::closeSession - Unable to store session with sid %s",
                          base->sid);
        } else {
            sesfactory->saveSession(sfile, req);
            fclose(sfile);
        }
    }
    if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE), "SessionMgr::closeSession - time %ld\n",
               base->start);
    delete base;
    req->app_data = 0;
}
// -------------------------------------------------------------------------------------------------
int
SessionMgr::login(fcgi_driver::Request* req)
{
    int rv = sesfactory->loginSession(req);
    if (CONF_facility) {
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE), "SessionMgr::login = %d\n", rv);
        SessionBase* base = (SessionBase*)req->app_data;
        base->dump(CONF_facility);
    }
    return rv;
}
// -------------------------------------------------------------------------------------------------
void
SessionMgr::logout(fcgi_driver::Request* req)
{
    SessionBase* base = (SessionBase*)req->app_data;
    if (base->isSID()) {
        rmSessionFile(base->sid);
        base->setSID();
    } else if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE), "SessionMgr::logout - No SID \n");
    base->sb_rights = 0; // Should be cleared by factory but done here just in case.
    sesfactory->logoutSession(req);
}
// -------------------------------------------------------------------------------------------------
void
SessionMgr::setLanguage(fcgi_driver::Request* req, const char* lang, bool set_session)
{
    int ndx;
    APPSTR_LC lcode = AppStr::lcname2code(lang);
    SessionBase* base = (SessionBase*)req->app_data;
    if (lcode == APPSTR_NONE) {
        CS_VAPRT_WARN("SessionMgr::setLanguage - Language:%s not supported.", lang);
        base->appstr = def_lang;
        base->locale_ndx = 0;
        return;
    }
    // Search for the locale
    for (ndx = 0; ndx < FRAME_LOCALES; ndx++) {
        if (strings[ndx] && strings[ndx]->getLCode() == lcode) {
            base->appstr = strings[ndx];
            base->locale_ndx = ndx;
            break;
        }
    }
    if (ndx == FRAME_LOCALES) {
        CS_VAPRT_WARN("SessionMgr::setLanguage - Language code %d not found from this system.",
                      lcode);
        base->appstr = def_lang;
        base->locale_ndx = 0;
    }
}
// -------------------------------------------------------------------------------------------------
void
SessionMgr::loadKeys()
{
    int* iptr;
    unsigned int count, ndx, cs;
    bool ses_count_exists = false;
    ssize_t rb;

    int rands = open(randfile.c_str(), O_RDONLY);
    if (rands == -1)
        goto RAND_KEYS;

    ses_count_exists = CONF_session_dir.exists();
    cs = open(CONF_session_dir.get_path().c_str(), O_CREAT | O_RDWR,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (cs > 0) {
        read(cs, &count, sizeof(int));
        if (!ses_count_exists)
            fchmod(cs, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    } else {
        CS_VAPRT_WARN("SessionMgr::loadKeys - unable to open key count file %s. Errno %d",
                      CONF_session_dir.get_path().c_str(), errno);
        count = 0;
    }

    lseek(rands, count * FRAME_KEY_SIZE, SEEK_SET);
    rb = read(rands, (char*)key_buf, FRAME_KEYS);
    if ((size_t)rb < FRAME_KEYS) {
        ssize_t left = FRAME_KEYS - rb;
        lseek(rands, 0, SEEK_SET);
        rb = read(rands, (char*)(key_buf + rb), left);
        if (left < rb) {
            if (CONF_facility)
                syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                       "SessionMgr::loadKeys - New keys loaded.\n");
            CS_VAPRT_WARN("Key file rewind did not work properly:%d", count);
            close(rands);
            close(cs);
            goto RAND_KEYS;
        }
        count = left / FRAME_KEY_SIZE;
    } else
        count += FRAME_KEYS / FRAME_KEY_SIZE;
    close(rands);

    lseek(cs, 0, SEEK_SET);
    write(cs, &count, sizeof(int));
    close(cs);

    key_ndx = 0;
    if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE), "SessionMgr::loadKeys - New keys loaded.\n");
    CS_PRINT_NOTE("SessionMgr::loadKeys - New keys loaded.");
    return;

RAND_KEYS:
    if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
               "SessionMgr::loadKeys - Generating random keys.\n");
    iptr = (int*)key_buf;
    srand(time(0));
    for (ndx = 0; ndx < sizeof(key_buf) / sizeof(int); ndx++) {
        iptr[ndx] = rand();
    }
    key_ndx = 0;
    CS_VAPRT_WARN("SessionMgr::loadKeys - Unable to open random file (%d). Using rand().", errno);
}
// -------------------------------------------------------------------------------------------------
void
SessionMgr::purge()
{
    int fd;
    time_t create;
    ssize_t rv;

    c4s::path_list sessions(c4s::path(CONF_session_dir), "\\.ses$");
    if (sessions.size() == 0)
        return;
    if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
               "SessionMgr::purge - checking %ld session files.\n", sessions.size());
    time_t now = time(0);
    for (c4s::path_iterator sf = sessions.begin(); sf != sessions.end(); sf++) {
        fd = open(sf->get_path().c_str(), O_RDONLY);
        if (fd == -1) {
            CS_VAPRT_ERRO("SessionMgr::purge - Cannot open session file %s",
                          sf->get_path().c_str());
            continue;
        }
        rv = read(fd, &create, sizeof(time_t));
        close(fd);
        if (rv > 0) {
            if (now - create > CONF_max_age) {
                if (CONF_facility)
                    syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                           "Purging stale session file %s\n", sf->get_base().c_str());
                sf->rm();
            }
        }
    }
}
// -------------------------------------------------------------------------------------------------
void
SessionMgr::rmSessionFile(const char* sid)
{
    if (CONF_facility)
        syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE), "SessionMgr::rmSessionFile - SID:%s\n", sid);
    CONF_session_dir.set_base(sid);
    CONF_session_dir.set_ext(".ses");
    if (CONF_session_dir.rm() == false) {
        CS_VAPRT_WARN("SessionMgr::rmSessionFile - Unable to remove session file with sid:%s", sid);
        if (CONF_facility)
            syslog(LOG_MAKEPRI(CONF_facility, LOG_NOTICE),
                   "SessionMgr::rmSessionFile - remove failed");
    }
}

} // namespace fcgi_frame