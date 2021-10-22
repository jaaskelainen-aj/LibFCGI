/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */

/*
Testing the dev manually:

Telnet localhost 25
helo mcdev.fi<enter>
>> 250 OK
mail from: root@dev.fi<enter>
>> 250 OK
rcpt to: root@dev.fi
>> 250 OK
data<enter>
>> 354 Send data.  End with CRLF.CRLF
To: Root<enter>
From: Root<enter>
Subject: Test message<enter>
Test message<enter>
<enter>
. <enter>
>> 250 OK
quit <enter>
*/

/********************************************************************************/
#include <curl/curl.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <cpp4scripts.hpp>

#include "../driver/fcgidriver.hpp"
#include "base64.h"
#include "SmtpMailer.hpp"

using namespace std;

#if SMTP_ERROR_SIZE<CURL_ERROR_SIZE
#error Too small error buffer
#endif

// -------------------------------------------------------------------------------------------------
extern "C" {
    unsigned long readSmtp(void *ptr, size_t size, size_t nmemb, void *userp)
    {
        if(size == 0 || nmemb == 0) {
            // cout<<"readFun - no space to write.\n";
            return 0;
        }
        auto *msg = (fcgi_frame::SmtpMsg*) userp;
        return msg->copySmtpData(ptr, size*nmemb);
    }
}

namespace fcgi_frame
{

SmtpMsg::SmtpMsg(const char *_from, const char *_subject, size_t bsize)
{
    curl = 0;
    rcpt_list = 0;
    body_size = 0;
    read_offset = 0;
    body_buffer = 0;
    header_sent = false;
    data_sent = false;
    header_size = 0;
    encoded_size = 0;
    mailer = 0;

    if(!_from || !_subject) {
        // cout << "SmtpMsg::SmtpMsg - from and subject must be specified.\n";
        throw runtime_error("SmtpMsg::SmtpMsg - from and subject must be specified.");
    }

    // Reserve email buffer
    body_size  = bsize;
    body_buffer = new char[body_size];
    std::stringbuf *pbuf = body.rdbuf();
    pbuf->pubsetbuf(body_buffer, body_size);

    // Write the required headers
    char ctime_b[64];
    time_t now = time(0);
    ctime_r(&now, ctime_b);
    fcgi_driver::trim(ctime_b);

    strncpy(from, _from, FRAME_SMTP_FROM-1);
    from[FRAME_SMTP_FROM-1] = 0;
    int reqmax = snprintf(header, sizeof(header),
                          "Content-Type: text/plain; charset=UTF-8\r\n"
                          "Content-Transfer-Encoding: base64\r\n"
                          "Date:%s\r\nFrom:%s\r\nSubject:%s\r\n\r\n",
                          ctime_b, from, _subject);
    if(reqmax<0 || (size_t)reqmax > sizeof(header)) {
        throw runtime_error("SmtpMsg::SmtpMsg - header too long (or failed).");
    }
    header_size = (size_t) reqmax;

    // Initialize this message
#ifndef UNIT_TEST
   curl = curl_easy_init();
    if(!curl) {
        // cout << "SmtpMsg::SmtpMsg - curl_easy_init failed.\n";
        throw runtime_error("SmtpMsg::SmtpMsg - curl_easy_init failed.");
    }
#endif
#ifdef _DEBUG
    memset(errormsg, 0, SMTP_ERROR_SIZE);
#endif
}
SmtpMsg::~SmtpMsg()
{
    if(rcpt_list)
        curl_slist_free_all(rcpt_list);
    if(curl)
        curl_easy_cleanup(curl);
    if(body_buffer)
        delete[] body_buffer;
}
void SmtpMsg::addRecipient(const char *rp)
{
    char TO[FRAME_SMTP_FROM];
    if(strlen(rp) >= FRAME_SMTP_FROM-3) {
        CS_PRINT_WARN("SmtpMsg::addRecipient - recipient too long. Ignored.");
        return;
    }
    sprintf(TO, "<%s>", rp);
    rcpt_list = curl_slist_append(rcpt_list, TO);
}
void SmtpMsg::clearRecipients()
{
    if(rcpt_list)
        curl_slist_free_all(rcpt_list);
}
// -------------------------------------------------------------------------------------------------
// private to be called by Mailer only
bool SmtpMsg::initializeSend(SmtpMailer *_m)
{
    mailer = _m;
    encoded_size = base64_size(body.tellp());
    if(encoded_size > body_size) {
        CS_PRINT_ERRO("SmtpMsg::initializeSend - body too large to encode.");
        return false;
    }
    char* base64 = mailer->getScratchBuffer(encoded_size);
    size_t rv = base64_encode(base64, encoded_size, (const unsigned char*)body_buffer, body.tellp(), Base64Type::URL);
    if(rv == 0) {
        CS_PRINT_ERRO("SmtpMsg::initializeSend - encoding failed.");
        return false;
    }
    memcpy(body_buffer, base64, encoded_size);
    CS_PRINT_TRCE("SmtpMsg::initializeSend - body encoded");
    return true;
}
// -------------------------------------------------------------------------------------------------
size_t SmtpMsg::copySmtpData(void *ptr, size_t max)
{
    if(data_sent)
        return 0;
    if (!encoded_size) {
        CS_PRINT_ERRO("SmtpMsg::copySmtpData - Data has not been initialized");
        return 0;
    }
    size_t copied=0;
    char *target = (char*)ptr;
    if(!header_sent) {
        if(max>header_size-read_offset) {
            copied = header_size-read_offset;
            memcpy(target, header+read_offset, copied);
            read_offset = 0;
            header_sent = true;
            CS_PRINT_TRCE("SmtpMsg::copySmtpData - header complete.");
            return copied;
        }
        copied =  max;
        memcpy(target, header+read_offset, copied);
        read_offset += copied;
        return copied;
    }

    while(copied<max && read_offset<encoded_size) {
        if(read_offset%76==0) {
            if(max-copied>=2) {
                memcpy(target+copied, "\r\n", 2);
                copied+=2;
            }
            else return copied;
        }
        target[copied] = body_buffer[read_offset];
        copied++;
        read_offset++;
    }
    if(read_offset>=encoded_size) {
        if(max-copied>=5) {
            memcpy(target+copied, "\r\n.\r\n", 5);
            copied += 5;
            data_sent = true;
            CS_PRINT_TRCE("SmtpMsg::copySmtpData - body complete.");
        }
    }
    CS_VAPRT_TRCE("SmtpMsg::copySmtpData - copied %ld",copied);
    return copied;
}

// -------------------------------------------------------------------------------------------------

SmtpMailer::SmtpMailer(const char *_server, const char *_user, const char *_pwd)
{
    running_count=0;
    timeout_count=0;
    scratch = 0;

    if(!_server)
        throw runtime_error("SmtpMailer::SmtpMailer - missing server url.");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    multi = curl_multi_init();
    if(!multi) {
        // cout << "SmtpMailer::SmtpMailer - curl_multi_init failed.";
        throw runtime_error("SmtpMailer::SmtpMailer - curl_multi_init failed.");
    }
    // Initialize attributes
    server = _server;
    if(_user)
        user = _user;
    if(_pwd)
        pwd  = _pwd;
    memset(transfers, 0, sizeof(transfers));
    CS_PRINT_TRCE("SmtpMailer::SmtpMailer - Initialized.");
}

SmtpMailer::~SmtpMailer()
{
    clear(true);
    curl_global_cleanup();
    if(scratch)
        delete[] scratch;
}
void SmtpMailer::clear(bool final)
{
    for(int ndx=0; ndx<FRAME_SMTP_MSG; ndx++) {
        if(transfers[ndx])
            delete transfers[ndx];
    }
    curl_multi_cleanup(multi);
    if(!final) {
        CS_PRINT_NOTE("SmtpMailer::clear - clearing due to timeouts or errors.");
        multi = curl_multi_init();
        running_count=0;
        timeout_count=0;
    }
}
bool SmtpMailer::send(SmtpMsg *msg)
{
    if(!msg->initializeSend(this)) {
        return false;
    }
#ifndef UNIT_TEST
    int ndx;
    for(ndx=0; ndx<FRAME_SMTP_MSG; ndx++) {
        if(!transfers[ndx]) {
            transfers[ndx] = msg;
            break;
        }
    }
    if(ndx == FRAME_SMTP_MSG) {
        CS_PRINT_WARN("SmtpMailer::send - transfer list full!");
        return false;
    }
    CS_VAPRT_NOTE("SmtpMailer::send - Begin sending #%d. Running count %d", ndx, running_count);
    if(!msg->rcpt_list) {
        CS_PRINT_ERRO("SmtpMailer::send - empty recipient list.");
        return false;
    }
    CS_PRINT_TRCE("SmtpMailer::send - setting curl options.");
    curl_easy_setopt(msg->curl, CURLOPT_URL, server.c_str());
    if(!user.empty())
        curl_easy_setopt(msg->curl, CURLOPT_USERNAME, user.c_str());
    if(!pwd.empty())
        curl_easy_setopt(msg->curl, CURLOPT_PASSWORD, pwd.c_str());
    if(curl_easy_setopt(msg->curl, CURLOPT_READFUNCTION, readSmtp) != CURLE_OK) {
        CS_PRINT_ERRO("SmtpMailer::send - CURLOPT_READFUNCTION failed.");
        return false;
    }
    if(curl_easy_setopt(msg->curl, CURLOPT_READDATA, (void*) msg) != CURLE_OK) {
        CS_PRINT_ERRO("SmtpMailer::send - CURLOPT_READDATA failed.");
        return false;
    }
    curl_easy_setopt(msg->curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(msg->curl, CURLOPT_MAIL_FROM, msg->from);
    curl_easy_setopt(msg->curl, CURLOPT_MAIL_RCPT, msg->rcpt_list);
    curl_easy_setopt(msg->curl, CURLOPT_SSL_VERIFYPEER, 0L);
#ifdef _DEBUG
    curl_easy_setopt(msg->curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(msg->curl, CURLOPT_ERRORBUFFER, msg->errormsg);
#endif
    curl_easy_setopt(msg->curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(msg->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    // curl_easy_setopt(msg->curl, CURLOPT_SSLVERSION, 0L);
    // curl_easy_setopt(msg->curl, CURLOPT_SSL_SESSIONID_CACHE, 0L);

    timeout_count = 0;
    msg->read_offset = 0;
    curl_multi_add_handle(multi, msg->curl);
    curl_multi_perform(multi, &running_count);
#endif
    return true;
}
#ifdef UNIT_TEST
int SmtpMailer::perform_UT(SmtpMsg *msg)
{
    const int BS=512;
    char ptr[BS];

    size_t br = readSmtp(ptr, BS, 1, msg);
    if(br) {
        cout.write(ptr, br);
        cout<<'\n';
    }
    return br;
}
#endif
// -------------------------------------------------------------------------------------------------
int SmtpMailer::perform()
{
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd = -1;
    int rc;
    CURLMcode mc;

    if(running_count==0)
        return 0;
    if(timeout_count>10) {
        clear();
        return 0;
    }
    /* get file descriptors from the transfers */
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);
    mc = curl_multi_fdset(multi, &fdread, &fdwrite, &fdexcep, &maxfd);
    if(mc != CURLM_OK) {
        CS_PRINT_WARN("SmtpMailer::perform - curl_multi_fdset failed.");
        clear();
        return 0;
    }
    struct timeval timeout;
    timeout.tv_sec = 0;
    if(maxfd == -1) {
        timeout.tv_usec = 100 * 1000; // 0.1 seconds
        rc = select(0, 0, 0, 0, &timeout);
    } else {
        timeout.tv_usec = 500 * 1000; // 0.3 seconds
        rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    }
    if(rc == -1) {
        CS_PRINT_WARN("SmtpMailer::perform - select failed.");
        clear();
        return 0;
    }
    else if(rc ==  0) { // timeout
        CS_PRINT_TRCE("SmtpMailer::perform - timeout");
        timeout_count++;
    }

    int last_count = running_count;
    curl_multi_perform(multi, &running_count);
    if(last_count == running_count) {
        CS_PRINT_TRCE("SmtpMailer::perform - no change in status");
        return running_count;
    }
    struct CURLMsg *cmsg;
    do {
        cmsg = curl_multi_info_read(multi, &rc);
        if(cmsg && cmsg->msg == CURLMSG_DONE) {
            for(int ndx=0; ndx<FRAME_SMTP_MSG; ndx++) {
                if(transfers[ndx]) {
                    if(cmsg->easy_handle == transfers[ndx]->curl) {
                        curl_multi_remove_handle(multi, transfers[ndx]->curl);
                        if(cmsg->data.result == CURLE_OK)
                            CS_VAPRT_NOTE("SmtpMailer::perform - Transfer #%d ready.", ndx);
                        else
#ifdef _DEBUG
                            CS_VAPRT_ERRO("SmtpMailer::perform - Transfer #%d ready. Error: %d. %s", ndx, cmsg->data.result, transfers[ndx]->errormsg);
#else
                            CS_VAPRT_ERRO("SmtpMailer::perform - Transfer #%d ready. Error: %s", ndx, curl_easy_strerror(cmsg->data.result));
#endif
                        delete transfers[ndx];
                        transfers[ndx] = 0;
                    }
                }
            }
        }
    } while(cmsg);
    CS_VAPRT_TRCE("SmtpMailer::perform - running count %d",running_count);
    return running_count;
}

// -------------------------------------------------------------------------------------------------
char* SmtpMailer::getScratchBuffer(size_t size)
{
    if(scratch_size>=size && scratch)
        return scratch;
    if(scratch)
        delete[] scratch;
    scratch = new char[size];
    scratch_size = size;
    return scratch;
}

} // namespace fcgi_frame