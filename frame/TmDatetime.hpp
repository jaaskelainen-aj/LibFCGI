/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_TMDATETIME_HPP
#define FCGI_TMDATETIME_HPP
#include <string.h>
#include <time.h>

#include "../fcgisettings.h"
#include "../driver/fcgidriver.hpp"
#include "AppStr.hpp"

namespace fcgi_frame
{

class TmDatetime
{
public:
    TmDatetime() { clear(); }
    TmDatetime(const TmDatetime &orig) { memcpy(&dtv, &orig.dtv, sizeof(struct tm)); }
    TmDatetime(time_t *tt) { localtime_r(tt, &dtv); }
    TmDatetime(struct tm *stm) { memcpy(&dtv, stm, sizeof(struct tm)); }
    TmDatetime(const char *str, APPSTR_LC tl) { parse(str, tl); }
    TmDatetime(int year, int month);
    void operator=(const TmDatetime &orig) { memcpy(&dtv, &orig.dtv, sizeof(struct tm)); }
    void operator=(time_t *tt) { localtime_r(tt, &dtv); }
    void operator=(struct tm *source) { memcpy(&dtv, source, sizeof(struct tm)); }

    void cp(struct tm *target) const { memcpy(target, &dtv, sizeof(struct tm)); }
    void cp(TmDatetime &target) const { memcpy(&target.dtv, &dtv, sizeof(struct tm)); }
    bool parse(CC, APPSTR_LC);
    bool parseISO(CC);
    int compare(TmDatetime &tg);
    int compare(time_t tg);

    void now();
    void thisMonth(); // current month with day=1 and time 00:00
    void nextMonth(int moff=1);
    void setDay1();

    int getYear() { return dtv.tm_year+1900; }
    int getMonth() { return dtv.tm_mon+1; }
    CC print(APPSTR_LC = APPSTR_NONE) const;
    CC printShort(APPSTR_LC = APPSTR_NONE) const;
    CC printTime(APPSTR_LC = APPSTR_NONE) const;
    CC printTimeShort(APPSTR_LC = APPSTR_NONE) const;
    CC printYear() const;
    CC printHourMinutes() const;
    CC printISODate() const;
    CC printISOTimestamp() const;

    bool empty() const { return dtv.tm_year==0?true:false; }
    bool isValid() { return dtv.tm_mday>0 ? true : false; }
    void clear() { memset(&dtv, 0, sizeof(struct tm)); }

    struct tm dtv;

protected:
    static char tstr[32];
};

} // namespace fcgi_frame

#endif