#!/bin/bash

LDFLAGS="-ljsoncpp -L/usr/local/include/json/"
#./waf clean
./waf configure --with-helics=/usr/local --with-fncs=/rd2c --with-czmq=/rd2c --with-zmq=/rd2c --disable-werror --enable-examples --enable-tests --enable-mpi 
./waf build
./waf install
