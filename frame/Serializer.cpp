/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <fcntl.h>
#include <unistd.h>
#include <string>

#include "SessionBase.hpp"
#include "Serializer.hpp"

using namespace std;

namespace fcgi_frame {

// -------------------------------------------------------------------------------------------------
Serializer::Serializer(SessionBase* base, string& session_dir, TYPE type)
{
    char fname[150];
    strcpy(fname, session_dir.c_str());
    base->catSID(fname);
    strcat(fname, "_@");
    int flags = type == INPUT ? O_RDONLY : O_WRONLY | O_CREAT;
    file_id = open(fname, flags, S_IRUSR | S_IWUSR);
    if (file_id < 0) {
        file_id = 0;
        CS_VAPRT_ERRO("Serializer::Serializer - Unable to open %s. Error %d", fname, errno);
        throw runtime_error("File open error.");
    }
}
// -------------------------------------------------------------------------------------------------
void
Serializer::read(std::string& val)
{
    static char buffer[1024];
    size_t sl;
    ::read(file_id, &sl, sizeof(size_t));
    if (sl) {
        ::read(file_id, buffer, sl);
        val.assign(buffer, sl);
    } else
        val.clear();
}
// -------------------------------------------------------------------------------------------------
void
Serializer::write(const std::string& val)
{
    size_t sl = val.size();
    ::write(file_id, &sl, sizeof(size_t));
    if (sl)
        ::write(file_id, val.c_str(), sl);
}

// -------------------------------------------------------------------------------------------------
void
Serializer::read(bool& val)
{
    char ch;
    ::read(file_id, &ch, 1);
    val = ch == 1 ? true : false;
}
// -------------------------------------------------------------------------------------------------
void
Serializer::write(bool val)
{
    char ch = val ? 1 : 0;
    ::write(file_id, &ch, 1);
}

} // namespace fcgi_frame