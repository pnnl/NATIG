#!/bin/bash

export RD2C=${PWD}
export FNCS_INSTALL=${RD2C}
export PATH=$PATH:${FNCS_INSTALL}/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib64
export PATH=$PATH:${FNCS_INSTALL}/include
export GLPATH=${PWD}/lib/gridlabd:${RD2C}/lib
