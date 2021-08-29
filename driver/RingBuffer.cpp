/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <unistd.h>
#include <stdexcept>
#include <string.h>

#include "RingBuffer.hpp"

#ifdef RB_THREAD_SAFE
  #define RBLOCK pthread_mutex_lock(&mtx_buffer)
  #define RBUNLOCK pthread_mutex_unlock(&mtx_buffer)
#else
  #define RBLOCK
  #define RBUNLOCK
#endif

namespace fcgi_driver
{

// -------------------------------------------------------------------------------------------------
#ifdef RB_THREAD_SAFE
RingBuffer::RingBuffer(size_t max, bool wait_)
#else
RingBuffer::RingBuffer(size_t max)
#endif
{
    RBMAX = max;
    last_read=0;
    rb = new char[RBMAX];
    memset(rb,0,RBMAX);

    reptr = rb;
    wrptr = rb;
    end = wrptr+RBMAX;
    eof = false;

#ifdef RB_THREAD_SAFE
    wait = wait_;
    pthread_mutex_init(&mtx_buffer, NULL);
    if(wait) {
        pthread_mutex_init(&mtx_data, NULL);
        pthread_cond_init(&cond_read, NULL);
    }
#endif
}

// -------------------------------------------------------------------------------------------------
RingBuffer::~RingBuffer()
{
#ifdef RB_THREAD_SAFE
    pthread_mutex_destroy(&mtx_buffer);
    if(wait) {
        pthread_mutex_destroy(&mtx_data);
        pthread_cond_destroy(&cond_read);
    }
#endif
    delete[] rb;
}

// -------------------------------------------------------------------------------------------------
size_t RingBuffer::write(const void *input, size_t slen)
{
    if(eof || !input || !slen)
        return 0;
    RBLOCK;
    size_t maxwrite = capacity_internal();
    if(maxwrite < slen)
        slen = maxwrite;

    size_t fp=end-wrptr;
    if(slen<fp) {
        memcpy(wrptr,input,slen);
        wrptr+=slen;
        if(wrptr==end)
            wrptr=rb;
    }else{
        memcpy(wrptr,input,fp);
        memcpy(rb,(char*)input+fp,slen-fp);
        wrptr=rb+(slen-fp);
    }
    if(wrptr==reptr)
        eof=true;
#ifdef RB_THREAD_SAFE
    if(wait && capacity_internal() >= 4) {
        pthread_mutex_lock(&mtx_data);
        pthread_cond_signal(&cond_read);
        pthread_mutex_unlock(&mtx_data);
    }
    pthread_mutex_unlock(&mtx_buffer);
#endif
    return slen;
}

// -------------------------------------------------------------------------------------------------
size_t RingBuffer::read(void *store, size_t slen)
{
    if(!slen || !store) {
        return 0;
    }
    RBLOCK;
    size_t ss=size_internal();
    if(!ss) {
        RBUNLOCK;
        last_read = 0;
        return 0;
    }
    if(slen>ss)
        slen=ss;
    size_t fp=end-reptr;
    if(slen<fp) {
        memcpy(store,reptr,slen);
        reptr+=slen;
        if(reptr==end)
            reptr=rb;
    }else{
        memcpy(store,reptr,fp);
        memcpy((char*)store+fp,rb,slen-fp);
        reptr = rb+(slen-fp);
    }
    eof=false;
    RBUNLOCK;
    last_read = slen;
    return slen;
}
// -------------------------------------------------------------------------------------------------
size_t RingBuffer::read_into(std::string &output)
{
    RBLOCK;
    last_read = 0;
    size_t ss=size_internal();
    if(!ss) {
        RBUNLOCK;
        return 0;
    }
    output.reserve(ss);
    while(reptr!=wrptr || eof) {
        output.push_back(*reptr);
        reptr++;
        last_read++;
        if(reptr==end)
            reptr=rb;
        eof = false;
    }
    RBUNLOCK;
    return last_read;
}
// -------------------------------------------------------------------------------------------------
size_t RingBuffer::read_into(int fd, size_t slen)
{
    if(!slen || fd<0) {
        return 0;
    }
    RBLOCK;
    size_t ss=size_internal();
    if(!ss) {
        RBUNLOCK;
        last_read = 0;
        return 0;
    }
    if(slen>ss)
        slen=ss;
    size_t fp=end-reptr;
    if(slen<fp) {
        ::write(fd, reptr, slen);
        reptr+=slen;
        if(reptr==end)
            reptr=rb;
    }else{
        ::write(fd, reptr, fp);
        ::write(fd, rb, slen-fp);
        reptr = rb+(slen-fp);
    }
    eof=false;
    RBUNLOCK;
    last_read = slen;
    return slen;
}
// -------------------------------------------------------------------------------------------------
size_t RingBuffer::read_max(void *store, size_t store_size, size_t max, bool partial)
{
    if(store_size>=max)
        return read(store,max);
    size_t br=0;
    if(partial)
        br = read(store,store_size);
    discard(max-br);
    return br;
}

// -------------------------------------------------------------------------------------------------
size_t RingBuffer::peek(void *store, size_t slen)
{
    if(!slen || !store)
        return 0;
    RBLOCK;
    size_t ss=size_internal();
    if(!ss) {
        RBUNLOCK;
        return 0;
    }
    if(slen>ss)
        slen=ss;
    size_t fp=end-reptr;
    if(slen<fp) {
        memcpy(store,reptr,slen);
    }else{
        memcpy(store,reptr,fp);
        memcpy((char*)store+fp,rb,slen-fp);
    }
    RBUNLOCK;
    return slen;
}

// -------------------------------------------------------------------------------------------------
size_t RingBuffer::exp_as_text(std::ostream &os, size_t slen, EXP_TYPE type)
{
    unsigned short int sich;
    char ch;
    if(!slen) {
        return 0;
    }
    RBLOCK;
    size_t ss=size_internal();
    if(!ss) {
        RBUNLOCK;
        last_read=0;
        return 0;
    }
    if(slen>ss)
        slen=ss;
    if(type == HEX)
        os << std::hex;
    while(reptr!=wrptr && slen) {
        ch = *reptr;
        if(type == HEX) {
            sich = 0xff&((unsigned short) ch);
            os << sich <<',';
        }
        else
            os << ch;
        reptr++;
        slen--;
        if(reptr==end)
            reptr=rb;
    }
    if(type == HEX)
        os << std::dec;
    eof=false;
    RBUNLOCK;
    last_read = slen;
    return slen;
}
// -------------------------------------------------------------------------------------------------
size_t RingBuffer::discard(size_t slen)
{
    if(!slen) {
        return 0;
    }
    RBLOCK;
    size_t ss=size_internal();
    if(!ss) {
        RBUNLOCK;
        return 0;
    }
    if(slen>ss)
        slen=ss;
    while( (reptr!=wrptr || eof) && slen) {
        reptr++;
        slen--;
        if(reptr==end)
            reptr=rb;
        eof = false;
    }
    RBUNLOCK;
    return slen;
}

// -------------------------------------------------------------------------------------------------
bool RingBuffer::unread(size_t length)
{
    size_t rewind=length?length:last_read;
    if(!rewind)
        return true;
    if(capacity_internal()<rewind)
        return false;
    reptr -= rewind;
    if(reptr<rb)
        reptr = end - (rb-reptr);
    return true;
}

// -------------------------------------------------------------------------------------------------
#ifdef RB_THREAD_SAFE
void RingBuffer::clear()
{
    pthread_mutex_lock(&mtx_buffer);
    reptr = wrptr;
    eof=false;
    pthread_mutex_unlock(&mtx_buffer);
}
#endif

// -------------------------------------------------------------------------------------------------
#ifdef RB_THREAD_SAFE
size_t RingBuffer::size()
{
    pthread_mutex_lock(&mtx_buffer);
    size_t s = size_internal();
    pthread_mutex_unlock(&mtx_buffer);
    return s;
}
#endif
size_t RingBuffer::size_internal() const
//! Returns number of bytes waiting for reading.
{
    if(eof)
        return RBMAX;
    if(wrptr==reptr)
        return 0;
    if(wrptr>reptr)
        return wrptr-reptr;
    return (end-reptr)+(wrptr-rb);
}

// -------------------------------------------------------------------------------------------------
#ifdef RB_THREAD_SAFE
size_t RingBuffer::capacity()
{
    pthread_mutex_lock(&mtx_buffer);
    size_t c = capacity_internal();
    pthread_mutex_unlock(&mtx_buffer);
    return c;
}
#endif
size_t RingBuffer::capacity_internal() const
//! Returns the remaining write size
{
    if(eof)
        return 0;
    if(wrptr == reptr)
        return RBMAX;
    if(wrptr>reptr)
        return (end-wrptr)+(reptr-rb);
    return reptr-wrptr;
}

// -------------------------------------------------------------------------------------------------
#ifndef RB_THREAD_SAFE
size_t RingBuffer::copy(RingBuffer &source, size_t copy_size)
{
    size_t cb=0, actual;
    char buffer[0x800];
    size_t max = capacity_internal();
    if(!copy_size || !max)
        return 0;
    if(copy_size>max)
        copy_size=max;
    while(copy_size) {
        actual = copy_size>sizeof(buffer) ? sizeof(buffer):copy_size;
        actual = source.read(buffer,actual);
        write(buffer,actual);
        copy_size -= actual;
        cb+=actual;
    }
    return cb;
}
// -------------------------------------------------------------------------------------------------
size_t RingBuffer::push_to(RBCallBack *callback, size_t max)
{
    size_t slen=0;
    size_t ss=size_internal();
    if(!ss)
        return 0;
    char *original_re=reptr;
    try {
        callback->init_push(max);
        while(reptr!=wrptr && slen<max) {
            callback->push_back(*reptr);
            slen++;
            reptr++;
            if(reptr==end)
                reptr = rb;
        }
        callback->end_push();
    } catch(const std::runtime_error &re) {
        reptr = original_re;
        last_read = 0;
        return 0;
    }
    eof=false;
    last_read = slen;
    return slen;
}
#endif

// -------------------------------------------------------------------------------------------------
void RingBuffer::dump(std::ostream &os)
{
    RBLOCK;
    os << "RingBuffer:\n  begin:"<<(const void*)rb<<"; reptr:"<<(const void*)reptr<<"; wrptr:"<<(const void*)wrptr<<";\n";
    os <<"  size:"<<size_internal()<<"; capacity:"<<capacity_internal()<<"; rbmax:"<<RBMAX<<"; eof:";
    if(eof) os<<"true;";
    else os<<"false;";
#ifdef RB_THREAD_SAFE
    os << " wait:";
    if(wait) os<<"true;";
    else os<<"false;";
    pthread_mutex_unlock(&mtx_buffer);
#endif
    if(RBMAX<100) {
        os<<" chars:";
        RBLOCK;
        for(char *ndx=rb; ndx<end; ndx++) {
            if(*ndx>31 && *ndx<127)
                os << *ndx;
            else
                os << '.';
        }
        RBUNLOCK;
    }
    os<<'\n';
}

} // namespace fcgi_driver