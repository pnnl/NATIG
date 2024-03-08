#!/bin/bash

git pull
RD2C=/rd2c

cp -r integration $RD2C 
cp -r RC/code/run.sh ${RD2C}/integration/control 
cp -r RC/code/ns3-helics-grid-dnp3-4G-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3-4G.cc  
cp -r RC/code/ns3-helics-grid-dnp3-5G-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3-5G.cc  
cp -r RC/code/ns3-helics-grid-dnp3-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3.cc  
cp -r RC/code/dnp3/wscript ${RD2C}/ns-3-dev/src/dnp3/ 
cp -r RC/code/applications/model/fncs-application.* ${RD2C}/ns-3-dev/src/applications/model/ 
cp -r RC/code/dnp3/model/dnp3-application.h ${RD2C}/ns-3-dev/src/dnp3/model/ 
cp -r RC/code/dnp3/model/dnp3-application-Docker.cc ${RD2C}/ns-3-dev/src/dnp3/model/dnp3-application.cc 
cp -r RC/code/helics-backup/model/dnp3-application-new.h ${RD2C}/ns-3-dev/contrib/helics/model/ 
cp -r RC/code/helics-backup/model/dnp3-application-new-Docker.cc ${RD2C}/ns-3-dev/contrib/helics/model/dnp3-application-new.cc 
cp -r RC/code/helics-backup/model/dnp3-helics-application-Docker.cc ${RD2C}/ns-3-dev/contrib/helics/model/dnp3-helics-application.cc 
cp -r RC/code/internet/* ${RD2C}/ns-3-dev/src/internet/ 
cp -r RC/code/lte/* ${RD2C}/ns-3-dev/src/lte/ 
cp -r RC/code/dnp3/model/dnp3-application-Docker.cc ${RD2C}/ns-3-dev/src/dnp3/model/dnp3-application.cc 
cp -r RC/code/gridlabd/* ${RD2C}/gridlab-d/tape_file/ 
cp -r RC/code/trigger.player ${RD2C}/integration/control/

cd ${RD2C}/gridlab-d/third_party/xerces-c-3.2.0 
./configure 
make 
make install 
cd ${RD2C}/gridlab-d 
autoreconf -if 
./configure --with-helics=/usr/local --prefix=${RD2C} --enable-silent-rules 'CFLAGS=-g -O2 -w' 'CXXFLAGS=-g -O2 -w -std=c++14' 'LDFLAGS=-g -O2 -w' 
make 
make install

LDFLAGS="-ljsoncpp -L/usr/local/include/jsoncpp/"
cd $RD2C/PUSH/NATIG 
./build_ns3.sh $1 ${RD2C}
