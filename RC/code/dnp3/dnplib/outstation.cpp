//
// $Id: outstation.cpp 17 2007-04-13 15:30:36Z sparky1194 $
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


// The Oustation class contains all of the outstation specific DNP
// application layer code.
//
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ns3/object.hpp"
#ifdef SECURITY_ENABLED
#include "ns3/security.hpp"
#endif
#include "ns3/outstation.hpp"
#include "ns3/log.h"
#include "ns3/factory.hpp"
#include "event_interface.hpp"
#ifdef FNCS
#include <fncs.hpp>
#include <exception>
#endif
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cassert>
#include <map>
#include <pthread.h>
#include <cerrno> // for ETIMEDOUT

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("outstation");

const char* Outstation::stateStrings[ Outstation::NUM_STATES] =
{
    "Idle",
    "Waiting For Event Confirm"
};

class CSVRow
{
    public:
        std::string const& geti(std::size_t index) const
        {
            return m_data[index];
        }
        std::size_t size() const
        {
            return m_data.size();
        }
        void readNextRow(std::istream& str)
        {
            std::string         line;
            std::getline(str,line);

            std::stringstream   lineStream(line);
            std::string         cell;

            m_data.clear();
            while(std::getline(lineStream,cell,','))
            {
                m_data.push_back(cell);
            }
        }
    private:
        std::vector<std::string>    m_data;
};

Outstation::Outstation( OutstationConfig& outstationConfig,
			Datalink::DatalinkConfig&     datalinkConfig,
			EventInterface*               eventInterface_p,
			TimerInterface*               timerInterface_p)
  : BaseApplication( outstationConfig.debugLevel_p, outstationConfig.addr,
		 outstationConfig.userNum, datalinkConfig,
		 eventInterface_p, timerInterface_p ),
#ifdef SECURITY_ENABLED
    secAuth(this),
#endif
    masterAddr(outstationConfig.masterAddr),
    broadcast(false)
{
    LogComponentEnable ("outstation", LOG_LEVEL_INFO);

    // create stationInfo and stationInfoMap to init the transport function
    StationInfo stationInfo;
    stationInfo.session_p = &session;
    stationInfo.stats_p   = &stats;
    stationInfo.addr      = masterAddr;
   

    //std::cout << "I AM SETTING THE STATIONMAP!!!!!! " << masterAddr << "\n";


    StationInfoMap stationInfoMap;
    // as an outstation device we accept broadcast messages
    stationInfoMap[ masterAddr] = stationInfo;

    // initialize the transport function
    tf_p = new TransportFunction(dl, stationInfoMap);


    Stats::Element temp[] =
    {
	// the following stats are incremented by the TransportFunction class.
	// We are initializing them here because it is simpler to
	// have one stats class for the outstation regardless of which
	// class is incrementing the stat
	{ TransportStats::TX_FRAGMENT,"Tx Fragments"  ,Stats::NORMAL,  0, 0 },
	{ TransportStats::TX_SEGMENT,"Tx Segments"    ,Stats::NORMAL,  0, 0 },
	{ TransportStats::RX_FRAGMENT,"Rx Fragments"  ,Stats::NORMAL,  0, 0 },
	{ TransportStats::RX_SEGMENT,"Rx Segments"    ,Stats::NORMAL,  0, 0 },
	{ TransportStats::RX_UNAUTH_SEG,"Rx Unauth Seg",Stats::ABNORMAL,0, 0 },
	{ TransportStats::RX_ROUGE_SEG,"Rx Rouge Seg"  ,Stats::ABNORMAL,0, 0 },
	{ TransportStats::RX_BAD_TH_SEQ_NUM,"Rx Bad Th SeqNum",
	                                               Stats::ABNORMAL,0, 0 },
	// Normal stats
        { STATE,                "State",             Stats::NORMAL,IDLE,IDLE },
        { TX_RESPONSE,          "Tx Response",         Stats::NORMAL,  0, 0 },
        { TX_NULL_RESPONSE,     "Tx Null Repsonse",    Stats::NORMAL,  0, 0 },
        { TX_CONFIRM,           "Tx Confirm",          Stats::NORMAL,  0, 0 },
        { TX_UNSOLICITED,       "Tx Unsolicited",      Stats::NORMAL,  0, 0 },
        { TX_SELECT_RESP,       "Tx Select Resp",      Stats::NORMAL,  0, 0 },
        { TX_OPERATE_RESP,      "Tx Operate Resp",     Stats::NORMAL,  0, 0 },
        { TX_FRAGMENT,          "Tx Fragment",         Stats::NORMAL,  0, 0 },
        { TX_TIME_DELAY_FINE,   "Tx Time Delay Fine",  Stats::NORMAL,  0, 0 },
        { TX_TIME_DELAY_COARSE, "Tx Time Delay Coasrse",Stats::NORMAL,  0, 0 },
        { RX_WRITE_TIME,        "Rx Write Time",       Stats::NORMAL,  0, 0 },
        { RX_DELAY_MEASUREMENT, "Rx Delay Measure",    Stats::NORMAL,  0, 0 },
        { RX_CONFIRM,           "Rx Confirm",          Stats::NORMAL,  0, 0 },
        { RX_CLASS_O_POLL,      "Rx Class 0 Poll",     Stats::NORMAL,  0, 0 },
        { RX_CLASS_1_POLL,      "Rx Class 1 Poll",     Stats::NORMAL,  0, 0 },
        { RX_CLASS_2_POLL,      "Rx Class 2 Poll",     Stats::NORMAL,  0, 0 },
        { RX_CLASS_3_POLL,      "Rx Class 3 Poll",     Stats::NORMAL,  0, 0 },
        { RX_READ,              "Rx Read",             Stats::NORMAL,  0, 0 },
        { RX_WRITE,             "Rx Write",            Stats::NORMAL,  0, 0 },
        { RX_SELECT,            "Rx Select",           Stats::NORMAL,  0, 0 },
        { RX_OPERATE,           "Rx Operate",          Stats::NORMAL,  0, 0 },
        { RX_DIR_OP,            "Rx Direct Operate",   Stats::NORMAL,  0, 0 },
        { RX_DIR_OP_NO_ACK,     "Rx Dir Op No Ack",    Stats::NORMAL,  0, 0 },
        { RX_BROADCAST,         "Rx Broadcast",        Stats::NORMAL,  0, 0 },

	// Abnormal stats.
        { IIN,                  "Internal Indications",Stats::ABNORMAL,0, 0 },
	{ NO_CONFIRM,           "Confirm Timeout",     Stats::ABNORMAL,0, 0 },
	{ RX_CONFIRM_TOO_LATE,  "Rx Confirm Too Late", Stats::ABNORMAL,0, 0 },
	{ RX_RESEND,            "Rx Resend",     Stats::ABNORMAL,0, 0 },
	{ RX_COLD_RESTART,      "Rx Cold Restart",     Stats::ABNORMAL,0, 0 },
	{ RX_WARM_RESTART,      "Rx Warm Restart",     Stats::ABNORMAL,0, 0 },
	{ RX_UNPARSABLE_DATA,   "Rx Unparsable Data",  Stats::ABNORMAL,0, 0 },
	{ RX_UNEXPECTED_CONFIRM,"Rx Unexpected Confirm",Stats::ABNORMAL,0, 0 },
       { RX_BAD_CONFIRM_SEQ_NUM,"Rx Bad Confirm Seq Num",Stats::ABNORMAL,0,0 },
	{ TX_OBJECT_UNKNOWN,    "Tx Object Unknown",   Stats::ABNORMAL,0, 0 },
	{ TX_PARAMETER_ERROR,   "Tx Parameter Error",  Stats::ABNORMAL,0, 0 },
	{ TX_FUNCTION_UNKNOWN,  "Tx Function Unknown", Stats::ABNORMAL,0, 0 },
    };

    assert (sizeof(temp)/sizeof(Stats::Element) == NUM_STATS);
    memcpy(statElements, temp, sizeof(temp));

    char name[Stats::MAX_USER_NAME_LEN];
//    addr.CopyTo((unsigned char*)name);
    snprintf(name, sizeof(name), "OS  %5d ", addr);
    //std::cout << "THE OS ADDRESS IS EQUAL TO " << addr << "\n";
    stats = Stats( name, addr, outstationConfig.debugLevel_p,
		   statElements, NUM_STATS, eventInterface_p,
		   EventInterface::AP_AB_ST);
    m_offline = false;
    m_multiplier = 1.0;
}

void Outstation::set_point_names(vector<string> analog_names, vector<string> binary_names) {
    analog_pt_names = analog_names;
    binary_pt_names = binary_names;
    vector<string>::iterator it;
}


void Outstation::set_stationName(std::string name){
  stationName = name;
  return;
}

static std::vector<std::string>
split(std::string str, char delimiter) {
  std::vector<std::string> retval;
  std::stringstream ss(str);
  std::string tok;
  while (std::getline(ss, tok, delimiter)) {
    retval.push_back(tok);
  }
  return retval;
}

void Outstation::enableSecureAuthentication( bool enable)
{
    secureAuthenticationEnabled = enable;
}

Datalink Outstation::getDl(void) {
        return dl;
}

DnpStat_t Outstation::getStat( int index)
{
    return stats.get( index);
}

DnpStat_t Outstation::getSecAuthStat( int index)
{
#ifdef SECURITY_ENABLED
    return secAuth.stats.get( index);
#endif
    return 0;
}

DnpStat_t Outstation::getState() const
{
    return (State) stats.get( STATE);
}

DnpStat_t Outstation::getSecAuthState() const
{
#ifdef SECURITY_ENABLED
    return secAuth.stats.get(SecureAuthentication::STATE);
#endif
    return 0;
}

void Outstation::changeState(State state)
{
    State old = (State) stats.get(STATE);
    if ( old != state)
    {
	stats.logNormal("State change: %s -> %s",
			stateStrings[ old],
			stateStrings[ state] );
	stats.set( STATE, state);
    }
}

void Outstation::set_publishCallback(const std::function<void(std::string, std::string, std::string)>& callback)
{
    publishCallback = callback;
}

DnpStat_t Outstation::rxData(Bytes* buf,
        map<string, float> analog_points,
        map<string, uint16_t> bin_points, Uptime_t timeRxd)
{
    NS_LOG_INFO("In rxData");
    while (buf->size() > 0)
    {
	Lpdu::UserData& segment = dl.rxData( *buf);
	//NS_LOG_INFO("Outstation DL segment size: " << segment.data.size());
        if (segment.data.size() > 0)
        {
            // this data has completed a segment
            //std::cout << "Outstation: src = " << segment.src << "\n";
            addr = tf_p->rxSegment( segment);
            if (addr != TransportFunction::FRAGMENT_NOT_FOUND) //To check
            {
                // this data had completed a fragment
                lastRxdAsdu = session.rxFragment;
                // send any responses to this address
                destAddr = segment.src;
                processRxdFragment(analog_points, bin_points);
            }
        }
    }
    return waitingFor;
}

DnpStat_t Outstation::timeout(TimerInterface::TimerId t)
{
    if (t == TimerInterface::RESPONSE)
    {
	if (waitingFor == EVENT_CONFIRM)
	{
	    stats.increment(NO_CONFIRM);
	}
    }
    return waitingFor;
}


void Outstation::processRxdFragment(map<string, float> analog_points, map<string, uint16_t> bin_points)
{
    NS_LOG_INFO("Processing Rxd Fragment");
    AppHeader::FunctionCode fn = AppHeader::getFn(session.rxFragment);
    lastRxSeqNum = AppHeader::getSeqNum(session.rxFragment);
#ifdef SECURITY_ENABLED
    if ( secAuth.rxAsdu( session.rxFragment) == false)
	return;
#endif
    ah.decode( session.rxFragment);
    // secAuth could have replaced the asdu with a queued asdu so we need
    // to get the fn again
    fn = ah.getFn();

    NS_LOG_INFO("Outstation Function code: " << fn);
    if (fn == AppHeader::READ) {
        NS_LOG_INFO("Outstation::Received function code: READ");
    } else if (fn == AppHeader::WRITE){
        NS_LOG_INFO("Outstation::Recieved function code: " << "WRITE");
    } else if (fn == AppHeader::SELECT) {
        NS_LOG_INFO("Outstation::Recieved function code: " << "SELECT");
    } else if (fn == AppHeader::OPERATE) {
        NS_LOG_INFO("Outstation::Recieved function code: " << "OPERATE");
    } else {
        NS_LOG_INFO("Outstation::Recieved function code: " << fn);
    }

    if (fn == AppHeader::CONFIRM)
    {
	if (lastTxSeqNum == ah.getSeqNum() or lastMIMTxSeqNum == ah.getSeqNum())
	{
	    stats.increment(RX_CONFIRM);
	    //processConfirm
	}
	else
	    stats.increment(RX_BAD_CONFIRM_SEQ_NUM);
    }
    else
    {
	if (waitingFor == EVENT_CONFIRM)
	    stats.increment(NO_CONFIRM);

	// regardless, process the new request
	if (fn == AppHeader::READ)
	{
            NS_LOG_INFO("Found Read message");
	    stats.increment(RX_READ);
	    read(analog_points, bin_points);
	}
	else if (fn == AppHeader::WRITE)
	{
	    stats.increment(RX_WRITE);
	    write();
	}
	else if (fn == AppHeader::SELECT)
	{
	    stats.increment(RX_SELECT);
	    control( fn);
	}
	else if (fn == AppHeader::OPERATE)
	{
	    stats.increment(RX_OPERATE);
	    control( fn);
	}
	else if (fn == AppHeader::DIR_OPERATE)
	{
	    stats.increment(RX_OPERATE);
	    control( fn);
	}
	else if (fn == AppHeader::DIR_OPERATE_NO_RESP)
	{
	    stats.increment(RX_OPERATE);
	    control( fn);
	}
	else if (fn == AppHeader::AUTHENTICATION_REPLY)
	{
	    // should have been handled by secAuth
	    assert(0);
	}
	else
	{
	    if (!broadcast)
	    {
		sendFunctionUnknown();
	    }
	}
    }
}

void Outstation::sendConfirm()
{
    // a confirm is simply a header with the confirm function code
    // no IIN bytes
    // first & final = 1, confirm & unsol = 0
    initResponse( 1, 1, 0, 0, 0, AppHeader::CONFIRM);
    transmit();
    stats.increment(Outstation::TX_CONFIRM);
}

void Outstation::initMIMResponse( bool fir, bool fin, bool con, bool uns,
			       uint16_t additionalIin,
			       AppHeader::FunctionCode fn)
{
    txFragment.clear();
    AppHeader a(fir,fin,con,uns,lastMIMRxSeqNum,fn,stats.get(IIN)|additionalIin);
    a.encode(txFragment);
}

void Outstation::initResponse( bool fir, bool fin, bool con, bool uns,
			       uint16_t additionalIin,
			       AppHeader::FunctionCode fn)
{
    txFragment.clear();
    AppHeader a(fir,fin,con,uns,lastRxSeqNum,fn,stats.get(IIN)|additionalIin);
    a.encode(txFragment);
}

void Outstation::transmit()
{
    NS_LOG_INFO("In transmit" );
    // hand it off to the transport function
    tf_p->transmit( destAddr, txFragment, stats);
}

void Outstation::transmitEmpty(Lpdu::UserData data) {
    tf_p->transmit( data.dest, txFragment, stats, data.src);
}

void Outstation::read(map<string, float> analog_points,
                        map<string, uint16_t> bin_points)
{
  NS_LOG_INFO("Inside Read");
    
    DnpObject* obj_p = NULL;
    bool parseOk = true;
    bool sendStaticData = false;
    bool sendEvents = false;
    bool sendScadaData = false;
    bool sendBinaryData = false;
    bool sendAnalogData = false;
    bool sendDeviceAttributes = false;

    // app header has already been stripped off
    while (session.rxFragment.size() > 0)
    {
	NS_LOG_INFO("IN THE WHILE LOOP");
        try
        {
            oh.decode( session.rxFragment, stats );
            stats.logNormal(oh.str(strbuf, sizeof(strbuf)));
            if ((oh.grp != 60) && (oh.grp != 30) && (oh.grp != 1) && (oh.grp != 41) && (oh.grp != 0))
            {
                obj_p = of.decode(oh, session.rxFragment, addr, stats);
            }
            if (oh.grp != 30)
                poll_cnt++;
        }
        catch (int e)
        {
            stats.increment( RX_UNPARSABLE_DATA);
            stats.logAbnormal(0, "Caught exception line# %d", e);
            parseOk = false;
            break;
        }

        NS_LOG_INFO("oh.grp: " << (int)oh.grp);

        // handle special cases
        if (oh.grp == 120)
        {
            // these objects are all supposed to come one at a time
            // with no other objects in the fragment
            if (session.rxFragment.size() > 0)
            {
            stats.logAbnormal(0, "Format not expected");
            parseOk = false;
            break;
            }
    #ifdef SECURITY_ENABLED
            if (oh.var == 4)
            {
            secAuth.rxKeyStatusReq((SessionKeyStatusReq*) obj_p);
            }
            else if (oh.var == 1)
            {
            secAuth.rxChallenge((Challenge*) obj_p);
            }
    #endif
        }
        else if (oh.grp == 60)
        {
            if  (oh.var==1)
            sendStaticData = true;
            else
            //sendEvents = true;
            sendScadaData = true;
        }
        else if (oh.grp == 30) //Handle Analog Input request
        {
            if ((oh.var == 2)||(oh.var == 5)){}
                sendAnalogData = true;
        }
        else if (oh.grp == 1) //Handle binary Input request
        {
            //cout << "Im here" << endl;
            if (oh.var == 0)
                sendBinaryData = true;
        }
        else if (oh.grp == 41) //Handle Analog output control
        {
            //cout << "Im here" << endl;
            //Ask about this one
            if (oh.var == 1)
                sendScadaData = true;
        }
        else if (oh.grp == 0) //Handle device attribute request
        {
            //std::cout << "got_group_0" << std::endl;
            //std::cout << (uint)(oh.var) << std::endl;
            //cout << "Im here" << endl;
            if (oh.var == 0xf1) {
                //std::cout << "correct_variation_of_group_0" << std::endl;
                sendDeviceAttributes = true;
            }
        }
    }
    //NS_LOG_INFO("READ request parseOk? " << parseOk);
    if (!parseOk)
    {
	    sendParameterError();
    }
    else if (sendStaticData)
    {
        //NS_LOG_INFO("READING static data ");
        // for prototype send 3 online binary inputs
        initResponse( 1, 1, 0, 0);
        oh = ObjectHeader(1,2,0,0,0,2);
        oh.encode(txFragment);
        BinaryInputWithStatus obj = BinaryInputWithStatus(0x81);
        obj.encode(txFragment);
        obj.encode(txFragment);
        obj = BinaryInputWithStatus(0x01);
        obj.encode(txFragment);

        transmit();
        stats.increment(TX_RESPONSE);
    }
    else if (sendEvents)
    {
        //NS_LOG_INFO("READING request sending null events!!");
	    sendNullResponse();
    }
    if (sendScadaData || (sendAnalogData && sendBinaryData))
    {
        NS_LOG_INFO("READ REQUEST for Analog data ");//<< poll_cnt << " size: " << pt1.size());
//        if (poll_cnt >= (pt1.size()-1)) {
//            poll_cnt = 0;
//        }
        if(sendScadaData){
          sendScadaData = false;
        }
        if(sendAnalogData && sendBinaryData){
          sendAnalogData = false;
          sendBinaryData = false;
        }

        transmitScadaData(analog_points, bin_points);
    }
    if(sendDeviceAttributes)
    {
        //std::cout << "sending_device_attributes" << std::endl;
        
        int fir;
        int fin;
        
        
        initResponse( 1, 1, 0, 0, 0x0000, AppHeader::RESPONSE);
        oh = ObjectHeader(0, 0xfe, 0, analog_pt_names.size() + binary_pt_names.size());
        oh.encode(txFragment);

        //Bytes attributes;
        for(uint8_t i = 0; i < analog_pt_names.size(); i++) {
            //txFragment.clear();
            // fir = analog_pt_names.size() > 1 && i == 0;
            // fin = analog_pt_names.size() > 1 && i == (analog_pt_names.size() - 1);
            //initResponse( fir, fin, 0, 0, i, 0x0000, AppHeader::RESPONSE);
            
            
            txFragment.push_back(5);
            txFragment.push_back('A');
            txFragment.push_back(i);

            for(uint8_t c = 0; c < analog_pt_names[i].size(); c++) {
                txFragment.push_back(analog_pt_names[i][c]);
            }
            
        }
        
        for(uint8_t i = 0; i < binary_pt_names.size(); i++) {
            //txFragment.clear();
            //initResponse( 1, 1, 0, 0, 0x0000, AppHeader::RESPONSE);
            // oh = ObjectHeader(0, 0xfe, 0, analog_pt_names.size() + i, 0, binary_pt_names[i].size() + 3);
            // oh.encode(txFragment);
            txFragment.push_back(5);
            txFragment.push_back('B');
            txFragment.push_back(i);

            for(uint8_t c = 0; c < binary_pt_names[i].size(); c++) {
                txFragment.push_back(binary_pt_names[i][c]);
            }
            //transmit();
        }
        transmit();
    }
    //attempted to break up analog and binary reading capabilites.
    if(sendAnalogData){
      NS_LOG_INFO("Reading Analog data");
      sendAnalogData = false;
      transmitAnalogData(analog_points);
      transmit();
      stats.increment(TX_RESPONSE);
    }
    if (sendBinaryData) {
        NS_LOG_INFO("READING Binary data ");
        sendBinaryData = false;
        CreateBinaryData(bin_points);
        transmit();
        stats.increment(TX_RESPONSE);
    }



}

void Outstation::write()
{
  NS_LOG_INFO("Inside write");
    DnpObject* obj_p = NULL;
    bool parseOk = true;
    // app header has already been stripped off
    while (session.rxFragment.size() > 0)
    {
	try
	{
	    //std::cout << session.rxFragment.size() << std::endl;
	    oh.decode( session.rxFragment, stats );
	    stats.logNormal(oh.str(strbuf, sizeof(strbuf)));
	    if (oh.grp == 120)
		obj_p = of.decode(oh, session.rxFragment, addr, stats);
	}
	catch (int e)
	{
	    stats.increment( RX_UNPARSABLE_DATA);
	    //std::cout << "Error 1" << std::endl;
	    stats.logAbnormal(0, "Caught exception line# %d", e);
	    parseOk = false;
	    break;
	}

	// handle special cases
	if (oh.grp == 120)
	{
	    // these objects are all supposed to come one at a time
	    // with no other objects in the fragment
	    if (session.rxFragment.size() > 0)
	    {
		stats.logAbnormal(0, "Format not expected");
		//std::cout << "Error 2" << std::endl;
		parseOk = false;
		break;
	    }
#ifdef SECURITY_ENABLED
	    if (oh.var == 6)
	    {
		secAuth.rxKeyChange((SessionKeyChange*) obj_p);
	    }
#endif
	}
	else if ((oh.grp == 80) and (oh.var == 1))
	{
	    if ((oh.qual == 0) && (oh.start==7) && (oh.stop==7))
	    {
		uint16_t iin = stats.get(IIN);
		// decode the value - it should always be 0;
		removeUINT8(session.rxFragment);
		if (iin & InternalIndications::DEVICE_RESTART)
		{
		    iin &= ~(InternalIndications::DEVICE_RESTART);
		    stats.set(IIN, iin);
		}
	    }
	    else
	    {
		parseOk = false;
		break;
	    }
	}
    }

    if (!parseOk)
    {
	    sendParameterError();
    }
}

void Outstation::control(AppHeader::FunctionCode fn)
{
    /*for(int listIndex = 0; listIndex < analog_pt_names.size(); listIndex++) {
        cout << "VICTORTEST6 A" << listIndex << ":   " << analog_pt_names[listIndex] << endl;
    }
    for(int listIndex = 0; listIndex < binary_pt_names.size(); listIndex++) {
        cout << "VICTORTEST6 B" << listIndex << ":   " << binary_pt_names[listIndex] << endl;
    }*/
    NS_LOG_INFO("INSIDE CONTROL");
    initResponse( 1, 1, 0, 0);
    // append a copy of the control request (which happens to be the
    // remaining portion of the request)
    //appendBytes( txFragment, session.rxFragment);
    //Get object to determine correct data for FNCS to publish
    oh.decode( session.rxFragment, stats );
    // NS_LOG_INFO("object group: " << unsigned(oh.grp));
    DnpObject* obj_p = of.decode(oh, session.rxFragment, addr, stats);
    int32_t v = obj_p->value;
    uint8_t f = obj_p->flag;
    string value;
    // value_type to handle different types of values (e.g., binary, analog, string)
    string value_type;
    // NS_LOG_INFO("Oh range specifier: " << unsigned(oh.rangeSpecifier));
    EventInterface::PointType_t pt = obj_p->pointType;
    NS_LOG_INFO("DNP val: " << v);
    NS_LOG_INFO("DNP point type: " << pt);
    NS_LOG_INFO("DNP flag: " << f);
    NS_LOG_INFO("DNP index: " << unsigned(obj_p->index));
    //NS_LOG_INFO("KYLE outstation name: " << Dnp3Application::GetName());
    string key;
    if(pt == EventInterface::AI){
      key = "AI";
    }
    else if(pt == EventInterface::BI){
      key = "BI";

    }
    else if(pt == EventInterface::CI){
      key = "CI";
    }
    else if(pt == EventInterface::AO){
      if (Bit32AnalogOutput* a = dynamic_cast<Bit32AnalogOutput*>(obj_p)) {
          //cout << "Requested value:" << a->request/1000 << " , Status: " << a->status << endl;  // request << ", Status:" << a->status << endl;
          //set value and its type
          value = to_string(a->request/1000);
          value_type = "int";
       }
       else {
             /// TODO: adding more types
       }
      //retrieve subclass
      /*Bit32AnalogOutput a = obj_p->value;*/
       //std::cout << "obj_p index: " << obj_p->index << std::endl; //<< " a index: " << a.index);

      //format string to successfully publish


      key = stationName + "/" + analog_pt_names[obj_p->index-2];
      //key = analog_pt_names[obj_p->index];


      //a.decode();

      //set value
      //value = to_string(a.request);
      //cout << "VICTORTEST4: value = " << value << endl;
      
      //cout << "key: " << key <<endl;
      

    }
    else if(pt == EventInterface::BO){
      //check with Priya about what GridLAB-D prefers.
      /*if(v == 0){
        value = "OPEN";
      }
      else{
        value = "CLOSED";
      }*/
      //cout << "I am in the Binary event "<< endl;
      if (ControlOutputRelayBlock* a = dynamic_cast<ControlOutputRelayBlock*>(obj_p)) {
         value_type = "string";
	 //cout << "a->outputCode: " << a->outputCode << endl;
	 //cout << "obj_p->index: " << obj_p->index << endl;
	 //cout << "TRIP: " << ControlOutputRelayBlock::Code::TRIP << endl;
         if(a->outputCode == ControlOutputRelayBlock::Code::TRIP){
             value = "OPEN";
         }
         else if (a->outputCode == ControlOutputRelayBlock::Code::CLOSE){
              value = "CLOSED";
         }
         else {
              /// TODO: Add ControlOutputRelayBlock::Code to handle different binary output
              throw(__LINE__);
          }
       }
       else {
               /// TODO: Add DNP objects to handle different binary output
               throw(__LINE__);
       }

      key = stationName + "/" + binary_pt_names[obj_p->index];
      //key = binary_pt_names[obj_p->index];

      //cout<<"key: " << key <<endl;

    }
    else if(pt == EventInterface::NONE){
      key = "NONE";
    }
    else if(pt == EventInterface::ST){
      key = "ST";
    }
    else if(pt == EventInterface::AP_AB_ST){
      key = "AP_AB_ST";
    }
    else if(pt == EventInterface::AP_NM_ST){
      key = "AP_NM_ST";
    }
    else if(pt == EventInterface::DL_AB_ST){
      key = "DL_AB_ST";
    }
    else if(pt == EventInterface::DL_NM_ST){
      key = "DL_NM_ST";
    }
    else if(pt == EventInterface::SA_AB_ST){
      key = "SA_AB_ST";
    }
    else if(pt == EventInterface::SA_NM_ST){
      key = "SA_NM_ST";
    }
    else if(pt == EventInterface::EP_AB_ST){
      key = "EP_AB_ST";
    }
    else if(pt == EventInterface::EP_NM_ST){
      key = "EP_NM_ST";
    }




    //Don't respond if command is for no response
    if(fn == AppHeader::DIR_OPERATE_NO_RESP){

      #ifdef FNCS
      //cout << "VICTORTEST key: " << key << "  |  value: " << value << endl;
      //fncs::publish(key, value);
      #endif
      // NS_LOG_INFO("Here in direct operate no response");
      //transmit();
      publishCallback(key, value, value_type);
      return;
    }
    //moved line below to execute at beginning of fucntion
    // initResponse( 1, 1, 0, 0);
    // // append a copy of the control request (which happens to be the
    // // remaining portion of the request)
    // appendBytes( txFragment, session.rxFragment);

    transmit();

    if (fn == AppHeader::SELECT){
	     stats.increment(TX_SELECT_RESP);
    }
    else if (fn == AppHeader::OPERATE || fn == AppHeader::DIR_OPERATE ){
      // NS_LOG_INFO("Here in operate and direct operate");
      #ifdef FNCS
      //cout << "VICTORTEST2 key: " << key << "  |  value: " << value << endl;
      //fncs::publish(key, value);
      #endif
      publishCallback(key, value, value_type);
      stats.increment(TX_OPERATE_RESP);
    }
    else
	assert(0);
}


void Outstation::sendNullResponse()
{
    NS_LOG_INFO("Sending Null response");
    initResponse( 1, 1, 0, 0);
    transmit();
    stats.increment(TX_NULL_RESPONSE);
    stats.increment(TX_RESPONSE);
}

void Outstation::transmitZeroResponse(Lpdu::UserData data, map<string, float> analog_points, map<string, uint16_t> bin_points, AppSeqNum_t seq) {
  NS_LOG_INFO("transmit zero response GGGGGGGGGGGGGGGGGGGGGGGGG");
    // for prototype send 3 16 bit analog inputs
    lastMIMRxSeqNum = seq;
    initMIMResponse( 1, 1, 0, 0);
    oh = ObjectHeader(30,5,ObjectHeader::ONE_OCTET_START_STOP_INDEXES, analog_points.size(),0,(analog_points.size()-1));
    oh.encode(txFragment);


    //Set all zero values
    map<string, float >::iterator it;
    for(it=analog_points.begin(); it != analog_points.end(); it++) {
        Bit16AnalogFloatInput obj = Bit16AnalogFloatInput(0.0);
        obj.encode(txFragment);
    }

    oh = ObjectHeader(1,2, 0, bin_points.size(), 0, (bin_points.size()-1));
    oh.encode(txFragment);
    //Set all zero values
    map<string, uint16_t >::iterator it1;
    for(it1 = bin_points.begin(); it1 != bin_points.end(); it1++) {
        BinaryInputWithStatus obj = BinaryInputWithStatus(0x01);
        obj.encode(txFragment);
    }
    transmitEmpty(data);
    stats.increment(TX_RESPONSE);
}

void Outstation::transmitModData(std::vector<uint8_t> a, Lpdu::UserData data, map<string, float> analog_points, map<string, uint16_t> bin_points, AppSeqNum_t seq){

   lastMIMRxSeqNum = seq;
   if (txFragment.size() == 0 or txFragment.size() > 560){
   //txFragment.clear();
   initMIMResponse(1,1,0,0);
   //initResponse( 1, 1, 0, 0, seq, AppHeader::RESPONSE); //0x0000, AppHeader::RESPONSE);
   //oh = ObjectHeader(30, 5, ObjectHeader::ONE_OCTET_START_STOP_INDEXES, analog_pt_names.size() + binary_pt_names.size(), 0, (analog_points.size() + bin_points.size()-1));
   oh = ObjectHeader(30, 5, ObjectHeader::ONE_OCTET_START_STOP_INDEXES, analog_points.size(), 0, (analog_points.size()-1));
   oh.encode(txFragment);
   }

   NS_LOG_INFO("The size of the buffer that is received is " << a.size());
   //Bytes a = *buf;
   std::vector<unsigned char> val;
   int h = 0;
   int sd = 0;//*18; //20;
   for (int j = 0; j < a.size()-1; j++){
       if (a[j] == 0x1e and a[j+1] == 0x05){
           sd = j - 4;
	   //std::cout << "SD: " << sd << std::endl;
	   //break;
       }
   }
   /*if (sd == 0){
       oh = ObjectHeader(30, 5, ObjectHeader::ONE_OCTET_START_STOP_INDEXES, analog_points.size(), 0, (analog_points.size()-1));
       oh.encode(txFragment); 
   }*/
   //std::cout << "The start of the packet is at " << sd << std::endl;
   //if (a[sd] != 0x01){
   //    txFragment.push_back(0x01);
   //}
   bool f = false;
   //if (sd > 0){
   int i = sd;
   //for (int i = sd; i < a.size(); i++){
   // std::cout << "txFragment size is " << txFragment.size() << std::endl;
   while(i < a.size()){
	 if (a[i] == 0x01 && a[i+1] == 0x02){
	     f = true;
             //std::cout << "Found binary points " << std::endl;
             oh = ObjectHeader(1, 2, 0, bin_points.size(), 0, (bin_points.size()-1));
	     oh.encode(txFragment);
	     map<string, uint16_t>::iterator it1;
	     for(it1 = bin_points.begin(); it1 != bin_points.end(); it1++){
                 BinaryInputWithStatus obj = BinaryInputWithStatus(0x81);
		 obj.encode(txFragment);
	     }
	     val.clear();
	     break;
	 }
	 else if (a[i] == 0x01){ // Each point is seperated with a 0x01 byte
             h ++; // to make sure that we are not removing bytes before we see the first 0x01
	     std::vector<unsigned char> temp;
	     if((val.size() > 4)) {//&& h> 1)){
		 //float xx = 0;
                 for (int j = 0; j < val.size(); j++){
	           //xx *= (float)val[i];
                   temp.push_back(val[j]);
		 }
                 //Bit16AnalogFloatInput obj = Bit16AnalogFloatInput (xx);
		 //obj.encode(txFragment);
	         temp.push_back(a[i]);
	         for (int j = 0; j < temp.size(); j++){
                     txFragment.push_back(temp[j]);// Add to the txBuffer all the bytes
	         }
	     }
	     val.clear(); //clear the intermediate vector
         }
	 else{
             val.push_back(a[i]); // collect the bytes between 0x01 flags
         }
	 i++;
   }
   if (val.size() > 0){ // if there are still points to put into the txBuffer make sure to put them 
       for (int j = 0; j < val.size()-1; j++){
           txFragment.push_back(val[j]);
       }
       val.clear();
   }
   //}

   /*if (txFragment.size() > 400){
       txFragment.pop_back();
       txFragment.pop_back();*/
       /*oh = ObjectHeader(1,2, 0, bin_points.size(), 0, (bin_points.size()-1));
       oh.encode(txFragment);
       //Set all zero values
       map<string, uint16_t >::iterator it1;
       for(it1 = bin_points.begin(); it1 != bin_points.end(); it1++) {
           BinaryInputWithStatus obj = BinaryInputWithStatus(0x01);
           obj.encode(txFragment);
       }*/
   //}

   NS_LOG_INFO("This is the size of txFragment " << txFragment.size());
   NS_LOG_INFO("This is the size of the rxFragment " << session.rxFragment.size());
   if (f){ //txFragment.size()>200) { //|| txFragment.size() < 52){
     transmitEmpty(data);
     stats.increment(TX_RESPONSE);
   }
}

void Outstation::sendFunctionUnknown()
{
    initResponse( 1, 1, 0, 0, InternalIndications::FUNCTION_UNKNOWN);
    transmit();
    stats.increment(TX_FUNCTION_UNKNOWN);
    stats.increment(TX_RESPONSE);
}

void Outstation::sendParameterError()
{
    initResponse( 1, 1, 0, 0, InternalIndications::PARAMETER_ERROR);
    transmit();
    stats.increment(TX_PARAMETER_ERROR);
    stats.increment(TX_RESPONSE);
}

void Outstation::set_offline(bool offline)
{
    m_offline = offline;
}

void Outstation::set_multiplier(float multiplier)
{
    m_multiplier = multiplier;
}

void Outstation::appendVariableSizedObject( const ObjectHeader& h,
					    const DnpObject& o)
{
    stats.logNormal( h.str( strbuf, sizeof(strbuf)));
    h.encode( txFragment);
    appendUINT16( txFragment, o.size());
    o.encode( txFragment);
}


void Outstation::transmitScadaData(map<string, float> analog_points, map<string, uint16_t> bin_points)
{
NS_LOG_INFO("In transmitScadaData");

    //Send null response if both points are empty
    if(analog_points.size() == 0 && binary_pt_names.size() == 0){
      Outstation::sendNullResponse();
      return;
    }
    if(analog_points.size() > 0){
      transmitAnalogData(analog_points);
    }
    if(bin_points.size() > 0){
      CreateBinaryData(bin_points);
    }
    transmit();
    stats.increment(TX_RESPONSE);
}

void Outstation::transmitAnalogData(map<string, float> analog_points){
  //for(int listIndex = 0; listIndex < analog_pt_names.size(); listIndex++) {
  //      cout << "VICTORTEST5 A" << listIndex << ":   " << analog_pt_names[listIndex] << endl;
  //}
    // for prototype send 3 16 bit analog inputs
  initResponse( 1, 1, 0, 0);
  int stop_index, range_field_size, flag;
  if(m_offline) flag = 0x00;
  else flag = 0x01;
  if(analog_pt_names.size() > 0) stop_index = (analog_pt_names.size()-1);
  else stop_index = 0;

   //NS_LOG_INFO("Analog points size: " << analog_points.size());
//TODO: We are assuming a start point of 0 here for this calculation. If we change that then this calculation will be wrong.
  if(analog_points.size() > 0){
    if(stop_index > 255) range_field_size = ObjectHeader::TWO_OCTET_START_STOP_INDEXES;
    else range_field_size = ObjectHeader::ONE_OCTET_START_STOP_INDEXES;
    oh = ObjectHeader(30,5, range_field_size, analog_pt_names.size(),0,stop_index);
    oh.encode(txFragment);

    vector<string>::iterator it;
    for(it=analog_pt_names.begin(); it != analog_pt_names.end(); it++) {
        string name = *it;
        // NS_LOG_INFO("I'm inside analog loop");
        if (analog_points.find(name) != analog_points.end()) {
             float value = analog_points[name];
             //NS_LOG_INFO("Found analog value: " << value);
           //cout << "Value: " << value << endl;

            Bit16AnalogFloatInput obj = Bit16AnalogFloatInput((value * m_multiplier), flag);
            obj.encode(txFragment);
        }
    }
  }

  return;
}

void Outstation::CreateBinaryData(map<string, uint16_t> bin_points)
{
    /*for(int listIndex = 0; listIndex < binary_pt_names.size(); listIndex++) {
        cout << "VICTORTEST5 B" << listIndex << ":   " << binary_pt_names[listIndex] << endl;
    }*/
    
    int stop_index, range_field_size, flag;
    if(binary_pt_names.size() > 0){

     stop_index = (binary_pt_names.size()-1);


    if(stop_index > 255) range_field_size = ObjectHeader::TWO_OCTET_START_STOP_INDEXES;
    else range_field_size = ObjectHeader::ONE_OCTET_START_STOP_INDEXES;
    if(m_offline) flag = 0x00;
    else flag = 0x01;
    oh = ObjectHeader(1,2, range_field_size, binary_pt_names.size(), 0, stop_index);
    oh.encode(txFragment);
    NS_LOG_INFO("binary data size " << binary_pt_names.size());
    vector<string>::iterator it;
    for(it=binary_pt_names.begin(); it != binary_pt_names.end(); it++) {
        string name = *it;
        if (bin_points.find(name) != bin_points.end()) {
            BinaryInputWithStatus obj = BinaryInputWithStatus(((bin_points[name] << 7) | flag));
            obj.encode(txFragment);
        }

    }
  }

}


}//namespace ns3
