#include <iostream>
#include <cpp4scripts.hpp>

using namespace std;
using namespace c4s;

int main(int argc, char **argv)
{
    path_list sources(path("./"), "^t_.*cxx$");
    if(!sources.size()) {
        cout<<"Nothing found!\n";
        return 1;
    }
    BUILD flag(BUILD::BIN|BUILD::DEB);
    for(path_iterator src=sources.begin(); src!=sources.end(); src++) {
        compiled_file cf(*src, src->get_base_plain());
        if(cf.outdated()) {
            path_list one;
            one += *src;
            builder_gcc gcc(&one, src->get_base_plain().c_str(), &cout, flag);
            gcc.add_comp("-O0 -Wall -fexceptions -pthread -fuse-cxa-atexit -std=c++14 "\
                         "-DC4S_LOG_LEVEL=1 "                               \
                         "-I/usr/local/include/menaconlib -I/usr/local/include/cpp4scripts");
            gcc.add_link("-lcurl -L/usr/local/lib-d -lmenacon -lc4s");
            cout<<src->get_base();
            if(gcc.build()) {
                cout<<" failed.\n";
                break;
            }
            else cout<<" OK\n";
        }
    }
    cout<<"Done\n";
    return 0;
}
