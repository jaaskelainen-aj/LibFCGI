/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <pwd.h>
#include <sys/stat.h>

#include <cpp4scripts.hpp>
#include <stdexcept>

#include "../fcgisettings.h"
#include "../driver/fcgidriver.hpp"
#include "Framework.hpp"

//- extern c4s::configuration conf;
//- extern int WEB_VERNUM;
//- extern int app_daemon;

using namespace std;
using namespace c4s;

namespace fcgi_frame {

Framework* Framework::the_frame = 0;
const char* Framework::fmget_error = "Framework not initialized.";
char* Framework::esc_buffer = 0;
size_t Framework::esc_len = 512;

// -------------------------------------------------------------------------------------------------
HandlerElement::HandlerElement(const char* name, fcgi_driver::Handler* _handler)
{
    key = fcgi_driver::fnv_64bit_hash(name, strlen(name));
    handler = _handler;
    //    CS_VAPRT_DEBU("HandlerElement::HandlerElement - %s = %lx", name, key);
}
// -------------------------------------------------------------------------------------------------
void
Framework::create(const std::string& exe_dir, SessionFactoryIF* sf, c4s::configuration* cf)
{
    if (!the_frame)
        the_frame = new Framework(exe_dir, sf, cf);
    if (!esc_buffer)
        esc_buffer = new char[esc_len];
}
// -------------------------------------------------------------------------------------------------
void
Framework::close()
{
    if (the_frame)
        delete the_frame;
    if (esc_buffer)
        delete[] esc_buffer;
    the_frame = 0;
    esc_buffer = 0;
}
// -------------------------------------------------------------------------------------------------
Framework::Framework(const std::string& home_dir, SessionFactoryIF* sf, c4s::configuration* conf)
  : conf(conf)
{
    string session_keys, hk;
    c4s::path uploadp;
    std::string tmpval;

    if (!sf || !conf)
        throw runtime_error(
            "Framework::Framework - SessionFactory and configuration are required parameters.");
    // Read the compulsory options
    if (!conf->get_value("LibFCGI", "PageName", page_name))
        throw runtime_error("Framework::Framework - Missing PageName from options.");
    if (!conf->get_value("LibFCGI", "SessionKeys", session_keys))
        throw runtime_error("Framework::Framework - Missing SessionKeys from options");

    // Handling other options
    if (!conf->get_value("LibFCGI", "DirUpload", upload_dir)) {
        upload_dir = "upload/";
        syslog(LOG_DEBUG,
               "Framework::Framework - Missing DirUpload from configuration. Using default value.");
    }
    if (upload_dir[0] == '/')
        uploadp.set(upload_dir);
    else {
        uploadp.set(home_dir);
        uploadp += upload_dir;
    }
    if (!uploadp.dirname_exists()) {
        syslog(LOG_DEBUG, "Framework::Framework - Creating missing upload directory.");
        uploadp.mkdir();
    }

    if (conf->get_value("LibFCGI", "LoginCookie", tmpval)) {
        strncpy(SessionBase::login_cookie, tmpval.c_str(), sizeof(SessionBase::login_cookie));
        SessionBase::login_cookie[sizeof(SessionBase::login_cookie) - 1] = 0;
    } else
        strcpy(SessionBase::login_cookie, "webapp_login");
    // ..................................................
    // Start initializations
    try {
        includer = new Includer(home_dir, conf);
        c4s::path sk_file(session_keys);
        if (!sk_file.exists())
            throw runtime_error("Framework::Framework - Session key file does not exists.");
        sesmgr = new SessionMgr(home_dir, sk_file, sf, conf);
        SessionBase::sm = sesmgr;
    } catch (const runtime_error& re) {
        syslog(LOG_CRIT, "Framework::Framework - initialization failure: %s.", re.what());
        throw;
    }
}
// -------------------------------------------------------------------------------------------------
Framework::~Framework()
{
    if (includer)
        delete includer;
    if (sesmgr)
        delete sesmgr;
}
// -------------------------------------------------------------------------------------------------
SessionBase*
Framework::initializeRequest(fcgi_driver::Request* req)
{
    // Load or create session
    SessionBase* base = sesmgr->initializeSession(req);
    if (!base)
        throw runtime_error(
            "Framework::initializeRequest - SessionBase cannot be null after initialization.");
    return base;
}
// -------------------------------------------------------------------------------------------------
void
Framework::beginRequest(fcgi_driver::Request* req,
                        std::ostringstream& html,
                        const char* type,
                        ostringstream* cookies,
                        ostringstream* headers)
{
    // NOTE! PageHandler should have called bufferCatch(req) before this function.
    SessionBase* base = (SessionBase*)req->app_data;
    if (!type)
        html << "Content-type: text/html\r\n";
    else if (!strcmp(type, "redirect") && cookies) {
        html << "Content-type: text/html\r\n";
        if (base)
            html << base->getCookieLogin();
        html << "Location: " << cookies->str() << "\r\n\r\n";
        html << "<html><head><title>Transfer</title></head><body><p>Redirecting...</p></body></"
                "html>\r\n";
        return;
    } else {
        html << "Content-type: " << type << "\r\n";
    }
    if (headers && headers->tellp() > 0)
        html << headers->str();
    if (base->isSID()) {
        CC cookie;
        if (base->old)
            cookie = base->dropCookieSID();
        else
            cookie = base->getCookieSID();
        html << cookie;
        int facility = sesmgr->getFacility();
        if (facility)
            syslog(LOG_MAKEPRI(facility, LOG_NOTICE), "Framework::beginRequest - %s", cookie);
    }
    html << base->getCookieLogin();
    if (cookies && cookies->tellp() > 0)
        html << cookies->str();
    html << "\r\n";
}
// -------------------------------------------------------------------------------------------------
void
Framework::closeRequest(fcgi_driver::Request* req, uint32_t http_status)
{
    sesmgr->closeSession(req);
    req->end(http_status);
}
// -------------------------------------------------------------------------------------------------
void
Framework::encodeURL(const char* origin, char* target, size_t tmax)
{
    uint8_t unsafe[15] = { 0x20, 0x22, 0x3c, 0x3e, 0x23, 0x25, 0x7b, 0x7d,
                           0x7c, 0x5c, 0x5e, 0x7e, 0x5b, 0x5d, 0x60 };

    size_t count = 0;
    int ndx;
    while (*origin) {
        for (ndx = 0; ndx < 15; ndx++) {
            if (*origin == (char)unsafe[ndx])
                break;
        }
        if (ndx < 15) {
            if (count + 3 >= tmax) {
                *target = 0;
                return;
            }
            *target = '%';
            fcgi_driver::char2hex(unsafe[ndx], target + 1);
            target += 3;
            count += 3;
        } else {
            if (count + 1 >= tmax) {
                *target = 0;
                return;
            }
            *target = *origin;
            target++;
            count++;
        }
        origin++;
    }
    *target = 0;
}
// -------------------------------------------------------------------------------------------------
void
Framework::decodeURL(char* URL)
{
    char* dc = URL;
    while (*URL) {
        if (*URL == '%') {
            *dc = (char)fcgi_driver::hex2byte(URL + 1);
            URL += 2;
        } else if (*URL == '+')
            *dc = ' ';
        else
            *dc = *URL;
        dc++;
        URL++;
    }
    *dc = 0;
}
// -------------------------------------------------------------------------------------------------
void
Framework::decodeURL(const char* URL, std::string& target)
{
    target.clear();
    while (*URL) {
        if (*URL == '%') {
            target.push_back((char)fcgi_driver::hex2byte(URL + 1));
            URL += 2;
        } else if (*URL == '+')
            target.push_back(' ');
        else
            target.push_back(*URL);
        URL++;
    }
}
// -------------------------------------------------------------------------------------------------
bool
Framework::decodeDate(const char* date, tm* tptr)
{
    char* rv;
    // yyyy-mm-dd | dd.mm.yyyy | mm/dd/yyyy | dd.mm.yy | mm/dd/yy
    memset(tptr, 0, sizeof(tm));
    int len = strlen(date);
    // ISO date is easiest let's check it first.
    if (len >= 8 && date[4] == '-' && date[7] == '-') {
        strptime(date, "%Y-%m-%d", tptr);
        return true;
    }
    // Next we see how long is the year.
    const char* end = date + len;
    while (*end != '.' && *end != '/' && end != date)
        end--;
    if (end == date)
        return false;
    if ((date + len) - end > 2) {
        if (date[1] == '.' || date[2] == '.')
            rv = strptime(date, "%d.%m.%Y", tptr);
        else if (date[1] == '/' || date[2] == '/')
            rv = strptime(date, "%m/%d/%Y", tptr);
        else
            return false;
    } else {
        if (date[1] == '.' || date[2] == '.')
            rv = strptime(date, "%d.%m.%y", tptr);
        else if (date[1] == '/' || date[2] == '/')
            rv = strptime(date, "%m/%d/%y", tptr);
        else
            return false;
    }
    if (!rv || *rv != 0)
        return false;
    return true;
}
// -------------------------------------------------------------------------------------------------
const char*
Framework::escapeHtml(const std::string& orig)
{
#if 0
    size_t len=orig.size()*1.2;
    if(!len) return "";
    if(len<esc_len) {
        esc_len = len;
        delete[] esc_buffer;
        esc_buffer = new char[esc_len];
    }
#endif
    if (orig.size() == 0)
        return "";
    escapeHtml(esc_buffer, orig.c_str());
    return esc_buffer;
}
// -------------------------------------------------------------------------------------------------
void
Framework::escapeHtml(char* target, const char* orig)
{
    while (*orig) {
        if (*orig == '<') {
            strcpy(target, "&#60;");
            target += 5;
        } else if (*orig == '>') {
            strcpy(target, "&#62;");
            target += 5;
        } else if (*orig == '\'') {
            strcpy(target, "&#39;");
            target += 5;
        } else if (*orig == '"') {
            strcpy(target, "&#34;");
            target += 5;
        } else
            *target++ = *orig;
        orig++;
    }
    *target = 0;
}
// -------------------------------------------------------------------------------------------------
const char*
Framework::escapeJson(const std::string& orig)
{
    char* target = esc_buffer;
    for (string::const_iterator si = orig.begin(); si != orig.end(); si++) {
        if (*si == '"') {
            *target++ = '\\';
            *target++ = '"';
        } else if (*si == '\n') {
            *target++ = '\\';
            *target++ = 'n';
        } else if (*si == '\t') {
            *target++ = '\\';
            *target++ = 't';
        } else
            *target++ = *si;
    }
    *target = 0;
    return esc_buffer;
}
// -------------------------------------------------------------------------------------------------
bool
Framework::moveUploadedFile(fcgi_driver::Request* req, const char* label, const char* target)
{
    char* pipe = 0;
    if (!label || !target) {
        return false;
    }
    CS_VAPRT_DEBU("Framework::moveUploadedFile - %s", target);
    const char* field = req->params.get(label, strlen(label));
    if (!field[0]) {
        CS_VAPRT_DEBU("Framework::moveUploadedFile - label %s not found.", label);
        return false; // nothing to do.
    }
    try {
        char* pipe = strchr((char*)field, '|');
        if (pipe) {
            *pipe = 0;
            c4s::path source(upload_dir, field);
            source.cp(c4s::path(target), c4s::PCF_FORCE | c4s::PCF_MOVE);
        } else {
            CS_VAPRT_DEBU("Framework::moveUploadedFile - unable to find pipe from: %s", field);
            return false;
        }
    } catch (const c4s::c4s_exception& ce) {
        CS_VAPRT_ERRO("Framework::moveUploadedFile - file %s move failure", pipe);
        return false;
    }

    return true;
}
// -------------------------------------------------------------------------------------------------
int
Framework::daemonize(const char* pid_fname)
/** Creates daemon from this process with different persona (www-data)
    Uses syslog to report errors.
    \param pid_fname    Name of the file where the daemon pid is stored.
    \return int         (same as fork-function) -1 on error.
                        0 running as child.
                        1 running as parent.
 */
{
    pid_t pid, child;
    long stored_pid;
    char pidstr[15];

    // Check for the pid file
    FILE* pfile = fopen(pid_fname, "rt");
    if (pfile) {
        int rv = 0;
        fgets(pidstr, sizeof(pidstr), pfile);
        pidstr[sizeof(pidstr) - 1] = 0;
        stored_pid = strtol(pidstr, 0, 10);
        fclose(pfile);
        if (stored_pid) {
            rv = kill(stored_pid, 0);
            if (rv == 0) {
                syslog(LOG_NOTICE, "PID file already exists and daemon running.");
                return -1;
            }
        }
        // We suppose that process is not running anymore
        syslog(LOG_WARNING, "PID exists (%d) but daemon not running. Errno %d. Restarting\n",
               (int)stored_pid, rv);
        c4s::path runfile(pid_fname);
        runfile.rm();
        runfile.set_ext(".socket");
        runfile.rm();
    }

    /* Drop user if there is one, and we were run as root */
    if (getuid() == 0 || geteuid() == 0) {
        struct passwd* pw = getpwnam("www-data");
        if (pw) {
            syslog(LOG_INFO, "parent - changin persona to www-data (%d)", pw->pw_uid);
            setgid(pw->pw_gid);
            setuid(pw->pw_uid);
        }
    }
    /* Fork off the parent process */
    fflush(0);
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "parent - forking failed: %d", errno);
        return -1;
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        syslog(LOG_INFO, "parent - OK, daemon running with pid: %d", pid);
        return 1;
    }
    /* At this point we are executing as the child process */
    freopen("/dev/null", "r", stdin);
    freopen("/dev/stdout", "w", stdout);
    freopen("/dev/stderr", "w", stderr);

    /* Cancel certain signals */
    signal(SIGTSTP, SIG_IGN); /* Various TTY signals */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    /* Change the file mode mask */
    umask(S_IWOTH | S_IXOTH);

    /* Store the pid number */
    child = getpid();
    pfile = fopen(pid_fname, "wt");
    if (pfile) {
        fprintf(pfile, "%d\n", child);
        fclose(pfile);
    } else {
        syslog(LOG_ERR, "daemon - unable to write pid file: %s. Errno %d", pid_fname, errno);
    }
    return 0;
}

} // namespace fcgi_frame