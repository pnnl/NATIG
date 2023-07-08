#include "ns3/dummy_timer.hpp"

namespace ns3 {
// Timer Interface --------------------

DummyTimer::DummyTimer() :
  timerActive(TimerInterface::NUM_TIMERS, false)
{
}

void DummyTimer::activate( TimerId timerId)
{
    timerActive[timerId] = true;
}

void DummyTimer::cancel( TimerId timerId)
{
    timerActive[timerId] = false;
}

bool DummyTimer::isActive( TimerId timerId)
{
    return timerActive[timerId];
}

}