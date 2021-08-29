/*
g++ -fno-rtti -fexceptions -fuse-cxa-atexit -pthread -ggdb -O0 -std=c++14 -o t_process_spool -D_DEBUG -D__STDC_LIMIT_MACROS t_process_spool.cpp -L./debug -L/usr/local/lib-d -lfcgi -lc4s
*/

#include <fcntl.h>
#include <unistd.h>
#include "iostream"
using namespace std;
#include "libfcgi.hpp"

int main(int argc, char **argv)
{
    if(argc<2) {
        cout<<"Missing spool file argument: t_process_spool [spool file]\n";
        return 1;
    }

    fcgi::Request req;
    if(!req.openTestSpool(argv[1], "-----------------------------16461220603416381191688077602")) {
        cout<<"Unable to open spool file\n";
        return 2;
    }
    req.processTestSpool();

    return 0;
}
