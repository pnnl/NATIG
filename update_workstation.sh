#!/bin/bash

git pull
RD2C=/rd2c

read -p "Do you want to save your current setup? [y/n] " answer

echo "Input 1: ${answer}"

if [[ $answer == [yY] || $answer == [yY][eE][sS] ]]
then
	    dt=$(date '+%d-%m-%Y-%H:%M:%S')
	    echo "Saving setup in $dt"
	    mkdir $dt
	    cp /rd2c/integration/control/*.cc $dt
	    cp /rd2c/integration/control/config/grid.json $dt
	    cp /rd2c/integration/control/config/gridlabd_config.json $dt
	    cp /rd2c/integration/control/config/topology.json $dt
	    cp /rd2c/integration/control/*.glm $dt
fi

cp -r integration $RD2C 
cp -r RC/code/run.sh ${RD2C}/integration/control 
cp -r RC/code/ns3-helics-grid-dnp3-4G-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3-4G.cc  
cp -r RC/code/ns3-helics-grid-dnp3-5G-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3-5G.cc 
cp -r RC/code/ns3-helics-grid-dnp3-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3.cc  
mkdir ${RD2C}/ns-3-dev/src/dnp3/ 
cp -r RC/code/dnp3/crypto ${RD2C}/ns-3-dev/src/dnp3  
cp -r RC/code/dnp3/dnplib ${RD2C}/ns-3-dev/src/dnp3  
cp -r RC/code/dnp3/examples ${RD2C}/ns-3-dev/src/dnp3 
cp -r RC/code/dnp3/helper ${RD2C}/ns-3-dev/src/dnp3 
mkdir ${RD2C}/ns-3-dev/src/dnp3/model 
cp -r RC/code/dnp3/wscript ${RD2C}/ns-3-dev/src/dnp3/ 
cp -r RC/code/dnp3/model/dnp3-application.h ${RD2C}/ns-3-dev/src/dnp3/model/ 
cp -r RC/code/dnp3/model/dnp3-application-Docker.cc ${RD2C}/ns-3-dev/src/dnp3/model/dnp3-application.cc 
cp -r RC/code/dnp3/model/dnp3-application-Docker.cc ${RD2C}/ns-3-dev/src/dnp3/model/dnp3-application.cc 
cp -r RC/code/dnp3/model/dnp3-simulator-impl.* ${RD2C}/ns-3-dev/src/dnp3/model/ 
cp -r RC/code/dnp3/model/tcptest* ${RD2C}/ns-3-dev/src/dnp3/model/ 
cp -r RC/code/dnp3/model/dnp3-mim-* ${RD2C}/ns-3-dev/src/dnp3/model/ 
#&& cp -r RC/code/applications/model/fncs-application.* ${RD2C}/ns-3-dev/src/applications/model/ 
cp -r RC/code/internet/* ${RD2C}/ns-3-dev/src/internet/ 
cp -r RC/code/lte/* ${RD2C}/ns-3-dev/src/lte/ 
cp -r RC/code/gridlabd/* ${RD2C}/gridlab-d/tape_file/ 
cp -r RC/code/trigger.player ${RD2C}/integration/control/
cd ${RD2C}/PUSH/NATIG
./build_helics.sh
cd ${RD2C}/ns-3-dev
cp -r ${RD2C}/PUSH/NATIG/RC/code/helics/helics-helper* ${RD2C}/ns-3-dev/contrib/helics/helper/
cp -r ${RD2C}/PUSH/NATIG/RC/code/helics/dnp3-application-helper-new.* ${RD2C}/ns-3-dev/contrib/helics/helper/
cp -r ${RD2C}/PUSH/NATIG/RC/code/helics/dnp3-application-new* ${RD2C}/ns-3-dev/contrib/helics/model/
cp -r ${RD2C}/PUSH/NATIG/RC/code/helics/wscript ${RD2C}/ns-3-dev/contrib/helics/


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
./build_ns3.sh "$1" ${RD2C}
