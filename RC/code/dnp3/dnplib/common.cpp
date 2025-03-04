//
// $Id: common.cpp 4 2007-04-10 22:55:27Z sparky1194 $
//
// Copyright (C) 2007 Turner Technolgoies Inc. http://www.turner.ca
//
// Permission is hereby granted, free of charge, to any person 
// obtaining a copy of this software and associated documentation 
// files (the "Software"), to deal in the Software without 
// restriction, including without limitation the rights to use, 
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following 
// conditions:
//      
// The above copyright notice and this permission notice shall be 
// included in all copies or substantial portions of the Software. 
//      
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
// OTHER DEALINGS IN THE SOFTWARE.

#include <assert.h>
#include <stdlib.h>
#include <iostream>
//#include "ns3/wrap.h"
#include "ns3/common.hpp"

using namespace std;
namespace ns3 {

static const char* dnpHex = "0123456789abcdef";

const char* x11_license()
{
    const char* s =
    "<p>"
    "<b>Copyright (C) 2007 Turner Technologies Inc.</b> "
    "<a href=\"http://www.turner.ca\">www.turner.ca</a>"
    "</p>"
    "<p>"
    "Permission is hereby granted, free of charge, to any person "
    "obtaining a copy of this software and associated documentation "
    "files (the \"Software\"), to deal in the Software without "
    "restriction, including without limitation the rights to use, "
    "copy, modify, merge, publish, distribute, sublicense, and/or sell"
    " copies of the Software, and to permit persons to whom the "
    "Software is furnished to do so, subject to the following "
    "conditions:"
    "<br>"
    "<br>"
    "The above copyright notice and this permission notice shall be "
    "included in all copies or substantial portions of the Software. "
    "<br>"
    "<br>"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANc[Y KIND,"
    " EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES "
    "OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND "
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT "
    "HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, "
    "WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING "
    "FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR "
    "OTHER DEALINGS IN THE SOFTWARE."
    "</p>";

    return s;
}


char* hex_repr( const Bytes& ab, char* buf, unsigned int buf_len)
{
    unsigned int i,j;
    // two chars for hex digit and one for space + one for null
    if ((ab.size()*3+1) < buf_len)
	// only print at max buf_len chars
	buf_len = ab.size()*3 + 1;
    for (i=0, j=0; i<ab.size(); i++, j++)
    {
        buf[j]= dnpHex[ (ab[i]>>4) & 0x0f];
        j++;
        buf[j]= dnpHex[ ab[i] & 0x0f];
        j++;
        buf[j] = ' ';
    }

    buf[j] = '\0'; /* string terminator */

    return buf;
}

void randGen(Bytes& data, int len)
{
    unsigned int i;

    // Note, this does not meet the random number generation standard
    // specified in FIPS186-2
    // for(i=0; i<len; i++)
    // 	   data.push_back(rand() & 0xff);

    // create a seed value
    unsigned char seed[16];
    for(i=0; i<sizeof(seed); i++)
	seed[i] = (rand() & 0xff);

    unsigned int randLen; // must be a multiple of 40 and at least len
    if (len % 40 == 0)
	randLen = len;
    else
	randLen = len + (40 - (len % 40));

    unsigned char randData[randLen];

    //gen_rand(seed, sizeof(seed), randData, randLen); //To check

    data = Bytes( randData, randData + len);
}

void appendUINT48(Bytes& data, uint64_t val)
{
    data.push_back((val & 0x00000000000000ffLLU));
    data.push_back((val & 0x000000000000ff00LLU) >> 8);
    data.push_back((val & 0x0000000000ff0000LLU) >> 16);
    data.push_back((val & 0x00000000ff000000LLU) >> 24);
    data.push_back((val & 0x000000ff00000000LLU) >> 32);
    data.push_back((val & 0x0000ff0000000000LLU) >> 40);
}

void appendUINT32(Bytes& data, uint32_t val)
{
    data.push_back((val & 0x000000ff));
    data.push_back((val & 0x0000ff00) >> 8);
    data.push_back((val & 0x00ff0000) >> 16);
    data.push_back((val & 0xff000000) >> 24);
}

void appendUINT24(Bytes& data, uint32_t val)
{
    data.push_back((val & 0x000000ff));
    data.push_back((val & 0x0000ff00) >> 8);
    data.push_back((val & 0x00ff0000) >> 16);
}

void appendUINT16(Bytes& data, uint16_t val)
{
    data.push_back((val & 0x00ff));
    data.push_back((val & 0xff00) >> 8);
}

void appendUINT8(Bytes& data, uint8_t val)
{
    data.push_back(val);
}

void appendBytes(Bytes& data, const Bytes& val)
{
    data.insert(data.end(), val.begin(), val.end());
}



void appendINT32(Bytes& data, int32_t val)
{
    appendUINT32(data, (uint32_t) val);
}

void appendINT16(Bytes& data, int16_t val)
{
    appendUINT16(data, (uint16_t) val);
}

void appendFloat(Bytes& data, float val) {
    unsigned char* c = reinterpret_cast<unsigned char*>(&val);
    //cout << "I will be adding " << val << endl;
    //cout << "The deque value is " << data.size() << endl;
    /*if (data.size() > 4){
      data[-4] = c[0];
      data[-3] = c[1];
      data[-2] = c[2];
      data[-1] = c[3];
    }*/
    //if(val == 0.0){
    //  data.clear();
    //}

    data.push_back(c[0]);
    data.push_back(c[1]);
    data.push_back(c[2]);
    data.push_back(c[3]);
}

uint64_t removeUINT48(Bytes& data) noexcept(true)
{
    uint64_t val;

    if(data.size() < 6)
	//throw(__LINE__);

    val = ((uint64_t) data[5]) << 40;
    val |= ((uint64_t) data[4]) << 32;
    val |= ((uint64_t) data[3]) << 24;
    val |= ((uint64_t) data[2]) << 16;
    val |= ((uint64_t) data[1]) << 8;
    val |= ((uint64_t) data[0]);
    data.pop_front();
    data.pop_front();
    data.pop_front();
    data.pop_front();
    data.pop_front();
    data.pop_front();

    return val;
}

uint32_t removeUINT32(Bytes& data) noexcept(true)
{
    uint32_t val;

    if(data.size() < 4)
	//throw(__LINE__);

    val = data[3] << 24;
    val |= data[2] << 16;
    val |= data[1] << 8;
    val |= data[0];
    data.pop_front();
    data.pop_front();
    data.pop_front();
    data.pop_front();

    return val;
}

uint32_t removeUINT24(Bytes& data) noexcept(true)
{
    uint32_t val;

    if(data.size() < 3)
	//throw(__LINE__);

    val = data[2] << 16;
    val |= data[1] << 8;
    val |= data[0];
    data.pop_front();
    data.pop_front();
    data.pop_front();

    return val;
}

uint16_t removeUINT16(Bytes& data) noexcept(true)
{
    uint16_t val;

    if(data.size() < 2)
	//throw(__LINE__);

    val = data[1] << 8;
    val |= data[0];
    data.pop_front();
    data.pop_front();

    return val;
}

uint8_t removeUINT8(Bytes& data) noexcept(true)
{
    uint8_t val;

    if(data.size() < 1)
	//throw(__LINE__);

    val = data[0];
    data.pop_front();
    return val;
}


int32_t removeINT32(Bytes& data)  noexcept(true)
{
    return (int32_t) removeUINT32(data);
}

int16_t removeINT16(Bytes& data) noexcept(true)
{
    return (int16_t) removeUINT16(data);
}



void moveBytes(Bytes& data, Bytes& val, unsigned int len) noexcept(true)
{
    if(data.size() < len)
	//throw(__LINE__);
    val.insert(val.end(), data.begin(), data.begin() + len);

    data.erase(data.begin(), data.begin() + len);
}

}
