/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <cstdio>
#include <ctime>

#include "TmDatetime.hpp"

namespace fcgi_frame {

char TmDatetime::tstr[32];

TmDatetime::TmDatetime(int year, int month)
{
    clear();
    dtv.tm_year = year - 1900;
    dtv.tm_mon = (month > 0 && month < 13) ? month - 1 : 1;
    dtv.tm_mday = 1;
}
/* NOTE: Database interface was changed so that if db has null value the tm_year is 0 instead of
   -1900. So this class was updated to compare to zero.
 */
bool
TmDatetime::parse(CC str, APPSTR_LC tl)
{
    char* rv;
    memset(&dtv, 0, sizeof(dtv));
    if (!str || str[0] == 0)
        return true;
    switch (tl) {
    case APPSTR_FI:
        rv = strptime(str, "%d.%m.%Y", &dtv);
        break;
    case APPSTR_EN:
        rv = strptime(str, "%m/%d/%Y", &dtv);
        break;
    default:
        rv = strptime(str, "%Y-%m-%d", &dtv);
    }
    if (rv)
        return true;
    return false;
}
bool
TmDatetime::parseISO(CC str)
{
    if (strlen(str) > 11) {
        if (strptime(str, "%Y-%m-%dT%H:%M:%S", &dtv))
            return true;
    } else if (strptime(str, "%Y-%m-%d", &dtv))
        return true;
    return false;
}
// -------------------------------------------------------------------------------------------------
void
TmDatetime::setDay1()
{
    dtv.tm_mday = 1;
    dtv.tm_hour = 0;
    dtv.tm_min = 0;
    dtv.tm_sec = 0;
}

void
TmDatetime::now()
{
    time_t now = time(0);
    localtime_r(&now, &dtv);
}
void
TmDatetime::thisMonth()
{
    now();
    setDay1();
}
void
TmDatetime::nextMonth(int m_offset)
{
    dtv.tm_mon += m_offset;
    std::mktime(&dtv);
}
// -------------------------------------------------------------------------------------------------
int
TmDatetime::compare(TmDatetime& tg)
{
    if (dtv.tm_year == 0 && tg.dtv.tm_year == 0)
        return 0;
    if (dtv.tm_year == 0)
        return 1;
    if (tg.dtv.tm_year == 0)
        return -1;

    time_t origin = mktime(&dtv);
    time_t target = mktime(&tg.dtv);
    if (origin > target)
        return 1;
    else if (origin < target)
        return -1;
    return 0;
}
// -------------------------------------------------------------------------------------------------
int
TmDatetime::compare(time_t orig)
{
    if (dtv.tm_year == 0)
        return 1;
    time_t myt = mktime(&dtv);
    if (myt < orig)
        return -1;
    if (myt > orig)
        return 1;
    return 0;
}
// -------------------------------------------------------------------------------------------------
const char*
TmDatetime::print(APPSTR_LC tl) const
{
    tstr[0] = 0;
    if (dtv.tm_year == 0) {
        return tstr;
    }
    switch (tl) {
    case APPSTR_FI:
        sprintf(tstr, "%02d.%02d.%d", dtv.tm_mday, dtv.tm_mon + 1, dtv.tm_year + 1900);
        break;
    case APPSTR_EN:
        sprintf(tstr, "%02d/%02d/%d", dtv.tm_mon + 1, dtv.tm_mday, dtv.tm_year + 1900);
        break;
    default:
        sprintf(tstr, "%d-%02d-%02d", dtv.tm_year + 1900, dtv.tm_mon + 1, dtv.tm_mday);
    }
    return tstr;
}
// -------------------------------------------------------------------------------------------------
const char*
TmDatetime::printShort(APPSTR_LC tl) const
{
    tstr[0] = 0;
    if (dtv.tm_year == 0)
        return tstr;
    switch (tl) {
    case APPSTR_FI:
        sprintf(tstr, "%02d.%02d.", dtv.tm_mday, dtv.tm_mon + 1);
        break;
    case APPSTR_EN:
        sprintf(tstr, "%02d/%02d", dtv.tm_mon + 1, dtv.tm_mday);
        break;
    default:
        sprintf(tstr, "%02d-%02d", dtv.tm_mon + 1, dtv.tm_mday);
        break;
    }
    return tstr;
}
// -------------------------------------------------------------------------------------------------
const char*
TmDatetime::printTime(APPSTR_LC tl) const
{
    tstr[0] = 0;
    if (dtv.tm_year == 0) {
        return tstr;
    }
    switch (tl) {
    case APPSTR_FI:
        sprintf(tstr, "%02d.%02d.%d %02d:%02d:%02d", dtv.tm_mday, dtv.tm_mon + 1,
                dtv.tm_year + 1900, dtv.tm_hour, dtv.tm_min, dtv.tm_sec);
        break;
    case APPSTR_EN:
        sprintf(tstr, "%02d/%02d/%d %02d:%02d:%02d", dtv.tm_mon + 1, dtv.tm_mday,
                dtv.tm_year + 1900, dtv.tm_hour, dtv.tm_min, dtv.tm_sec);
        break;
    default:
        sprintf(tstr, "%d-%02d-%02d %02d:%02d:%02d", dtv.tm_year + 1900, dtv.tm_mon + 1,
                dtv.tm_mday, dtv.tm_hour, dtv.tm_min, dtv.tm_sec);
    }
    return tstr;
}
// -------------------------------------------------------------------------------------------------
const char*
TmDatetime::printTimeShort(APPSTR_LC tl) const
{
    tstr[0] = 0;
    if (dtv.tm_year == 0) {
        return tstr;
    }
    switch (tl) {
    case APPSTR_FI:
        sprintf(tstr, "%02d.%02d. %02d:%02d", dtv.tm_mday, dtv.tm_mon + 1, dtv.tm_hour, dtv.tm_min);
        break;
    case APPSTR_EN:
        sprintf(tstr, "%02d/%02d %02d:%02d", dtv.tm_mon + 1, dtv.tm_mday, dtv.tm_hour, dtv.tm_min);
        break;
    default:
        sprintf(tstr, "%02d-%02d %02d:%02d", dtv.tm_mon + 1, dtv.tm_mday, dtv.tm_hour, dtv.tm_min);
    }
    return tstr;
}
// -------------------------------------------------------------------------------------------------
const char*
TmDatetime::printHourMinutes() const
{
    sprintf(tstr, "%02d:%02d", dtv.tm_hour, dtv.tm_min);
    return tstr;
}
// -------------------------------------------------------------------------------------------------
const char*
TmDatetime::printYear() const
{
    if (dtv.tm_year != 0)
        sprintf(tstr, "%4d", dtv.tm_year + 1900);
    else
        tstr[0] = 0;
    return tstr;
}
// -------------------------------------------------------------------------------------------------
CC
TmDatetime::printISODate() const
{
    snprintf(tstr, sizeof(tstr), "%4d-%02d-%02d", dtv.tm_year + 1900, dtv.tm_mon + 1, dtv.tm_mday);
    return tstr;
}
// -------------------------------------------------------------------------------------------------
CC
TmDatetime::printISOTimestamp() const
{
    snprintf(tstr, sizeof(tstr), "%4d-%02d-%02dT%02d:%02d:%02d", dtv.tm_year + 1900, dtv.tm_mon + 1,
             dtv.tm_mday, dtv.tm_hour, dtv.tm_min, dtv.tm_sec);
    return tstr;
}

} // namespace fcgi_frame