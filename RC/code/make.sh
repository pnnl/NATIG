#!/bin/bash

LDFLAGS="-ljsoncpp -L$1/include/json/"
#./waf clean
./waf configure --prefix=$1 --with-helics=$1  --with-zmq=$1 --disable-werror --enable-examples --enable-tests --enable-mpi --build-profile=optimized 
./waf build
./waf install
