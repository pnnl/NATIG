//
// $Id: outstation.hpp 12 2007-04-12 22:17:18Z sparky1194 $
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

#ifndef OUTSTATION_H
#define OUTSTATION_H

#include "ns3/common.hpp"
#include "ns3/app.hpp"
#include "ns3/asdu.hpp"
#include "ns3/object.hpp"
#ifdef SECURITY_ENABLED
#include "ns3/security.hpp"
#endif
#include "ns3/socket.h"
#include <deque>
#include <vector>
#include <string>
#include <map>
#include <pthread.h>
#include <functional>
#include "ns3/transmit_interface.hpp"


using namespace std;

namespace ns3 {

class Outstation : BaseApplication
{
public:
    static const unsigned int MAX_POINTS      = 79;
    static const unsigned int MAX_ANALOG_POINTS  = 8;
    static const unsigned int MAX_BINARY_POINTS  = 71;

    typedef struct
    {
	DnpAddr_t        addr;
	DnpAddr_t        masterAddr;
	UserNumber_t     userNum;
	int*             debugLevel_p;
    } OutstationConfig;

    enum PointType {
        ANALOG=0,
        BINARY
    };

    typedef struct{
        PointType type;
        string name;
        uint16_t analog_values;
        uint8_t bin_values;
    } PointInfo;

    // state can either be waiting for something or not
    enum State         {  IDLE = 0,
			  EVENT_CONFIRM,
			  NUM_STATES  };

    void changeState(State state);

    static const char* stateStrings[ NUM_STATES];

    Outstation( OutstationConfig&          outstationConfig,
		Datalink::DatalinkConfig&  datalinkConfig,
		EventInterface*            eventInterface_p,
		TimerInterface*            timerInterface_p);


    DnpStat_t rxData(Bytes* buf, map<string, float> analog_points,
                        map<string, uint16_t> bin_points, Uptime_t timeRxd=0);
    DnpStat_t timeout(TimerInterface::TimerId t);

    DnpStat_t getState() const;
    DnpStat_t getSecAuthState() const;
    void transmitEmpty(Lpdu::UserData data);
    void transmitZeroResponse(Lpdu::UserData data, map<string, float> analog_points, map<string, uint16_t> bin_points, AppSeqNum_t seq);
    void transmitScadaData(map<string, float> analog_points, map<string, uint16_t> bin_points);
    void transmitModData(std::vector<uint8_t> buf, Lpdu::UserData data, map<string, float> analog_points, map<string, uint16_t> bin_points, AppSeqNum_t seq);
    void enableSecureAuthentication(bool enable=true);

    enum StatIndex     {  STATE  = TransportStats::NUM_STATS,
			  TX_RESPONSE,
			  TX_NULL_RESPONSE,
			  TX_CONFIRM,
			  TX_UNSOLICITED,
			  TX_SELECT_RESP,
			  TX_OPERATE_RESP,
			  TX_FRAGMENT,
			  TX_TIME_DELAY_FINE,
			  TX_TIME_DELAY_COARSE,
			  RX_WRITE_TIME,
			  RX_DELAY_MEASUREMENT,
			  RX_CONFIRM,
			  RX_CLASS_O_POLL,
			  RX_CLASS_1_POLL,
			  RX_CLASS_2_POLL,
			  RX_CLASS_3_POLL,
			  RX_READ,
			  RX_WRITE,
			  RX_SELECT,
			  RX_OPERATE,
			  RX_DIR_OP,
			  RX_DIR_OP_NO_ACK,
			  RX_BROADCAST,
			  IIN,

			  // Abnormal
			  NO_CONFIRM,
			  RX_CONFIRM_TOO_LATE,
			  RX_RESEND,
			  RX_COLD_RESTART,
			  RX_WARM_RESTART,
			  RX_UNPARSABLE_DATA,
			  RX_UNEXPECTED_CONFIRM,
			  RX_BAD_CONFIRM_SEQ_NUM,
			  TX_OBJECT_UNKNOWN,
			  TX_PARAMETER_ERROR,
			  TX_FUNCTION_UNKNOWN,
			  NUM_STATS };


    Stats::Element      statElements[NUM_STATS];
    Stats               stats;

    void transmit();
    int ind = 0;
    std::map<int, std::string> observed;
    // primarity for unit testing
    DnpStat_t getStat( int index);
    DnpStat_t getSecAuthStat( int index);
    Datalink getDl();

    DnpStat_t rxMIMData(Bytes* buf);
    void set_point_names(vector<string> analog_names, vector<string> binary_names);
    void set_offline(bool offline);
    void set_multiplier(float multiplier);
    //set the stationName
    void set_stationName(string name);
    void set_publishCallback(const std::function<void(std::string, std::string, std::string)>& callback);
private:
#ifdef SECURITY_ENABLED
    friend class SecureAuthentication;
    friend class OutstationSecurity;
#endif
    // call when a new fragment has been rxd by the transport function
    void processRxdFragment(map<string, float> analog_points, map<string, uint16_t> bin_points);

    void read(map<string, float> analog_points, map<string, uint16_t> bin_points);
    void write();
    void control( AppHeader::FunctionCode fn);
    void controlNoResp( AppHeader::FunctionCode fn);
    void sendNullResponse();
    void sendFunctionUnknown();
    void sendObjectUnknown();
    void sendParameterError();

    // this method begins the construction of a response by
    // starting with the application header
    void initResponse(bool fir, bool fin, bool con, bool uns,
		      uint16_t additionalIin = 0,
		      AppHeader::FunctionCode fn = AppHeader::RESPONSE);

    void initMIMResponse(bool fir, bool fin, bool con, bool uns,
		      uint16_t additionalIin = 0,
		      AppHeader::FunctionCode fn = AppHeader::RESPONSE);

    void sendConfirm();

    void sendFragments();
    void sendNextFragment();

    void appendVariableSizedObject(const ObjectHeader& h, const DnpObject& o);
    //void transmitScadaData(map<string, float> analog_points, map<string, uint16_t> bin_points);
    void CreateBinaryData(map<string, uint16_t> bin_points);
    vector<float> reformatPowerData(map<string, float> analog_points);
    void initConfig(void);
    void transmitAnalogData(map<string, float> analog_points);

    State                   waitingFor;
    Bytes                   txFragment;
    TransportSession        session;       // used by the transport function
#ifdef SECURITY_ENABLED
    OutstationSecurity      secAuth;
#endif
    AppSeqNum_t             lastRxSeqNum;
    AppSeqNum_t             lastTxSeqNum;
    AppSeqNum_t             lastMIMRxSeqNum;
    AppSeqNum_t             lastMIMTxSeqNum;
    DnpAddr_t               masterAddr;
    DnpAddr_t               destAddr;     // addr to send responses to
    bool                    broadcast; // true if last rxd frag was broadcast

    int poll_cnt;
    int point_num;
    bool m_offline;
    float m_multiplier;
    TransmitInterface* ep_p;
    vector<string> analog_pt_names;
    vector<string> binary_pt_names;
    //Kyle: added name of the outstation to easily concatenate strings to
    //publish to FNCS for closed loop functionality
    string stationName;
    std::function<void(std::string, std::string, std::string)> publishCallback;
};

}

#endif
