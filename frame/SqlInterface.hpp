/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_SQL_HPP
#define FCGI_SQL_HPP

#include <sstream>
#include <directdb.hpp>
#include <cpp4scripts.hpp>

namespace fcgi_frame
{

class SqlInterface
{
public:
    SqlInterface(ddb::Database *db) { if(db) rs = db->CreateRowSet(); else rs=0; count=0; }
    virtual ~SqlInterface() { if(rs) delete rs; }
    bool Next() { if(rs->GetNext()>0) { count++; return true; } return false; }
    void Quit() { rs->Reset(); }
    int GetCount() { return count; }
    std::string GetLastQuery() { return rs->query.str(); }

protected:
    int count;
    ddb::RowSet *rs;
};

} // namespace fcgi_frame

#endif