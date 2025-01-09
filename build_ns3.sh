#!/bin/bash

if [ -d "/rd2c/test" ]
then
    rm -rf /rd2c/test/
fi

if [ -d "/rd2c/ns-3-dev" ]
then
    rm -rf /rd2c/ns-3-dev/
fi

if [ -d "/rd2c/ns-3-dev-git" ]
then
    rm -rf /rd2c/ns-3-dev-git/
fi

cd ../../
#mkdir test
#cd test 
git clone https://github.com/nsnam/ns-3-dev-git.git
mv ns-3-dev-git ns-3-dev
cd ns-3-dev
git checkout ns-3.35
cd /rd2c/PUSH/NATIG
./build_helics.sh
cd /rd2c/ns-3-dev
cp -r ../PUSH/NATIG/RC/code/make.sh .
cp -r ../PUSH/NATIG/RC/code/dnp3/ src
cp -r ../PUSH/NATIG/RC/code/dnp3/model/dnp3-application-Docker.cc src/dnp3/model/dnp3-application.cc
cp -r ../PUSH/NATIG/RC/code/internet/ipv4-l3-protocol* src/internet/model/
cp -r ../PUSH/NATIG/RC/code/internet/internet-stack-helper-MIM.* src/internet/helper/
cp -r ../PUSH/NATIG/RC/code/internet/wscript src/internet/
cp -r ../PUSH/NATIG/RC/code/lte/* src/lte/
cp -r ../PUSH/NATIG/RC/code/point-to-point-layout/* src/point-to-point-layout/
mkdir src/dnp3/
cp -r ../PUSH/NATIG/RC/code/dnp3/crypto src/dnp3
cp -r ../PUSH/NATIG/RC/code/dnp3/dnplib src/dnp3
cp -r ../PUSH/NATIG/RC/code/dnp3/examples src/dnp3
cp -r ../PUSH/NATIG/RC/code/dnp3/helper src/dnp3
mkdir src/dnp3/model
cp -r ../PUSH/NATIG/RC/code/dnp3/wscript src/dnp3/
cp -r ../PUSH/NATIG/RC/code/dnp3/model/dnp3-application.h src/dnp3/model/
cp -r ../PUSH/NATIG/RC/code/dnp3/model/dnp3-application-Docker.cc src/dnp3/model/dnp3-application.cc
cp -r ../PUSH/NATIG/RC/code/dnp3/model/dnp3-application-Docker.cc src/dnp3/model/dnp3-application.cc
cp -r ../PUSH/NATIG/RC/code/dnp3/model/dnp3-simulator-impl.* src/dnp3/model/
cp -r ../PUSH/NATIG/RC/code/dnp3/model/tcptest* src/dnp3/model/
cp -r ../PUSH/NATIG/RC/code/dnp3/model/dnp3-mim-* src/dnp3/model/
cp -r ../PUSH/NATIG/RC/code/internet/* src/internet/
cp -r ../PUSH/NATIG/RC/code/lte/* src/lte/
cp -r /rd2c/PUSH/NATIG/RC/code/helics/helics-helper* /rd2c/ns-3-dev/contrib/helics/helper/
cp -r /rd2c/PUSH/NATIG/RC/code/helics/dnp3-application-helper-new.* /rd2c/ns-3-dev/contrib/helics/helper/
cp -r /rd2c/PUSH/NATIG/RC/code/helics/dnp3-application-new* /rd2c/ns-3-dev/contrib/helics/model/
cp -r /rd2c/PUSH/NATIG/RC/code/helics/dnp3-application-new-Docker.cc /rd2c/ns-3-dev/contrib/helics/model/dnp3-application-new.cc
cp -r /rd2c/PUSH/NATIG/RC/code/helics/dnp3-application-new-Docker.h /rd2c/ns-3-dev/contrib/helics/model/dnp3-application-new.h
cp -r /rd2c/PUSH/NATIG/RC/code/helics/wscript /rd2c/ns-3-dev/contrib/helics/
sudo ./make.sh $2
if [ "$1" == "5G" ]; then
    echo "installing 5G"
    cd contrib
    git config --global http.sslverify false
    git clone https://gitlab.com/cttc-lena/nr.git
    cd nr
    git checkout 5g-lena-v1.2.y
    cd ../../
    cp -r $2/PUSH/NATIG/patch/nr/* contrib/nr/
    sudo ./make.sh $2
fi
mkdir $2/integration/control/physicalDevOutput
