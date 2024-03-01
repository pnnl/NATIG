# This is a xenial base with python 3.5 as well
# RD2C Base Docker file with GridLabD, NS, Helics configured
# Version 1.0
# Date: 11/21/2021
# Author: Sumit Purohit Sumit.Purohit@pnnl.gov
FROM python:3.6-slim AS builder
#FROM ubuntu:xenial

RUN apt-get update \
    && apt-get install --allow-unauthenticated -y \
       wget \
       git \
       automake \
       autoconf \
       make \
       g++ \
       gcc \
       libtool \
       ca-certificates \
       openssl \
	   libssl-dev \
       python3 \ 
       python3-pip \
       sudo \
       vim \
       # helics \
       libboost-dev \
       libzmq5-dev \
       m4 \
    #&& sudo /usr/bin/update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc 1 \
    #&& sudo /usr/bin/update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++ 1 \
    #&& sudo sudo /usr/bin/update-alternatives  --set gcc /usr/bin/gcc \
    #&& sudo sudo /usr/bin/update-alternatives  --set g++ /usr/bin/g++ \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /var/cache/apt/archives/*
	
# Build and install gcc 8
RUN cd /opt \
    && wget http://ftp.mirrorservice.org/sites/sourceware.org/pub/gcc/releases/gcc-8.4.0/gcc-8.4.0.tar.gz \
    && tar zxf gcc-8.4.0.tar.gz \
    && cd gcc-8.4.0 \
    && ./contrib/download_prerequisites \
    && apt -y install build-essential \
    && ./configure --disable-multilib \
    && make -j 4 \
    && make install \
    && cd \
    && rm -rf /opt/gcc-8.4.0 /opt/gcc-8.4.0.tar.gz \
    && sudo /usr/bin/update-alternatives --install /usr/bin/gcc gcc /usr/local/bin/gcc 1 \
    && sudo /usr/bin/update-alternatives --install /usr/bin/g++ g++ /usr/local/bin/g++ 1 \
    && sudo sudo /usr/bin/update-alternatives  --set gcc /usr/local/bin/gcc \
    && sudo sudo /usr/bin/update-alternatives  --set g++ /usr/local/bin/g++

# Build and install cmake >= 10 
RUN cd /opt \
    && wget --no-check-certificate https://github.com/Kitware/CMake/releases/download/v3.20.3/cmake-3.20.3.tar.gz \
    && tar zxf cmake-3.20.3.tar.gz \
    && cd cmake-3.20.3 \
    && ./bootstrap \
    && make \
    && make install \
    && cd \
    && rm -rf /opt/cmake-3.20.3 cmake-3-20.3.tar.gz

WORKDIR /rd2c

ENV RD2C=/rd2c
ENV GLD_INSTALL=${RD2C}
ENV GLPATH=/rd2c/share/gridlabd/:/rd2c/lib/gridlabd/
ENV CZMQ_VERSION 4.2.0
ENV ZMQ_VERSION 4.3.1
ENV TEMP_DIR=/tmp/source

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${RD2C}/lib


ENV PYHELICS_INSTALL=/usr/local

# ----------------------------------------------------
# INSTALL ZMQ and BINDINGS for c++
# ----------------------------------------------------
RUN cd ${RD2C}

RUN cd ${RD2C} \
    && wget --no-check-certificate http://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}/zeromq-${ZMQ_VERSION}.tar.gz \
    && tar -xzf zeromq-${ZMQ_VERSION}.tar.gz \
    && cd ${RD2C}/zeromq-${ZMQ_VERSION} \
    && ./configure --prefix=${RD2C} \
    && make \
    && make install 

ENV CFLAGS=-I${RD2C}/include
ENV LDFLAGS=-L${RD2C}/lib
ENV PKG_CONFIG_PATH=${RD2C}/lib/pkgconfig

RUN cd ${RD2C} \
    && wget --no-check-certificate https://github.com/zeromq/czmq/releases/download/v${CZMQ_VERSION}/czmq-${CZMQ_VERSION}.tar.gz \
    && tar -xzf czmq-${CZMQ_VERSION}.tar.gz \
    && cd ${RD2C}/czmq-${CZMQ_VERSION} \
    && ./configure --prefix=${RD2C} \
    && make \
    && make install 

ENV CLFAGS=
ENV LDFLAGS=
ENV PKG_CONFIG_PATH=



# ----------------------------------------------------
# INSTALL Helics
# ----------------------------------------------------


RUN cd ${RD2C} \
    && git clone https://github.com/GMLC-TDC/HELICS -b v2.7.1 \ 
    && cd HELICS \
    && mkdir build \
    && cd build \
    && cmake -DHELICS_BUILD_CXX_SHARED_LIB=ON ../ \
    && make \
    && make install \ 
    #&& cd /tmp \
    #&& /bin/rm -r ${TEMP_DIR}/HELICS \ 
    && pip install helics-apps==2.7.1 \
    && pip3 install helics-apps==2.7.1 
    #&& pip install helics==3.0.0 \
    #&& pip3 install helics==3.0.0 \
    #&& cd ${RD2C}

#    && pip install --trusted-host pypi.python.org --trusted-host=files.pythonhosted.org --upgrade pip \
#    && pip install --trusted-host=files.pythonhosted.org helics-apps==2.7.1 \
#    && pip3 install --trusted-host=files.pythonhosted.org helics-apps==2.7.1 \
#    && pip install --trusted-host=files.pythonhosted.org helics==2.7.1 \
#    && pip3 install --trusted-host=files.pythonhosted.org helics==2.7.1 

# ----------------------------------------------------
# DOWNLOAD Gridlab-D
# ----------------------------------------------------

RUN cd ${RD2C} \
   && git clone https://github.com/gridlab-d/gridlab-d.git -b v4.3 --single-branch \
   && cd ${RD2C}/gridlab-d/third_party \
   && tar -xzf xerces-c-3.2.0.tar.gz \


# ----------------------------------------------------
# INSTALL NS-3
# ----------------------------------------------------	

#Download NS3
RUN cd $RD2C \
    && mkdir PUSH \
    && cd PUSH \
    && git clone https://github.com/pnnl/NATIG.git 

#Update code in NS3 and Gridlabd
RUN cd $RD2C \
    && git clone https://github.com/pnnl/NATIG.git \
    && cd NATIG \
    && cp -r integration $RD2C \
    && cp -r RC/code/run.sh ${RD2C}/integration/control \
    && cp -r RC/code/ns3-helics-grid-dnp3-4G-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3-4G.cc  \
    && cp -r RC/code/ns3-helics-grid-dnp3-5G-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3-5G.cc  \
    && cp -r RC/code/ns3-helics-grid-dnp3-Docker.cc ${RD2C}/integration/control/ns3-helics-grid-dnp3.cc  \
    && cp -r RC/code/dnp3/wscript ${RD2C}/ns-3-dev/src/dnp3/ \
    && cp -r RC/code/applications/model/fncs-application.* ${RD2C}/ns-3-dev/src/applications/model/ \
    && cp -r RC/code/dnp3/model/dnp3-application.h ${RD2C}/ns-3-dev/src/dnp3/model/ \
    && cp -r RC/code/dnp3/model/dnp3-application-Docker.cc ${RD2C}/ns-3-dev/src/dnp3/model/dnp3-application.cc \
    && cp -r RC/code/helics-backup/model/dnp3-application-new.h ${RD2C}/ns-3-dev/contrib/helics/model/ \
    && cp -r RC/code/helics-backup/model/dnp3-application-new-Docker.cc ${RD2C}/ns-3-dev/contrib/helics/model/dnp3-application-new.cc \
    && cp -r RC/code/helics-backup/model/dnp3-helics-application-Docker.cc ${RD2C}/ns-3-dev/contrib/helics/model/dnp3-helics-application.cc \
    && cp -r RC/code/internet/* ${RD2C}/ns-3-dev/src/internet/ \
    && cp -r RC/code/lte/* ${RD2C}/ns-3-dev/src/lte/ \
    && cp -r RC/code/dnp3/model/dnp3-application-Docker.cc ${RD2C}/ns-3-dev/src/dnp3/model/dnp3-application.cc \
    && cp -r RC/code/gridlabd/* ${RD2C}/gridlab-d/tape_file/ \
    && cp -r RC/code/trigger.player ${RD2C}/integration/control/ 

# Install Gridlabd
RUN cd ${RD2C}/gridlab-d/third_party/xerces-c-3.2.0 \
   && ./configure \
   && make \
   && make install \
   && cd ${RD2C}/gridlab-d \
   && autoreconf -if \
   && ./configure --with-helics=/usr/local --prefix=$GLD_INSTALL --enable-silent-rules 'CFLAGS=-g -O2 -w' 'CXXFLAGS=-g -O2 -w -std=c++14' 'LDFLAGS=-g -O2 -w' \
   && make \
   && make install

RUN apt-get update && apt-get install -y libjsoncpp-dev

RUN apt-get update && apt-get install -y gcc

RUN apt-get install -y openmpi-bin openmpi-common libopenmpi-dev libgtk2.0-dev

#Finish installing NS3
ENV LDFLAGS="-ljsoncpp -L/usr/local/include/jsoncpp/"
RUN cd $RD2C/PUSH/NATIG \
    && ./build_ns3.sh "" ${RD2C} 

RUN apt-get update && apt-get install -y procps

RUN cd ${RD2C}/integration/control/ \
    && mkdir collected_data

# ----------------------------------------------------
# INSTALL Java
# ----------------------------------------------------

RUN apt update \
     && mkdir -p /usr/share/man/man1 \
     && sudo apt-get install -y openjdk-11-jdk \
     # && apt install -y openjdk-8-jdk-headless \
     && apt-get clean

# ----------------------------------------------------
# Set the JAVA_HOME variable 
# ----------------------------------------------------

ENV JAVA_HOME /usr/lib/jvm/java-8-openjdk-amd64

# ----------------------------------------------------
# Copy model files 
# ----------------------------------------------------

ADD input $RD2C/input
RUN mkdir $RD2C/output_gridlabd

ENV PATH=/rd2c/bin:/rd2c/lib:/usr/local/bin:/usr/local/sbin:/usr/sbin:/usr/bin:/sbin:/bin
ENV LD_LIBRARY_PATH=/usr/local/lib64

WORKDIR /rd2c
CMD sleep infinity
