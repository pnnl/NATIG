#!/bin/bash

cd /rd2c/ns-3-dev/contrib/
rm -r helics
git clone --depth 1 --branch HELICS-v2.x-waf https://github.com/GMLC-TDC/helics-ns3.git; mv helics-ns3 helics
cp -r /rd2c/PUSH/NATIG/RC/code/helics/helics-helper* /rd2c/ns-3-dev/contrib/helics/helper/
cp -r /rd2c/PUSH/NATIG/RC/code/helics/dnp3-application-helper-new.* /rd2c/ns-3-dev/contrib/helics/helper/
cp -r /rd2c/PUSH/NATIG/RC/code/helics/dnp3-application-new* /rd2c/ns-3-dev/contrib/helics/model/
cp -r /rd2c/ns-3-dev/contrib/helics/model/dnp3-application-new-Docker.h /rd2c/ns-3-dev/contrib/helics/model/dnp3-application-new.h
cp -r /rd2c/ns-3-dev/contrib/helics/model/dnp3-application-new-Docker.cc /rd2c/ns-3-dev/contrib/helics/model/dnp3-application-new.cc
cp -r /rd2c/PUSH/NATIG/RC/code/helics/wscript /rd2c/ns-3-dev/contrib/helics/
cp -r /rd2c/PUSH/NATIG/RC/code/helics/helics-simulator-impl.cc /rd2c/ns-3-dev/contrib/helics/model/
