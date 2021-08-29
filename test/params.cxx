/***
Compile:
g++ -o params params.cxx -ggdb -fno-rtti -Wno-reorder -Wnon-virtual-dtor -DUNIT_TEST -DC4S_LOG_LEVEL=1 -I/usr/local/include/cpp4scripts -L/usr/local/lib-d -L../debug -lfcgi -lc4s -lmenacon

 */

#include <stdio.h>

#include "../libfcgi.hpp"

extern FILE *trace;

void strToHex()
{
    fcgi::PACK64 pack[3];

    pack[0].value = 0;
    pack[1].value = 0;
    pack[2].value = 0;

    strncpy(pack[0].str, "attt0", 5);
    strncpy(pack[1].str, "att31", 5);
    strncpy(pack[2].str, "atu20", 5);

    printf("pack 1 = %lX\n", pack[0].value);
    printf("pack 2 = %lX\n", pack[1].value);
    printf("pack 3 = %lX\n", pack[2].value);
/*
 */    
}

void findParam()
{
    fcgi::PACK64 fnd;
    fnd.value = 0;
    strncpy(fnd.str, "att", 7);
    printf("Find 'att' = %lX\n", fnd.value);
    fcgi::ParamData pd(0x100);

    pd.add("attt0", 5, "OK1");
    pd.add("att31", 5, "OK2");
    pd.add("atu20", 5, "NOK");

    size_t nx;
    for(bool c=pd.findFirst("att", &nx); c; c=pd.findNext(&nx)) {
        printf("Found at %ld = %s\n", nx, pd.getValue(nx));
    }
}

int main(int argc, char **argv)
{
    trace = stdout;

    strToHex();
    findParam();

    /*
    fcgi::PACK64 a, b;
    a.value = 0;
    b.value = 0; 
    a.str[0] = 'a';
    a.str[1] = 'b';
    b.str[0] = 'a';
    b.str[1] = 'c';

    printf("a = %lX  b = %lX\n", a.value, b.value);

    uint64_t k = 0x61;
    uint64_t m = 0xF;
    for(int x=0; x<16; x++) {
        if( (k & ~m) == 0)
            break;
        m=(m<<4)|0xF;
    }
    m=~m;
    printf("%lX %lX = %lX\n",k, m, a.value & m);

    m = ~k;
    printf("%lX %lX = %lX\n",k, m, (m-1)+k);
    */
    return 0;
}
