/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <ostream>
#include <cpp4scripts.hpp>

#include "Framework.hpp"
#include "HtmlBuffer.hpp"
#include "SessionBase.hpp"

using namespace std;

namespace fcgi_frame {

Includer::Includer(const std::string& exe_dir, c4s::configuration* conf)
{
    uint64_t val;
    docroot = exe_dir;
    if (!conf->get_value("LibFCGI", "IncludeEditDefault", val))
        CONF_edit_default = 0;
    else
        CONF_edit_default = val;
}
// -------------------------------------------------------------------------------------------------
Includer::~Includer() {}
// -------------------------------------------------------------------------------------------------
const char*
Includer::getPath(fcgi_driver::Request* req, const char* fname)
{
    static char inc_name[150];
    SessionBase* sb = (SessionBase*)req->app_data;
    strcpy(inc_name, docroot.c_str());
    strcat(inc_name, sb->appstr->getLName());
    strcat(inc_name, "/");
    strcat(inc_name, fname);
    return inc_name;
}
// -------------------------------------------------------------------------------------------------
const char*
Includer::getEditPath(fcgi_driver::Request* req, const char* name)
{
    static char inc_name[150];
    SessionBase* sb = (SessionBase*)req->app_data;
    strcpy(inc_name, docroot.c_str());
    strcat(inc_name, sb->appstr->getLName());
    strcat(inc_name, "/e_");
    strcat(inc_name, name);
    strcat(inc_name, ".html");
    return inc_name;
}
// -------------------------------------------------------------------------------------------------
bool
Includer::exists(fcgi_driver::Request* req, const char* fname)
{
    c4s::path incfile(getPath(req, fname));
    return incfile.exists();
}
// -------------------------------------------------------------------------------------------------
void
Includer::remove(fcgi_driver::Request* req, const char* fname)
{
    c4s::path(getPath(req, fname)).rm();
}
// -------------------------------------------------------------------------------------------------
void
Includer::editable(fcgi_driver::Request* req, const char* name, HtmlBuffer* hb, bool rights)
{
    SessionBase* sb = (SessionBase*)req->app_data;
    if (rights)
        hb->html << "<div id='frame_" << name
                 << "' class='editor-frame' data-toggle='popover'><div data-editor_id='" << name
                 << "' class='myeditors'>\n";
    hb->bufferFlush(req);
    if (!toRequest(getEditPath(req, name), req)) {
        hb->html << "<p>" << sb->appstr->getsp(CONF_edit_default) << "</p>\n";
    }
    if (rights)
        hb->html << "\n</div></div>\n";
}
// -------------------------------------------------------------------------------------------------
bool
Includer::toRequest(const char* fname, fcgi_driver::Request* req, bool localize)
{
    // #ifdef _DEBUG
    //     char dbuffer[128];
    //     req->log(dbuffer, sprintf(dbuffer, "Includer::toRequest - %s\n", fname));
    //     CS_PRINT_DEBU(dbuffer);
    // #endif
    int fd = open((localize ? getPath(req, fname) : fname), O_RDONLY);
    if (fd < 0) {
        if (errno != ENOENT) {
            CS_VAPRT_ERRO("Includer::toRequest - Unable to open '%s'. System error:%d", fname,
                          errno);
        }
        CS_PRINT_DEBU("Includer::toRequest - no such file.");
        return false;
    }
    bool rv = req->writeFd(fd);
    if (!rv) {
        CS_PRINT_ERRO("Includer::toRequest - source read failed.");
    }
    close(fd);
    return rv;
}
// -------------------------------------------------------------------------------------------------
bool
Includer::toHtml(const char* fname, ostringstream& html)
{
    ssize_t br, max, total;
    // ssize_t original_max;
    char rbuf[0x1000]; // 4k buffer

    int fd = open(fname, O_RDONLY);
    if (fd < 0) {
        if (errno != ENOENT) {
            CS_VAPRT_ERRO("Includer::toHtml - Unable to open '%s'. System error:%d", fname, errno);
        } else {
            CS_VAPRT_DEBU("Includer::toHtml - '%s' not found. Forgot getPath()?", fname);
        }
        return false;
    }
    max = fcgi_driver::REQ_MAX_OUT - html.tellp();
    // original_max = max;
    total = 0;
    do {
        br = ::read(fd, rbuf, max < 0x1000 ? max : 0x1000);
        if (br < 0) {
            CS_VAPRT_ERRO("Includer::toHtml - File %s read failure (%d)", fname, errno);
            return false;
        } else if (br > 0)
            html.write(rbuf, br);
        max -= br;
        total += 0;
    } while (br > 0 && max > 0);
    // CS_VAPRT_DEBU("Includer::toHtml - max %ld; total %ld",original_max, total);
    return true;
}
// -------------------------------------------------------------------------------------------------
void
Includer::writeSafeHtml(int fd, const char* txt)
{
    // NOTE! Do not send JS Editor text here! Only short user editable text that is not supposed to
    // have any html.
    while (*txt) {
        switch (*txt) {
        case '&':
            write(fd, "&amp;", 5);
            break;
        case '\'':
            write(fd, "&apos;", 6);
            break;
        case '\"':
            write(fd, "&quot;", 6);
            break;
        case '<':
            write(fd, "&lt;", 4);
            break;
        case '>':
            write(fd, "&g;t", 4);
            break;
        default:
            write(fd, txt, 1);
        }
        txt++;
    }
}
// -------------------------------------------------------------------------------------------------
bool
Includer::save(fcgi_driver::Request* req, const char* fname, const char* content, const char* title)
{
    const char* fpath = getPath(req, fname);

    int fd =
        open(fpath, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd < 0) {
        CS_VAPRT_ERRO("Includer::save -- Error %d. Unable to open/create file: %s", errno, fname);
        return false;
    }
    if (fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1) {
        CS_VAPRT_ERRO("Includer::save -- Error %d. Unable to change permissions for: %s", errno,
                      fname);
        return false;
    }
    if (title) {
        write(fd, "<!--TITLE:", 10);
        writeSafeHtml(fd, title);
        write(fd, "-->\n", 4);
    }
    write(fd, content, strlen(content));
    close(fd);
    return true;
}

} // namespace fcgi_frame