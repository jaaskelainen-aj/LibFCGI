/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <string.h>

#include "../fcgisettings.h"
#include "../driver/fcgidriver.hpp"
#include "AppStr.hpp"

const int MAX_READ_BUFFER = 0x800;
const int MAX_MESSAGE = 0x400;
const int MAX_GROUPS = 255;

const int STATE_LF=0;
const int STATE_NUMBER=1;
const int STATE_TEXT=2;
const int STATE_DISCARD=3;
const int STATE_ESC=4;
const int STATE_WS=5;     //White space

const char *NO_MSG = "[NO STR]";

using namespace std;

namespace fcgi_frame
{

const char* AppStr::lc_names[FRAME_LOCALES]={"[]", "en", "fi", "sv" };

// -------------------------------------------------------------------------------------------------
//# ASArray

AppStr::ASArray::ASArray(size_t _count)
{
    count = _count;
    strarr = new char*[count];
    memset(strarr,0,sizeof(char*)*count);
}
AppStr::ASArray::~ASArray()
{
    for(size_t ndx=0; ndx<count; ndx++)
        if(strarr[ndx]) delete strarr[ndx];
    delete[] strarr;
}
void AppStr::ASArray::add(size_t index, const char *str, size_t line)
{
    if(index>=count){
        std::ostringstream err;
        err << "AppStr::ASArray::add - Line="<<line<<". Index out of bounds:"<<std::hex<<index;
        throw runtime_error(err.str());
    }
    if(strarr[index]!=0) {
        std::ostringstream err;
        err << "AppStr::ASArray::add - Line="<<line<<". Duplicate index:"<<std::hex<<index;
        throw runtime_error(err.str());
    }
    strarr[index]=new char[strlen(str)+1];
    strcpy(strarr[index],str);
}
const char* AppStr::ASArray::get(uint16_t ndx)
{
    if(ndx >= count)
        return NO_MSG;
    if(!strarr[ndx])
        return NO_MSG;
    return strarr[ndx];
}

// -------------------------------------------------------------------------------------------------
//# ASGroup

AppStr::ASGroup::ASGroup(size_t count)
{
    groups = new ASArray*[count];
    memset(groups,0,sizeof(ASArray*)*count);
    size = count;
}
AppStr::ASGroup::~ASGroup()
{
    for(size_t ndx=0; ndx<size; ndx++)
        if(groups[ndx]) delete groups[ndx];
    delete[] groups;
}
void AppStr::ASGroup::add(size_t index, ASArray *array, size_t line)
{
    char errbuf[156];
    if(index>=size) {
        sprintf(errbuf, "AppStr::ASGroup::add - Line=%ld. Index out of bounds:%ld", line, index);
        throw runtime_error(errbuf);
    }
    if(groups[index]!=0) {
        sprintf(errbuf, "AppStr::ASGroup::add - Line=%ld. Index out of bounds:%ld", line, index);
        throw runtime_error("AppStr::ASGroup::add - Duplicate index.");
    }
    groups[index]=array;
}
const char* AppStr::ASGroup::getCPtr(uint16_t ndx)
{
    uint8_t grp = ndx >> 8;
    if(grp >= size || groups[grp]==0)
        return NO_MSG;
    uint8_t msg = 0xFF&ndx;
    return groups[grp]->get(msg);
}
size_t AppStr::ASGroup::getTotalCount()
{
    size_t count=0;
    for(size_t ndx=0; ndx<size; ndx++) {
        if(groups[ndx])
            count += groups[ndx]->size();
    }
    return count;
}
// -------------------------------------------------------------------------------------------------
AppStr::AppStr(const char *filepath, const char *locname)
/*! If no exeption flag is on please use the is_ok and get_status functions to determine the success
  of this constructor.
  \param filepath Full path to the message files.
  \param locname Name of the locale.
*/
{
    bool first=true;
    int state;
    ssize_t bytes;
    uint16_t listIndex;
    uint8_t group_no=0, group_prev=0, str_no=0;
    int nIndex,msgIndex;
    char buffer[MAX_READ_BUFFER];
    char msgBuf[MAX_MESSAGE];
    char number[7];
    char errbuf[156];
    ASArray *strings=0;
    size_t str_count[MAX_GROUPS], line;
    memset(str_count,0,sizeof(str_count));

    count_grp = 0;
    lcode = lcname2code(locname);
    // Open the source
    int msg_file = open(filepath, O_RDONLY);
    if(msg_file == -1) {
        sprintf(errbuf, "AppStr::AppStr - Unable to open language file. errno=%d", errno);
        throw runtime_error(errbuf);
    }

 NEXT_ROUND:
    // ..................................................
    // store numbers and messages.
    state = STATE_LF;
    listIndex = 0;
    nIndex = 0;
    msgIndex = 0;
    line = 0;
    do {
        bytes = read(msg_file, buffer, MAX_READ_BUFFER);
        for(int i=0; i<bytes; i++) {

            if(msgIndex>=MAX_MESSAGE) {
                close(msg_file);
                sprintf(errbuf, "AppStr::AppStr - Too long string encountered. Line %ld", line);
                throw runtime_error(errbuf);
            }

            switch(state) {
            case STATE_LF:
                if(fcgi_driver::isHex(buffer[i])) {
                    nIndex = 0;
                    number[nIndex++] = buffer[i];
                    state = STATE_NUMBER;
                }
                else if(buffer[i] == '#')
                    state = STATE_DISCARD;
                else if(buffer[i] != '\n'){
                    close(msg_file);
                    sprintf(errbuf, "AppStr::AppStr - Syntax error: Unrecognized characters. Line %ld", line);
                    throw runtime_error(errbuf);
                }
                line++;
                break;

            case STATE_NUMBER:
                if(!fcgi_driver::isHex(buffer[i])) {
                    if(!nIndex) {
                        close(msg_file);
                        sprintf(errbuf, "AppStr::AppStr - Unrecognized number. Line %ld", line);
                        throw runtime_error(errbuf);
                    }
                    number[nIndex] = 0;
                    listIndex = fcgi_driver::hex2short(number);
                    group_no = listIndex >> 8;
                    str_no = listIndex & 0xFF;
                    if(first) {
                        // ++++++++++++++++++++++++++++++++++++++++++++++++++
                        //cout << "Found msg - "<<number<<"; "<<listIndex<<"; "<<(int)group_no<<"; "<<(int)str_no<<'\n';
                        if(str_count[group_no] < (size_t)(str_no+1))
                            str_count[group_no] = str_no+1;
                    }
                    else {
                        if(group_prev > group_no) {
                            close(msg_file);
                            sprintf(errbuf, "AppStr::AppStr - Syntax error: Groups should be sequentally ordered within a file. Line %ld", line);
                            throw runtime_error(errbuf);
                        }
                        if(group_prev < group_no) {
                            // ++++++++++++++++++++++++++++++++++++++++++++++++++
                            //cout << "New group "<<(int)group_prev<<" with "<<strings->size()<<" strings. Next "<<(int)group_no<<'\n';
                            groups->add(group_prev,strings,line);
                            strings = new ASArray(str_count[group_no]);
                            group_prev = group_no;
                        }
                    }
                    state = STATE_WS;
                }
                else {
                    number[nIndex++] = buffer[i];
                    if(nIndex == sizeof(number)) {
                        close(msg_file);
                        sprintf(errbuf, "AppStr::AppStr - Syntax error: String number too large. Line %ld", line);
                        throw runtime_error(errbuf);
                    }
                }
                break;

            case STATE_WS:
                if(buffer[i] =='"')
                {
                    msgIndex = 0;
                    state = STATE_TEXT;
                }
                else if(buffer[i]!=' ' && buffer[i]!='\t') {
                    close(msg_file);
                    sprintf(errbuf, "AppStr::AppStr - Syntax error: Number defined but start of string missing. Line %ld", line);
                    throw runtime_error(errbuf);
                }
                break;

            case STATE_DISCARD:
                if(buffer[i] == '\n')
                    state = STATE_LF;
                break;

            case STATE_TEXT:
                if(buffer[i] == '"') {
                    msgBuf[msgIndex] = 0;
					if(!first) {
                        // ++++++++++++++++++++
                        // cout << "Adding: ["<<hex<<(uint16_t)str_no<<dec<<"] = "<<msgBuf<<'\n';
						strings->add(str_no,msgBuf,line);
                    }
                    state = STATE_DISCARD;
                }
                else if(buffer[i] == '\\')
                    state = STATE_ESC;
                else if(buffer[i] == '\n' || buffer[i] == '\r') {
                    close(msg_file);
                    sprintf(errbuf, "AppStr::AppStr - Mismatched quotation marks. Line %ld", line);
                    throw runtime_error(errbuf);
                }
                else {
                    msgBuf[msgIndex++] = buffer[i];
                }
                break;

            case STATE_ESC:
                if(buffer[i] == 'n')
                    msgBuf[msgIndex++] = '\n';
                else if(buffer[i] == 't')
                    msgBuf[msgIndex++] = '\t';
                else if(buffer[i] == '\"')
                    msgBuf[msgIndex++] = '\"';
                else
                    msgBuf[msgIndex++] = ' ';
                state = STATE_TEXT;
                break;
            }
        }
    }while(bytes);
    if(first) {
        uint8_t ndx;
        count_grp = group_no+1;
        // ++++++++++++++++++++++++++++++++++++++++++++++++++
        //cout << "Max group number:"<<(int)count_grp<<'\n';
        //cout << "String counts:\n";
        //for(ndx=0; ndx<MAX_GROUPS; ndx++) {
        //    if(str_count[ndx]>0)
        //        cout << (int)ndx << " - "<<str_count[ndx]<<'\n';
        //}
        // ++++++++++++++++++++++++++++++++++++++++++++++++++
        groups = new ASGroup(count_grp);
        for(ndx=0; str_count[ndx]==0 && ndx<MAX_GROUPS; ndx++)
            groups->add(ndx,0,line);
        if(ndx==MAX_GROUPS) {
            close(msg_file);
            throw runtime_error("AppStr::AppStr - No strings found from the source file.");
        }
        group_prev = ndx;
        strings = new ASArray(str_count[ndx]);
        // Rewind
        lseek(msg_file, 0, SEEK_SET);
        first = false;
        goto NEXT_ROUND;
    }
	close(msg_file);
    groups->add(group_no,strings,line);
}

// -------------------------------------------------------------------------------------------------
AppStr::~AppStr()
/*! This function will release all memory acquired by the getmsg-functions.
*/
{
    delete groups;
}
// -------------------------------------------------------------------------------------------------
APPSTR_LC AppStr::lcname2code(const char *locname)
{
    int ndx;
    if(!locname || !locname[0])
        return APPSTR_NONE;
    for(ndx=0; ndx<APPSTR_MAX_LC; ndx++) {
        if(lc_names[ndx][0]==locname[0] && lc_names[ndx][1]==locname[1])
            break;
    }
    if(ndx==APPSTR_MAX_LC)
        return APPSTR_NONE;
    return (APPSTR_LC)ndx;
}
// -------------------------------------------------------------------------------------------------
const char* AppStr::code2lcname(APPSTR_LC lc)
{
    if(lc>0 && lc<APPSTR_MAX_LC)
        return lc_names[lc];
    return lc_names[0];
}
// -------------------------------------------------------------------------------------------------
const char* AppStr::getLName()
{
    return lc_names[lcode];
}

} // namespace fcgi_frame