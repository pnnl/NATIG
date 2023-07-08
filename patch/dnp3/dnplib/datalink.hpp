//
// $Id: datalink.hpp 4 2007-04-10 22:55:27Z sparky1194 $
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


#ifndef DNP_DATALINK_H
#define DNP_DATALINK_H

#include "ns3/common.hpp"
#include "ns3/stats.hpp"
#include "ns3/lpdu.hpp"
#include "ns3/transmit_interface.hpp"
#include "ns3/event_interface.hpp"

namespace ns3 {

class Datalink
{

public:

    typedef struct
    {
	DnpAddr_t               addr;
	bool                    isMaster;
	int                     keepAliveInterval_ms;
	int*                    debugLevel_p;
	TransmitInterface*      tx_p;
    } DatalinkConfig;

    Datalink(DatalinkConfig& config, EventInterface* eventInterface_p);

    // called by the transport function when new user data
    // is to be transmitted out the endpoint
    // the destination address of the userData structure must be assigned
    Uptime_t transmit( DnpAddr_t dest, Bytes& data);
    Uptime_t transmit( DnpAddr_t dest, DnpAddr_t src, Bytes& data);

    // called by an application object upon receipt of new data bytes
    // from an endpoint.
    // returns NULL if no application user data
    Lpdu::UserData& rxData(Bytes& data);

    Lpdu             rxLpdu;    // object for building incoming lpdus
    Lpdu             emptyLpdu;
    Lpdu             txLpdu;    // object for building outgoing lpdus
    // for the transmit function
    TransmitInterface* tx_p;
private:

    enum statIndex {       RX_FRAMES = Lpdu::NUM_STATS,
			   RX_ACKS,
			   RX_LINK_STATUS,
			   RX_RESET_LINK,
			   RX_TEST_LINK,
			   RX_USER_DATA_CONFIRM,
			   RX_USER_DATA_NO_CONFIRM,  
			   RX_REQUEST_LINK_STATUS,
			   TX_FRAMES,
			   TX_ACKS,
			   TX_LINK_STATUS,
			   TX_RESET_LINK,
			   TX_TEST_LINK,
			   TX_USER_DATA_CONFIRM,  
			   TX_USER_DATA_NO_CONFIRM,  
			   TX_REQUEST_LINK_STATUS,
			   // Abnormal
			   TIMEOUTS_ACK, 
			   TIMEOUTS_lINK_STATUS,            
			   RX_NACKS,
			   RX_NOT_SUPPORTED,
			   RX_FN_CODE_ERROR,
			   RX_UNEXPECTED_ACKS,
			   RX_UNEXPECTED_LINK_STATUS,
			   RX_FCB_INCORRECT,
			   RX_FCV_INCORRECT,
			   RX_OTHER_ADDRESSES,
			   TX_NACKS,
			   TX_NOT_SUPPORTED,
			   NUM_STATS       };

    // these function codes are valid only for PRM = 1
    static const unsigned short RESET_LINK                    = 0x00;
    static const unsigned short TEST_LINK                      =0x02;
    static const unsigned short CONFIRMED_USER_DATA            =0x03;
    static const unsigned short UNCONFIRMED_USER_DATA          =0x04;
    static const unsigned short REQUEST_LINK_STATUS            =0x09;

    // these function codes are valid only for PRM = 0
    static const unsigned short ACK                            =0x00;
    static const unsigned short NACK                           =0x01;
    static const unsigned short LINK_STATUS                    =0x0b;
    static const unsigned short NOT_SUPPORTED                  =0x0f;

    //internal states
    enum State{   IDLE,
		  WAITING_FOR_ACK,
		  WAITING_FOR_LINK_STATUS,
		  WAITING_FOR_ACK_TO_LINK_RESET };

    void             processRxdLpdu();

    void             ack( DnpAddr_t dest);
    void             nack( DnpAddr_t dest);
    // only used for TCP keep alives
    void             sendRequestLinkStatus( DnpAddr_t dest);

    State            state;
    int              linkReset;
    int              linkNeedsToBeReset;
    unsigned int     rxFcb;



    // will fill in our private userData object from the rxLpdu
    Lpdu::UserData   userData;
    

    unsigned char    isMaster;
    Uptime_t         lastRx;
    Stats            stats;
    Stats::Element   statElements[NUM_STATS];

    DnpAddr_t        addr;

    // these are used for tcp only
    int              keepAliveInterval_ms;
    int              responseTimeout_ms;
};

}
#endif  /* DNP_DATALINK_H */
