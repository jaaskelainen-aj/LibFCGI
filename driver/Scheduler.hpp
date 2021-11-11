/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Source code inspired and based on original work of Peter Simons and Ralph Babel
 * See https://www.nongnu.org/fastcgi/
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_SCHEDULER_HPP
#define FCGI_SCHEDULER_HPP

#include <stdexcept>
#include <map>
#include <string>
#include <ctime>
#include <cerrno>

#include "Driver.hpp"

namespace fcgi_driver {

class Scheduler
{
  public:
    explicit Scheduler(Driver*, const char* socket_path = 0, time_t idle_period = 0);
    ~Scheduler();

    bool run();
    bool idle() const;

    void set_poll_interval(int to)
    {
        if (to < -1)
            throw std::invalid_argument("Scheduler: Hard poll intervals must be -1 or greater.");
        hard_poll_interval = to;
    }

    void use_accurate_poll_interval() { hard_poll_interval = -2; }

    void reset_idle_timer() { time(&idle_start); }

  private:
    // Don't copy me!
    Scheduler(Scheduler const&);
    Scheduler& operator=(Scheduler const&);

    Driver* driver;
    pollfd poll_data;
    int hard_poll_interval;
    int fail_counter;
    // int next_conn_timeout;
    sigset_t sigmask;
    struct timespec next_conn_timeout;
    char socket_path[108];
    time_t idle_period;
    time_t idle_start;
};

} // namespace fcgi_driver

#endif