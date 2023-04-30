#!/bin/bash 
 
clear 
 
pkill -9 helics_broker
pkill -9 gridlabd
pkill -9 python
pkill -9 ns3-helics-grid-dnp3
pkill -9 ns3-helics-grid-dnp3-direct-analog
pkill -9 ns3-helics-grid-dnp3-direct-binary

exit 0 
