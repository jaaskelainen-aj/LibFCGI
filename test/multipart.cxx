/***
Compile:
g++ -o multipart multipart.cxx -ggdb -fno-rtti -Wno-reorder -Wnon-virtual-dtor -DUNIT_TEST -DC4S_LOG_LEVEL=1 -I/usr/local/include/cpp4scripts -L/usr/local/lib-d -L../debug -lfcgi -lc4s -lmenacon

./multipart req-spool_2files WebKitFormBoundaryxhKGcEjU1960vmmk
 */

#include <stdio.h>
#include "../libfcgi.hpp"

extern FILE *trace;

class RequestUT : public fcgi::Request
{
public:
    RequestUT() : Request() {}
    bool parseMP(const char *fn, const char *tag);

private:
    bool prepareMP(const char *fname, const char *mptag);
};

// Unit Test variant of fcgi::Request::openMPSpool(const char *data, int len)
bool RequestUT::prepareMP(const char *fname, const char *mptag)
{
    fd_spool = open(fname, O_RDONLY, S_IRUSR|S_IRGRP|S_IROTH);
    if(fd_spool == -1) {
        printf("Request::openMPSpool - unable to open spool file for multipart imput. Errno %d.\n",errno);
        return false;
    }
    flags.set(fcgi::FLAG_MULTIP);
    strcpy(boundary,"\r\n------");
    strcat(boundary, mptag);
    bound_len = strlen(boundary);
    spool_size = 103538;

    return true;
}

bool RequestUT::parseMP(const char *fn, const char *tag)
{
    if(!prepareMP(fn, tag) ) {
        return false;
    }
    if(!processSpool()) {
        return false;
    }
    for(size_t nx=0; nx<params.size(); nx++) {
        printf("%lx \t\t%s\n", params.getKey(nx), params.getValue(nx));
    }
    return true;
}

int main(int argc, char **argv)
{
    RequestUT req;
    if(argc != 3) {
        printf("Missing spool and boundary parameters.\n");
        return 1;
    }
    trace = stdout;
    if( !req.parseMP(argv[1], argv[2]) ) {
        printf("ParseMP failed\n");
        return 1;
    }
    return 0;
}
