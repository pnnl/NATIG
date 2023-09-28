//
// $Id: endpoint.hpp 4 2007-04-10 22:55:27Z sparky1194 $
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

#ifndef DNP_ENDPOINT_H
#define DNP_ENDPOINT_H

#include <map>
#include "ns3/address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/common.hpp"
#include "ns3/stats.hpp"
#include "ns3/lpdu.hpp"
#include "ns3/transmit_interface.hpp"
#include "ns3/socket.h"
#include "ns3/ptr.h"
#include "ns3/simulator-impl.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

namespace ns3 {
class Socket;
class Packet;
class InetSocketAddress;
class Inet6SocketAddress;

typedef struct {

    unsigned short portOfLastRequest;   // only used for UDP
    uint16_t       port;                // for tx
    Address        ip;
    Uptime_t       timeOfLastRx;

} RemoteDevice;


class Endpoint : public TransmitInterface
{
public:

    // the key for deviceInfoMap is the remote IP - this is to
    // to comply with 4.3.5.1 Connection Establishment Method 1
    // of the DNP3 IP Networking Specification

    typedef struct
    {
	DnpAddr_t                             ownerDnpAddr;
	bool                                  tcp;
	bool                                  initiating;
	unsigned short                        listenPort;
	std::map<DnpAddr_t, RemoteDevice>     deviceMap;
	int*                                  debugLevel_p;
	double                                jitterMinNs; //!<minimum jitter delay time for packets sent via FNCS
    double                                jitterMaxNs; //!<maximum jitter delay time for packets sent via FNCS

    } EndpointConfig;

    Endpoint(const EndpointConfig& config, TracedCallback<Ptr<const Packet> > trace_p,
            Ptr<Socket> socket, EventInterface* eventInterface_p);

    // implement the transmit interface
    Uptime_t transmit( const Lpdu& lpdu);
    void update_sock(Ptr<Socket> sock);
    ~Endpoint();
    void readDatagrams();

private:
    void send (const Lpdu& lpdu, Address remote_addr, uint16_t remote_port);
    InetSocketAddress GetInet (Address remote_addr, uint16_t remote_port);
    Inet6SocketAddress GetInet6 (Address remote_addr, uint16_t remote_port);
    enum statIndex {       RX_UDP_MULTICAST,
			   RX_UDP_PACKET,
			   TX_UDP_PACKET,
			   RX_UNKNOWN_IP,
			   NUM_STATS       };

    bool               tcp;
    unsigned short     listenPort;
    Ptr<Socket> m_socket; //!< Socket
    Ptr<UniformRandomVariable> m_rand_delay_ns; //!<random value used to add jitter to packet transmission

      /// Callbacks for tracing the packet Tx events
    TracedCallback<Ptr<const Packet> > m_txTrace;
    // dnp address is the key - used by tx
    std::map<DnpAddr_t, RemoteDevice> deviceMap;

    // ip address is the key - used by rx
    std::map<Address, RemoteDevice> deviceIpMap;
    double m_jitterMinNs; //!<minimum jitter delay time for packets sent via FNCS
    double m_jitterMaxNs; //!<maximum jitter delay time for packets sent via FNCS
    Stats              stats;
    Stats::Element     statElements[NUM_STATS];
    char               strbuf[Stats::MAX_LOG_LEN];
    uint32_t m_sent; //!< Counter for sent packets

};

}

#endif //DNP_ENDPOINT_H
