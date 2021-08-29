/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Source code inspired and based on original work of Peter Simons and Ralph Babel
 * See https://www.nongnu.org/fastcgi/
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <algorithm>
#include <fstream>
#include <stdexcept>

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <time.h>

#include <cpp4scripts.hpp>

#include "../fcgisettings.h"
#include "Scheduler.hpp"

extern FILE* trace;
pollfd pfdarray[DRIVER_POLL_FD];

using namespace std;
using namespace c4s;

namespace fcgi_driver
{

// ------------------------------------------------------------------------------------------
Scheduler::Scheduler(Driver *_driver, const char *sp, time_t _idle_period)
    : driver(_driver)
{
    memset(socket_path, 0, sizeof(socket_path));
    idle_period = _idle_period;
    time(&idle_start);
    if(sp) {
        struct sockaddr_un addr{};
        strncpy(socket_path, sp, sizeof(socket_path)-1);
        poll_data.fd = socket(PF_LOCAL, SOCK_STREAM, 0);
        if(poll_data.fd == -1) {
            throw runtime_error("Scheduler - Socket error");
        }
        CS_VAPRT_INFO("Scheduler::Scheduler - new socket %s fd %d",sp,poll_data.fd);
        addr.sun_family = PF_LOCAL;
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
        addr.sun_path[sizeof(addr.sun_path)-1]=0;
        // Just in case we still have old file lingering
        unlink(socket_path);
        if( bind(poll_data.fd,(struct sockaddr*)&addr, sizeof(addr)) == -1) {
            throw runtime_error("Scheduler - bind error");
        }
        if( listen(poll_data.fd, 5) == -1) {
            throw runtime_error("Scheduler - listen error");
        }
    } else {
        poll_data.fd = 0;
    }
    use_accurate_poll_interval();
    poll_data.events = POLLIN;
    poll_data.revents = 0;
    fail_counter = 0;
    next_conn_timeout.tv_sec = 0;
    next_conn_timeout.tv_nsec = 0; // 10000000L;

    sigemptyset(&sigmask);
    sigaddset(&sigmask,SIGTERM);
    sigaddset(&sigmask,SIGINT);
    sigaddset(&sigmask,SIGQUIT);
    sigaddset(&sigmask,SIGKILL);
}
// ------------------------------------------------------------------------------------------
Scheduler::~Scheduler()
{
    if(poll_data.fd != 0)
        unlink(socket_path);
}
// ------------------------------------------------------------------------------------------
bool Scheduler::run()
{
    if(!driver) {
        CS_PRINT_CRIT("Scheduler::run - missing driver. Terminating.");
        return false;
    }
    // Listen for new connections first
    int rc = ppoll(&poll_data, 1, &next_conn_timeout, &sigmask);
    if(rc==-1) {
        if(errno == EINTR) {
            CS_PRINT_DEBU("Scheduler::run - Signal received while polling (a).");
            return true; // This is OK. the main program will check if we need to terminate or not.
        }
        if(fail_counter++ < 10)
            return true;
        throw runtime_error("Scheduler::run - Too many poll failures.");
    }
    else if(rc==1) {
        pollfd newfd{};
        struct sockaddr_un addr_peer{};
        socklen_t addr_size = sizeof(sockaddr_un);
        int socket = accept(poll_data.fd, (struct sockaddr *)&addr_peer, &addr_size);
        if (socket >= 0) {
            if (fcntl(socket, F_SETFL, O_NONBLOCK) == -1)
                throw runtime_error(std::string("Scheduler::run - Cannot set non-blocking mode: ") + strerror(errno));
        }
        else
            throw runtime_error(std::string("Scheduler::run - accept() failed: ") + strerror(errno));
        TRACE("Scheduler::run - New socked with fd:%d\n",socket);
        newfd.fd=socket;
        newfd.events = POLLIN;
        newfd.revents = 0;
        driver->createRequest(&newfd); // => RQS_PARAMS
    }

    // Poll the open connections.
    size_t count = driver->fillPollFd(pfdarray, DRIVER_POLL_FD);
    if(!count) {
        next_conn_timeout.tv_sec = 3;
        next_conn_timeout.tv_nsec = 0;
        return true;
    }
    next_conn_timeout.tv_sec = 0;
    next_conn_timeout.tv_nsec = 0; // 10000000L;
    rc = ppoll(pfdarray, count, &next_conn_timeout, &sigmask);
    if (rc == -1)
    {
        if (errno == EINTR) {
            CS_PRINT_DEBU("Scheduler::run - Signal received while polling (b).");
            return true;
        }
        throw runtime_error(std::string("Scheduler::run - ppoll failed. Error: ") + strerror(errno));
    }
    // while reads
    bool rw=false;
    Request *rq;
    for(int ndx=0; ndx<count; ndx++) {
        if( (pfdarray[ndx].revents & POLLIN) > 0) {
            rw = true;
            rq = driver->findRequest(pfdarray[ndx].fd);
            if(rq)
                driver->read(rq);
        }
    }
    // Work with running requests.
    driver->work();
    // while writes
    for(int ndx=0; ndx<count; ndx++) {
        if( (pfdarray[ndx].revents & POLLOUT) > 0) {
            rw = true;
            rq = driver->findRequest(pfdarray[ndx].fd);
            if(rq)
                driver->write(rq);
        }
    }
    driver->freeDormantRequests();
    if(rw) {
        time(&idle_start);
    }
    return true;
}
// ------------------------------------------------------------------------------------------
bool Scheduler::idle() const {
    time_t now;
    time(&now);
    return now - idle_start > idle_period ? true : false;
}

} // namespace fcgi_driver