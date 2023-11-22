#!/bin/bash

if [ -d "/rd2c/test" ]
then
    rm -r /rd2c/test/
fi

if [ -d "/rd2c/ns-3-dev" ]
then
    rm -r /rd2c/ns-3-dev/
fi

if [ -d "/rd2c/ns-3-dev-git" ]
then
    rm -r /rd2c/ns-3-dev-git/
fi

cd ../../
#mkdir test
#cd test 
git clone https://github.com/nsnam/ns-3-dev-git.git
mv ns-3-dev-git ns-3-dev
cd ns-3-dev
git checkout ns-3.35
#cp -r ../PUSH/NATIG/patch/make.sh .
#cp -r ../PUSH/NATIG/patch/helics-backup/ contrib/helics
#cp -r ../PUSH/NATIG/patch/fncs/ src
#cp -r ../PUSH/NATIG/patch/dnp3/ src
#cp -r ../PUSH/NATIG/patch/applications/* src/applications/
#cp -r ../PUSH/NATIG/patch/internet/* src/internet/
#cp -r ../PUSH/NATIG/patch/lte/* src/lte/
#cp -r ../PUSH/NATIG/patch/point-to-point-layout/* src/point-to-point-layout/
cp -r ../PUSH/NATIG/RC/code/make.sh .
cp -r ../PUSH/NATIG/RC/code/helics-backup/ contrib/helics
cp -r ../PUSH/NATIG/RC/code/fncs/ src
cp -r ../PUSH/NATIG/RC/code/dnp3/ src
cp -r ../PUSH/NATIG/RC/code/applications/* src/applications/
cp -r ../PUSH/NATIG/RC/code/internet/* src/internet/
cp -r ../PUSH/NATIG/RC/code/lte/* src/lte/
cp -r ../PUSH/NATIG/RC/code/point-to-point-layout/* src/point-to-point-layout/
sudo ./make.sh $2
if [ "$1" == "5G" ]; then
    echo "installing 5G"
    cd contrib
    git clone https://gitlab.com/cttc-lena/nr.git
    cd nr
    git checkout 5g-lena-v1.2.y
    cd ../../
    cp -r ../PUSH/NATIG/patch/nr/* contrib/nr/
    sudo ./make.sh $2
fi
#sudo ./make.sh
