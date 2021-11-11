/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_INCLUDER_HPP
#define FCGI_INCLUDER_HPP

namespace fcgi_driver {
class Request;
}

namespace fcgi_frame {

class HtmlBuffer;

#define LOCALIZE true

class Includer
{
  public:
    Includer(const std::string&, c4s::configuration* cf);
    ~Includer();

    //- // New interface
    void editable(fcgi_driver::Request* req, const char* name, HtmlBuffer* hb, bool rights);
    const char* getPath(fcgi_driver::Request* req, const char* fname);
    const char* getEditPath(fcgi_driver::Request* req, const char* name);
    bool toRequest(const char* fname, fcgi_driver::Request* req, bool localize = false);
    bool toHtml(const char* fname, std::ostringstream& html);

    // Legacy interface
    bool save(fcgi_driver::Request* req, const char* fname, const char* content, const char* title);
    bool exists(fcgi_driver::Request* req, const char* fname);
    void remove(fcgi_driver::Request* req, const char* fname);

  private:
    bool read(const char* fname, std::ostringstream& html, bool quiet, uint16_t _max = 0);
    void writeSafeHtml(int fd, const char* txt);
    std::string docroot;

    long CONF_edit_default;
};

} // namespace fcgi_frame

#endif