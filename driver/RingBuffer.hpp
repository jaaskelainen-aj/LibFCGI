/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_RINGBUFFER_HPP
#define FCGI_RINGBUFFER_HPP

#include <iostream>
#ifdef RB_THREAD_SAFE
  #include <pthread.h>
#endif

namespace fcgi_driver
{

class RBCallBack
{
public:
    RBCallBack() {}
    virtual ~RBCallBack() {}
    virtual void init_push(size_t)=0;
    virtual void push_back(char ch)=0;
    virtual void end_push()=0;
};

class RingBuffer
{
public:
#ifdef RB_THREAD_SAFE
    RingBuffer(size_t max, bool wait);
#else
    RingBuffer(size_t max);
#endif
    ~RingBuffer();

    size_t write(const void*, size_t);
    size_t read(void*, size_t);
    size_t read_into(std::string &);
    size_t read_into(int fd, size_t len);
    size_t read_max(void *, size_t, size_t, bool);
    size_t peek(void *, size_t);
    bool is_eof() { return eof; }

#ifdef RB_THREAD_SAFE
    size_t size();
    void clear();
    size_t capacity();
#else
    size_t size() { return size_internal(); }
    void clear() { reptr = wrptr; eof=false; }
    size_t capacity() { return capacity_internal(); }
    size_t gcount() { return last_read; }
    bool unread(size_t len=0);
    size_t copy(RingBuffer &, size_t);
    size_t push_to(RBCallBack *, size_t);
#endif
    size_t max_size() const { return RBMAX; }

    enum EXP_TYPE { TEXT, HEX };
    size_t exp_as_text(std::ostream &, size_t, EXP_TYPE);
    size_t discard(size_t);
    void dump(std::ostream &);

protected:
    size_t size_internal() const;
    size_t capacity_internal() const;

#ifdef RB_THREAD_SAFE
    pthread_mutex_t mtx_buffer;
    pthread_mutex_t mtx_data;
    pthread_cond_t cond_read;
    bool wait;
#endif

    size_t RBMAX,last_read;
    char *rb;
    char *reptr;
    char *wrptr;
    char *end;
    bool eof;
};

} // namespace fcgi_driver

#endif