#ifndef DUMMY_H
#define DUMMY_H

#include <vector>
#include "ns3/timer_interface.hpp"

namespace ns3{

class DummyTimer : public TimerInterface
{
public:
    DummyTimer();
    void activate( TimerId timerId);
    void cancel( TimerId timerId);
    bool isActive( TimerId timerId);
    std::vector<bool> timerActive;

};

}

#endif
