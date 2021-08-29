#include <iostream>
#include <cpp4scripts.hpp>

typedef const char* CC;

#include "TmDatetime.hpp"
#include "TmDatetime.cpp"

using namespace std;
using namespace c4s;

int main(int argc, char **argv)
{
    TmDatetime day;

    day.thisMonth();
    TmDatetime next(day);
    next.nextMonth();

    cout<<"This month: "<<day.print()<<'\n';
    cout<<"Next month: "<<next.print()<<'\n';
    return 0;
}
