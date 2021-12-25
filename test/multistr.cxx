/***
Compile:
g++ -o multistr multistr.cxx -ggdb -fno-rtti -Wno-reorder -Wnon-virtual-dtor -DUNIT_TEST -DC4S_LOG_LEVEL=1 -I/usr/local/include/cpp4scripts -L/usr/local/lib-d -L../debug -lfcgi -lc4s

Use:
./multistr
 */

#include <iostream>
#include "../libfcgi.hpp"

using namespace std;
using namespace fcgi_frame;

int main(int, char **)
{
    MultiStr ms;
    const char* str;
    if(!ms.set("params1=value1&   param2  =  value2 &param3=value3", "&=", " ")) {
        cout << "Unable to parse first test\n";
        return 1;
    }
    int ndx=0;
    for (str = ms.getFirst(); str; str=ms.getNext()) {
        if ((ndx&1)==0)
            cout << str << " = ";
        else
            cout << str << '\n';
        ndx++;
    }
    cout << '\n';

    if(!ms.set("  Value1  ,Value2  ,Value3", ",", " ")) {
        cout << "Unable to parse second test\n";
        return 1;
    }
    for (str = ms.getFirst(); str; str=ms.getNext()) {
        cout << str  << '\n';
    }
    cout << "-- Done\n";
    return 0;
}
