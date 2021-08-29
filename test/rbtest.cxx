#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;
#include "../driver/RingBuffer.hpp"

streamsize total=0;

void input(ifstream &txtin, fcgi_driver::RingBuffer &rb)
{
    char rbuf[100];
    int rin = rand()%100+1;
    txtin.read(rbuf,rin);
    streamsize ract = txtin.gcount();
    total += ract;

    size_t wact = rb.write(rbuf,ract);
    cout << "read - "<<ract << ' '<<' '<<wact;
    if(wact < (size_t)ract)
        txtin.seekg(wact-ract,ios::cur);
    if(rb.is_eof()) {
        cout << " EOF\n";
        return;
    }
    cout << endl;
}

void output(ofstream &txtout, fcgi_driver::RingBuffer &rb)
{
    char wbuf[100];
    int wout = rand()%100+1;
    size_t avail = rb.size();
    size_t ract = rb.read(wbuf,wout);
    txtout.write(wbuf,ract);
    cout << "write - "<<wout<<' '<<ract<<' '<<avail<<'\n';
}

int test2(const char *fname)
{
    unsigned char buffer[0x11000], hdr[8];
    fcgi_driver::RingBuffer rb(0x11000);
    ssize_t cr1, cr2, cw, max, cap;

    int fdr = open(fname,O_RDONLY);
    int fdw = open("rbtest.out", O_CREAT|O_WRONLY|O_TRUNC);
    if(fdr<0 || fdw<0) {
        cout << "Unable to open files\n";
        return 1;
    }
    fchmod(fdw,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    do {
        cap = rb.capacity();
        max = (size_t)cap<sizeof(buffer) ? cap : sizeof(buffer);
        cr1 = read(fdr,buffer,max);

        if(cr1) {
            cw = rb.write(buffer,(size_t)cr1);
            rb.peek(hdr,8);
            write(fdw,hdr,8);
            rb.discard(8);
        }
        cout << "Read: cap:"<<cap<<"; read:"<<cr1<<"; write:"<<cw<<'\n';

        cap = rb.size();
        cr2 = rb.read(buffer,(size_t)cap);
        cw = write(fdw,buffer,(size_t)cr2);
        cout << "Write: cap:"<<cap<<"; read:"<<cr2<<"; write:"<<cw<<'\n';
    } while(cr1>0);
    close(fdr);
    close(fdw);
    return 0;
}

int main(int argc, char **argv)
{
    if(argc!=3) {
        cout << "Missing parameters\n";
        return 1;
    }
    if(argv[1][0]=='F')
        return test2(argv[2]);

    fcgi_driver::RingBuffer rb(100);
    srand(time(0));


    ifstream txtin(argv[1]);
    if(!txtin) {
        cout << "Unable to open: "<<argv[1]<<'\n';
        return 1;
    }
    ofstream txtout(argv[2]);
    if(!txtout) {
        txtin.close();
        cout << "Unable to open: "<<argv[2]<<'\n';
        return 2;
    }

    while(!txtin.eof()) {
        if(!rb.is_eof())
            input(txtin,rb);
        output(txtout,rb);
    }
    cout << "Read complete\n";
    rb.dump(cout);
    int orcount=0;
    while(rb.size()>0 && orcount<30) {
        output(txtout,rb);
        orcount++;
    }
    txtin.close();
    txtout.close();

    return 0;
}

