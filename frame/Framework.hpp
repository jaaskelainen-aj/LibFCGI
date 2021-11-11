/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_FRAMEWORK_HPP
#define FCGI_FRAMEWORK_HPP

#include <fstream>
#include <sstream>
#include <string>
#include <stdint.h>
// Framework dependensies:
#include <cpp4scripts.hpp>
#include <hoedown/document.h>

#include "Includer.hpp"
#include "SessionBase.hpp"
#include "MultiStr.hpp"
//#include "HtmlBuffer.hpp"
//#include "Serializer.hpp"
//#include "ErrorList.hpp"
//#ifdef WEBMAILER
//  #include "SmtpMailer.hpp"
//#endif
//#include "PostData.hpp"
//#include "TmDatetime.hpp"

namespace fcgi_frame {

class HandlerElement
{
  public:
    HandlerElement(const char*, fcgi_driver::Handler*);
    ~HandlerElement() { delete handler; }
    uint64_t key;
    fcgi_driver::Handler* handler;
};

class Framework
{
  public:
    static void create(const std::string& exe_dir, SessionFactoryIF* sf, c4s::configuration* cf);
    static void close();
    static Framework* get()
    {
        if (!the_frame)
            throw std::runtime_error(fmget_error);
        return the_frame;
    }

    static int daemonize(const char* pid_fname);

    SessionMgr* getSesMgr() { return sesmgr; }
    Includer* getIncluder() { return includer; }

    SessionBase* initializeRequest(fcgi_driver::Request*);
    void beginRequest(fcgi_driver::Request* req,
                      std::ostringstream& html,
                      const char* type = 0,
                      std::ostringstream* cookies = 0,
                      std::ostringstream* headers = 0);
    void closeRequest(fcgi_driver::Request* req, uint32_t http_status = 0);
    int login(fcgi_driver::Request* r) { return sesmgr->login(r); }
    void logout(fcgi_driver::Request* r) { sesmgr->logout(r); }
    void setLanguage(fcgi_driver::Request* r, const char* lc) { sesmgr->setLanguage(r, lc, true); }
    void purgeSessions() { sesmgr->purge(); }
    bool moveUploadedFile(fcgi_driver::Request* r, const char* label, const char* target);

    static void encodeURL(const char* origin, char* target, size_t tmax);
    static void decodeURL(char* URL);
    static void decodeURL(const char* URL, std::string& target);
    static bool decodeDate(const char* date, tm* tptr);
    static void escapeHtml(char*, const char*);
    static const char* escapeHtml(const std::string&);
    static const char* escapeJson(const std::string&);

    std::string page_name;
    std::string upload_dir;
    static const char* fmget_error;
    c4s::configuration* conf{};

  protected:
    Includer* includer{};
    SessionMgr* sesmgr{};
    int session_max{};
    MultiStr cookie_buffer;
    uint64_t handler_key_hash{};

  private:
    Framework(const std::string&, SessionFactoryIF*, c4s::configuration* cf);
    Framework(const Framework&) {}
    ~Framework();

    static Framework* the_frame;
    static char* esc_buffer;
    static size_t esc_len;
};

} // namespace fcgi_frame

#endif