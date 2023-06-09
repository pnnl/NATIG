//
// $Id: app.hpp 4 2007-04-10 22:55:27Z sparky1194 $
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

#ifndef APP_H
#define APP_H

#include "ns3/common.hpp"
#include "ns3/asdu.hpp"
#include "ns3/datalink.hpp"
#include "ns3/factory.hpp"
#include "ns3/transport.hpp"
#include "ns3/stats.hpp"
#include "ns3/event_interface.hpp"
#include "ns3/timer_interface.hpp"

namespace ns3 {

// Base class for Master and Outstation implemenations
class BaseApplication
{
  public:
    virtual ~BaseApplication();
    TimerInterface*        timer_p;

  protected:
    friend class SecureAuthentication;
    friend class MasterSecurity;
    friend class OutstationSecurity;

    BaseApplication(int*                       debugLevel_p,
		DnpAddr_t                  dnpAddr,
		UserNumber_t               num,
		Datalink::DatalinkConfig&  datalinkConfig,
		EventInterface*            eventInterface_p,
		TimerInterface*            timerInterface_p );

    int*                   debug_p;
    DnpAddr_t              addr;
    UserNumber_t           userNum;

    TransportFunction*     tf_p;
    Datalink               dl;
    AppHeader              ah;
    ObjectHeader           oh;
    Factory                of;
    EventInterface*        db_p;
    bool                   secureAuthenticationEnabled;

    Bytes                  lastRxdAsdu; // used for by security

    // should be adequate for the app and object header strs
    char             strbuf[80];

    virtual void transmit()=0;

    virtual void appendVariableSizedObject(const ObjectHeader& h,
					   const DnpObject& o)=0;
};

}
#endif
