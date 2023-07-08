#!/bin/bash

LDFLAGS="-ljsoncpp -L/usr/local/include/json/"
#./waf clean
./waf configure --with-helics=/usr/local --with-fncs=/rd2c --with-czmq=/rd2c --with-zmq=/rd2c --disable-werror --enable-examples --enable-tests #--disable-python --enable-mpi -d optimized dir=/rd2c/ns-3-dev
./waf build
./waf install
