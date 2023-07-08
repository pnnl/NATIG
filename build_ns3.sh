#!/bin/bash

if [ -d "/rd2c/test" ]
then
    rm -r /rd2c/test/
fi

cd ../../
mkdir test
cd test 
git clone https://github.com/nsnam/ns-3-dev-git.git
mv ns-3-dev-git ns-3-dev
cd ns-3-dev
git checkout ns-3.35
cp -r ../../PUSH/NATIG/patch/make.sh .
cp -r ../../PUSH/NATIG/patch/helics-backup/ contrib/helics
cp -r ../../PUSH/NATIG/patch/fncs/ src
cp -r ../../PUSH/NATIG/patch/dnp3/ src
cp -r ../../PUSH/NATIG/patch/applications/* src/applications/
cp -r ../../PUSH/NATIG/patch/internet/* src/internet/
sudo ./make.sh
