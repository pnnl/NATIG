//
// $Id: factory.hpp 8 2007-04-12 17:25:30Z sparky1194 $
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

#ifndef FACTORY_H
#define FACTORY_H

#include <vector>
#include <map>
#include "ns3/object.hpp"
#include "ns3/event_interface.hpp"
#include "ns3/stats.hpp"

namespace ns3 {




class ObjectHeader
{
public:

    enum ObjectGroup  {
	// these function codes are valid only for requests
    DEVICE_ATTRIBUTES                      = 0x00,
	BINARY_INPUTS                          = 0x01,
	BINARY_INPUT_EVENTS                    = 0x02,
	DBIT_BINARY_INPUTS                     = 0x03,
	DBIT_BINARY_INPUT_EVENTS               = 0x04,
	BINARY_OUTPUTS                         = 0x0A,
	BINARY_OUTPUT_EVENTS                   = 0x0B,
	BINARY_OUTPUT_COMMANDS                 = 0x0C,
	BINARY_OUTPUT_COMMAND_EVENTS           = 0x0D,
	COUNTERS	                           = 0x14,
	FROZEN_COUNTERS                        = 0X15,
	COUNTER_EVENTS                         = 0x16,
	FROZEN_COUNTER_EVENTS                  = 0x17,
	ANALOG_INPUTS                          = 0x1E,
	FROZEN_ANALOG_INPUTS                   = 0x1F,
	ANALOG_INPUT_EVENTS                    = 0x20,
	FROZEN_ANALOG_INPUT_EVENTS             = 0x21,
	ANALOG_INPUT_REPORTING_DEADBANDS       = 0x22,
	ANALOG_OUTPUT_STATUS                   = 0x28,
	ANALOG_OUTPUTS                         = 0x29,
	ANALOG_OUTPUT_EVENTS                   = 0x2A,
	ANALOG_OUTPUT_COMMAND_EVENTS           = 0x2B,
	TIME_AND_DATE                          = 0x32
//TODO: Need to finish defining all the group numbers
    };

    // object prefix codes
    static const uint8_t PACKED_WITHOUT_A_PREFIX = 0;
    static const uint8_t ONE_OCTET_INDEX         = 1;
    static const uint8_t TWO_OCTET_INDEX         = 2;
    static const uint8_t FOUR_OCTET_INDEX        = 3;
    static const uint8_t ONE_OCTET_SIZE          = 4;
    static const uint8_t TWO_OCTET_SIZE          = 5;
    static const uint8_t FOUR_OCTET_SIZE         = 6;

    // range specifier codes
    static const uint8_t ONE_OCTET_START_STOP_INDEXES               = 0;
    static const uint8_t TWO_OCTET_START_STOP_INDEXES               = 1;
    static const uint8_t FOUR_OCTET_START_STOP_INDEXES              = 2;
    static const uint8_t ONE_OCTET_START_STOP_VIRTUAL_ADDRESSES     = 3;
    static const uint8_t TWO_OCTET_START_STOP_VIRTUAL_ADDRESSES     = 4;
    static const uint8_t FOUR_OCTET_START_STOP_VIRTUAL_ADDRESSES    = 5;
    static const uint8_t NO_RANGE_FIELD                             = 6;
    static const uint8_t ONE_OCTET_COUNT_OF_OBJECTS                 = 7;
    static const uint8_t TWO_OCTET_COUNT_OF_OBJECTS                 = 8;
    static const uint8_t FOUR_OCTET_COUNT_OF_OBJECTS                = 9;
    static const uint8_t ONE_OCTET_COUNT_OF_OBJECTS_VARIABLE_FORMAT =0xb;

    // the object header size is dependent on the qualifier field
    ObjectHeader(uint8_t  group=0,
		 uint8_t  variation=0,
		 uint8_t  qualifier=0,
         uint32_t count=0,
		 uint32_t startIndex=0,
		 uint32_t stopIndex=0);

    // text representation of the object
    // must pass in buffer and length of buffer
    char* str( char* buf, int len) const;

    // adds the object header to the end of the data
    void encode( Bytes& data) const;

    // builds the object header from the data pyrovided
    // the bytes used for the object header are removed from the data
    void decode( Bytes& data, Stats& stats);

    uint8_t       grp;
    uint8_t       var;
    uint8_t       qual;
    uint8_t       indexSize;
    uint8_t       rangeSpecifier;
    uint32_t      count;
    uint32_t      start;
    uint32_t      stop;
};

class Factory
{
public:
    typedef uint16_t ObjectKey;
    typedef std::map<Factory::ObjectKey, DnpObject*> DnpObjectMap;
    typedef std::map<Factory::ObjectKey, DnpFloatObject*> DnpFloatObjectMap;
    Factory(EventInterface* eventInterface_p);

    // only returns the last object parsed to handle special cases
    // in most cases this method will perform the action required
    // for the destramed object and then overwrite it with the next
    DnpObject* decode(const ObjectHeader& oh, Bytes& data,
			DnpAddr_t addr,
			Stats& stats);

    DnpFloatObject* decode_float(const ObjectHeader& oh, Bytes& data,
			     DnpAddr_t addr,
			     Stats& stats) ;
    void setCTO( DnpTime_t newCTO);

private:

    // called by the contructor to check parts of the object and factory code
    void selfTest();

    static ObjectKey key(uint8_t group, uint8_t variation);
    static DnpObjectMap objectMap;
    static DnpFloatObjectMap fMap;

    void createObjects(uint8_t grp, uint8_t var, Bytes& data,
		       uint32_t startIndex, uint32_t stopIndex,
		       DnpAddr_t addr,
		       Stats& stats,
		       uint32_t objectSize=0);

    void create_float(uint8_t grp, uint8_t var, Bytes& data,
		       uint32_t startIndex, uint32_t stopIndex,
		       DnpAddr_t addr,
		       Stats& stats,
		       uint32_t objectSize=0) ;

    EventInterface*   db_p;
    DnpObject*        lastObjectParsed;
    DnpFloatObject*   lastFloatObjectParsed;
    DnpTime_t         cto;

};
}//namespace ns3
#endif
