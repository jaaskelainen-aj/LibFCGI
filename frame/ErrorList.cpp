/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stdexcept>

#include "../fcgisettings.h"
#include "AppStr.hpp"
#include "ErrorList.hpp"

using namespace std;

namespace fcgi_frame {

uint16_t
ErrorList::next()
{
    while (cursor < items) {
        if (errtbl[cursor]) {
            return errtbl[cursor];
        }
        cursor++;
    }
    return 0;
}
// -------------------------------------------------------------------------------------------------
uint16_t& ErrorList::operator[](const uint16_t ndx)
{
    if (ndx >= 0 && ndx < items)
        return errtbl[ndx];
    throw std::runtime_error("ErrorList::operator - index out of bounds");
}
// -------------------------------------------------------------------------------------------------
void
ErrorList::append(uint16_t val)
{
    int ndx = 0;
    while (errtbl[ndx] && ndx < items)
        ndx++;
    if (ndx < items)
        errtbl[ndx] = val;
}
// -------------------------------------------------------------------------------------------------
uint16_t
ErrorList::size()
{
    uint16_t count = 0;
    int ndx;
    for (ndx = 0; ndx < items; ndx++) {
        if (errtbl[ndx]) {
            count++;
        }
    }
    return count;
}
// -------------------------------------------------------------------------------------------------
void
ErrorList::print(std::ostringstream& html, AppStr* appstr, const char* clsname)
{
    bool firstline = true;
    html << "<div class='" << clsname << "'><p>\n";
    for (int ndx = 0; ndx < items; ndx++) {
        if (errtbl[ndx]) {
            if (firstline)
                firstline = false;
            else
                html << "<br/>\n";
            html << appstr->getsp(errtbl[ndx]);
        }
    }
    html << "</p></div>\n";
}
// -------------------------------------------------------------------------------------------------
void
ErrorList::getText(AppStr* appstr, std::string& buffer)
{
    bool firstline = true;
    for (int ndx = 0; ndx < items; ndx++) {
        if (errtbl[ndx]) {
            if (firstline)
                firstline = false;
            else
                buffer += "; ";
            buffer += appstr->getsp(errtbl[ndx]);
        }
    }
}
// -------------------------------------------------------------------------------------------------
void
ErrorList::getJSON(AppStr* appstr, std::ostream& json)
{
    bool firstline = true;
    json << "{ \"error\":false, \"message\": \"";
    for (int ndx = 0; ndx < items; ndx++) {
        if (errtbl[ndx]) {
            if (firstline)
                firstline = false;
            else
                json << "; ";
            json << appstr->getsp(errtbl[ndx]);
        }
    }
    json << "\"}";
}

} // namespace fcgi_frame