# This is a xenial base with python 3.5 as well
# RD2C Base Docker file with GridLabD, NS, Helics configured
# Version 1.0
# Date: 11/21/2021
# Author: Sumit Purohit Sumit.Purohit@pnnl.gov
FROM python:3.7.9-slim-stretch AS builder
#FROM ubuntu:xenial


RUN apt-get update \
    && apt-get install -y \
       wget \
       git \
       automake \
       autoconf \
       make \
       g++-6 \
       gcc-6 \
       libtool \
       ca-certificates \
       openssl \
	   libssl1.0-dev \
       python-dev \ 
       python-pip \
       sudo \
       vim \
       # helics \
       libboost-dev \
       libzmq5-dev \
       m4 \
    && sudo /usr/bin/update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 1 \
    && sudo /usr/bin/update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-6 1 \
    && sudo sudo /usr/bin/update-alternatives  --set gcc /usr/bin/gcc-6 \
    && sudo sudo /usr/bin/update-alternatives  --set g++ /usr/bin/g++-6 \
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
ENV FNCS_INSTALL=${RD2C}
ENV GLD_INSTALL=${RD2C}
ENV GLPATH=/rd2c/share/gridlabd/:/rd2c/lib/gridlabd/
ENV CZMQ_VERSION 4.2.0
ENV ZMQ_VERSION 4.3.1
ENV TEMP_DIR=/tmp/source

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib

ENV FNCS_LOG_FILE=yes
ENV FNCS_LOG_STDOUT=yes
ENV FNCS_LOG_TRACE=yes
ENV FNCS_LOG_LEVEL=DEBUG1

ENV PYHELICS_INSTALL=/usr/local

# ----------------------------------------------------
# INSTALL ZMQ and BINDINGS for c++
# ----------------------------------------------------

RUN cd ${RD2C} \
    && wget --no-check-certificate http://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}/zeromq-${ZMQ_VERSION}.tar.gz \
    && tar -xzf zeromq-${ZMQ_VERSION}.tar.gz \
    && cd ${RD2C}/zeromq-${ZMQ_VERSION} \
    && ./configure --prefix=${FNCS_INSTALL} \
    && make \
    && make install 

ENV CFLAGS=-I${FNCS_INSTALL}/include
ENV LDFLAGS=-L${FNCS_INSTALL}/lib
ENV PKG_CONFIG_PATH=${FNCS_INSTALL}/lib/pkgconfig

RUN cd ${RD2C} \
    && wget --no-check-certificate https://github.com/zeromq/czmq/releases/download/v${CZMQ_VERSION}/czmq-${CZMQ_VERSION}.tar.gz \
    && tar -xzf czmq-${CZMQ_VERSION}.tar.gz \
    && cd ${RD2C}/czmq-${CZMQ_VERSION} \
    && ./configure --prefix=${FNCS_INSTALL} \
    && make \
    && make install 

ENV CLFAGS=
ENV LDFLAGS=
ENV PKG_CONFIG_PATH=

# ----------------------------------------------------
# INSTALL FNCS
# ----------------------------------------------------

RUN cd ${RD2C} \
    && git config --global http.sslverify false \
    && git clone -b develop --single-branch https://github.com/GRIDAPPSD/fncs.git \
    && cd fncs \
    && ./configure --prefix=${FNCS_INSTALL} --with-zmq=${FNCS_INSTALL} \
    && make \
    && make install \
    && cd python \
    && python setup.py sdist \
    && pip install dist/fncs-2.0.1.tar.gz \
    && pip3 install dist/fncs-2.0.1.tar.gz 

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
# INSTALL Gridlab-D
# ----------------------------------------------------

RUN cd ${RD2C} \
   && git clone https://github.com/gridlab-d/gridlab-d.git -b v4.3 --single-branch \
   && cd ${RD2C}/gridlab-d/third_party \
   && tar -xzf xerces-c-3.2.0.tar.gz \
   && cd ${RD2C}/gridlab-d/third_party/xerces-c-3.2.0 \
   && ./configure \
   && make \
   && make install \
   && cd ${RD2C}/gridlab-d \
   && autoreconf -if \
   && ./configure --with-helics=/usr/local --prefix=$GLD_INSTALL --with-fncs=/rd2c --enable-silent-rules 'CFLAGS=-g -O2 -w' 'CXXFLAGS=-g -O2 -w -std=c++14' 'LDFLAGS=-g -O2 -w' \
   && make \
   && make install 
#    && cd /tmp \
#    && /bin/rm -r ${RD2C}/gridlab-d


# ----------------------------------------------------
# INSTALL NS-3
# ----------------------------------------------------	

# RUN cd ${RD2C} \
#    && git clone https://github.com/open-source-parsers/jsoncpp.git \
#    && cd jsoncpp \
#    && mkdir -p build/debug \
#    && cd build/debug \
#    && cmake -DCMAKE_BUILD_TYPE=debug -DJSONCPP_LIB_BUILD_STATIC=ON-DJSONCPP_LIB_BUILD_SHARED=OFF -G "Unix Makefiles" ../.. \
#    && make ; make install

RUN apt-get update && apt-get install -y libjsoncpp-dev 
ENV LDFLAGS="-ljsoncpp -L/usr/local/include/jsoncpp/"
RUN cd $RD2C \
    && git clone https://stash.pnnl.gov/scm/hagen/ns-3-dev.git \
	&& cd ns-3-dev \
    && git checkout OceaneBranch \
    #&& git clone https://github.com/GMLC-TDC/helics-ns3 contrib/helics \
	#&& chmod -R 777 contrib/helics \
    && git clone https://stash.pnnl.gov/scm/hagen/helics-ns3.git contrib/helics \
	&& cd contrib/helics \
	&& git checkout dev-dnp3 \
    && cd - \
    #&& cp -r scratch-helics/*.cc scratch \
	&& ./waf configure --with-helics=/usr/local --with-fncs=/rd2c --with-czmq=/rd2c --with-zmq=/rd2c --disable-werror --enable-examples --enable-tests \
    && ./waf build 

RUN cd $RD2C \
    #&& git clone https://stash.pnnl.gov/scm/hagen/cps_modeling.git \
    #&& cd cps_modeling \
    && git clone git@github.com:pnnl/NATIG.git \
    && cd NATIG \
    && cp -r integration $RD2C \
    && cd ${RD2C}/integration 

RUN apt-get update && apt-get install -y procps
# ----------------------------------------------------
# INSTALL Java
# ----------------------------------------------------

RUN apt update \
     && mkdir -p /usr/share/man/man1 \
     && apt install -y openjdk-8-jdk-headless \
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
