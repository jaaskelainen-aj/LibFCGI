/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_SMTPMAILER_HPP
#define FCGI_SMTPMAILER_HPP

#include "../fcgisettings.h"

#define SMTP_ERROR_SIZE 256

extern "C" size_t
readSmtp(void*, size_t, size_t, void*);

namespace fcgi_frame {

class SmtpMailer;

class SmtpMsg
{
    friend class SmtpMailer;
    friend unsigned long ::readSmtp(void*, size_t, size_t, void*);

  public:
    SmtpMsg(const char* _from, const char* _subject, size_t bsize = 0x8000);
    ~SmtpMsg();
    void addRecipient(const char* rp);
    void clearRecipients();

    std::ostringstream body;

  private:
    size_t copySmtpData(void* ptr, size_t max);
    bool initializeSend(SmtpMailer* _m);

    char from[FRAME_SMTP_FROM];
    char header[FRAME_SMTP_HEADER];
    bool header_sent;
    bool data_sent;
    size_t header_size;
    void* curl;
    struct curl_slist* rcpt_list;
    size_t body_size;
    size_t encoded_size;
    size_t read_offset;
    char* body_buffer;

    SmtpMailer* mailer;
#ifdef _DEBUG
    char errormsg[SMTP_ERROR_SIZE];
#endif
};

// ..........................................................................................
class SmtpMailer
{
  public:
    SmtpMailer(const char* server, const char* user, const char* pwd);
    ~SmtpMailer();

    bool send(SmtpMsg* msg);
    int perform();
#ifdef UNIT_TEST
    int perform_UT(SmtpMsg*);
#endif
    char* getScratchBuffer(size_t size);

  private:
    void clear(bool final = false);

    std::string server, user, pwd;
    int running_count;
    int timeout_count;
    void* multi;
    SmtpMsg* transfers[FRAME_SMTP_MSG];
    char* scratch;
    size_t scratch_size;
};

} // namespace fcgi_frame

#endif