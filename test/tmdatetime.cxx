/***
Compile:
g++ -o tmdatetime tmdatetime.cxx -ggdb -fno-rtti -Wno-reorder -Wnon-virtual-dtor -DUNIT_TEST -DC4S_LOG_LEVEL=1 -I/usr/local/include/cpp4scripts -L/usr/local/lib-d -L../debug -lfcgi -lc4s

Use:
./tmdatetime
 */

#include <iostream>
#include "../libfcgi.hpp"

using namespace std;
using namespace fcgi_frame;

int main(int, char **)
{
    TmDatetime nov(2021, 11);
    nov.setLastDay();
    cout << "Last day 11.2021 = " << nov.print() << '\n';

    TmDatetime dec(2021, 12);
    dec.setLastDay();
    cout << "Last day 12.2021 = " << dec.print() << '\n';

    TmDatetime feb(2022, 2);
    feb.setLastDay();
    cout << "Last day 2.2022 = " << feb.print() << '\n';

    TmDatetime feb4(2024, 2);
    feb4.setLastDay();
    cout << "Last day 2.2024 = " << feb4.print() << '\n';

    cout << "-- Done\n";
    return 0;
}
