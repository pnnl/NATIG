#!/bin/bash
#SBATCH --time=64:15:00

#module load cmake/3.26.3
#module load gcc/8.4.0

export RD2C=${PWD}
export FNCS_INSTALL=${RD2C}
export GLD_INSTALL=${RD2C}
export GLPATH=/rd2c/share/gridlabd/:/rd2c/lib/gridlabd/
export CZMQ_VERSION=4.2.0
export ZMQ_VERSION=4.3.1
export PATH=$PATH:${FNCS_INSTALL}/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib64
export FNCS_LOG_FILE=yes
export FNCS_LOG_STDOUT=yes
export FNCS_LOG_TRACE=yes
export FNCS_LOG_LEVEL=DEBUG1


cd ${RD2C} 
wget http://ftp.mirrorservice.org/sites/sourceware.org/pub/gcc/releases/gcc-8.4.0/gcc-8.4.0.tar.gz 
tar zxf gcc-8.4.0.tar.gz 
cd gcc-8.4.0 
./contrib/download_prerequisites 
./configure --disable-multilib --prefix=${RD2C}
make -j 4  
make install 

wget --no-check-certificate https://github.com/Kitware/CMake/releases/download/v3.20.3/cmake-3.20.3.tar.gz 
tar zxf cmake-3.20.3.tar.gz 
cd cmake-3.20.3 
./bootstrap --prefix=${RD2C}
make 
make install 

# ----------------------------------------------------
# INSTALL ZMQ and BINDINGS for c++
# ----------------------------------------------------

cd ${RD2C} 
wget --no-check-certificate http://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}/zeromq-${ZMQ_VERSION}.tar.gz 
tar -xzf zeromq-${ZMQ_VERSION}.tar.gz 
cd ${RD2C}/zeromq-${ZMQ_VERSION} 
./configure --prefix=${FNCS_INSTALL} 
make 
make install

export CFLAGS=-I${FNCS_INSTALL}/include
export LDFLAGS=-L${FNCS_INSTALL}/lib
export PKG_CONFIG_PATH=${FNCS_INSTALL}/lib/pkgconfig

cd ${RD2C} 
wget --no-check-certificate https://github.com/zeromq/czmq/releases/download/v${CZMQ_VERSION}/czmq-${CZMQ_VERSION}.tar.gz 
tar -xzf czmq-${CZMQ_VERSION}.tar.gz 
cd ${RD2C}/czmq-${CZMQ_VERSION} 
./configure --prefix=${FNCS_INSTALL} 
make 
make install

export CLFAGS=
export LDFLAGS=
export PKG_CONFIG_PATH=

# ----------------------------------------------------
# INSTALL FNCS
# ----------------------------------------------------

cd ${RD2C} 
git config --global http.sslverify false 
git clone -b develop --single-branch https://github.com/GRIDAPPSD/fncs.git 
cd fncs 
./configure --prefix=${FNCS_INSTALL} --with-zmq=${FNCS_INSTALL} 
make 
make install 
cd python 
python setup.py sdist 
pip install --user dist/fncs-2.0.1.tar.gz 
pip3 install --user dist/fncs-2.0.1.tar.gz

#cd ${RD2C}
#wget https://boostorg.jfrog.io/artifactory/main/release/1.83.0/source/boost_1_83_0.tar.gz
#tar -xvf boost_1_83_0.tar.gz
#cd boost_1_83_0
#./bootstrap.sh --prefix=${RD2C} --without-libraries=
#./b2 install | tee install.log

#export CXXFLAGS="-I $(readlink -f ${RD2C}/include)"
#export LDFLAGS="-L $(readlink -f ${RD2C}/lib) -Wl,-rpath=$(readlink -f ${RD2C}/lib)"

cd ${RD2C}
git clone https://github.com/zeromq/libzmq
cd libzmq
./autogen.sh
./configure --prefix=${RD2C}
make -j4
make install

export ZMQ_LIBS=${RD2C}/lib/libzmq.so
export PATH=$PATH:${RD2C}/libzmq/../include
export PATH=$PATH:${RD2C}/libzmq/../lib
export PATH=$PATH:${RD2C}/libzmq/../lib64
export CMAKE_INSTALL_PREFIX=${RD2C}

cd ${RD2C} 
git clone https://github.com/GMLC-TDC/HELICS -b v2.7.1 
cd HELICS 
mkdir build 
cd build 
cmake -DCMAKE_BUILD_TYPE=Release -DHELICS_BUILD_CXX_SHARED_LIB=ON -DCMAKE_INSTALL_PREFIX=${RD2C} ../ 
make 
make install 
pip install --user helics-apps==2.7.1 
pip3 install --user helics-apps==2.7.1

# ----------------------------------------------------
# INSTALL Gridlab-D
# ----------------------------------------------------

cd ${RD2C} 
git clone https://github.com/gridlab-d/gridlab-d.git -b v4.3 --single-branch 
cd ${RD2C}/gridlab-d/third_party 
tar -xzf xerces-c-3.2.0.tar.gz 
cd ${RD2C}/gridlab-d/third_party/xerces-c-3.2.0 
./configure --prefix=${RD2C}
make 
make install 
cd ${RD2C}/gridlab-d 
autoreconf -if 
./configure --prefix=${RD2C} --with-xerces=${RD2C} --with-helics=${RD2C} --with-fncs=${RD2C} --enable-silent-rules 'CFLAGS=-g -O2 -w' 'CXXFLAGS=-g -O2 -w -std=c++14' 'LDFLAGS=-g -O2 -w' 
make 
make install

cd ${RD2C}
git clone https://github.com/open-source-parsers/jsoncpp.git
cd jsoncpp
mkdir -p build/debug
cd build/debug
cmake -DCMAKE_INSTALL_LIBDIR=${RD2C}/lib -DCMAKE_INSTALL_INCLUDEDIR=${RD2C}/include -DCMAKE_BUILD_TYPE=debug -DJSONCPP_LIB_BUILD_SHARED=ON -G "Unix Makefiles" ../../
make
make install

cd ${RD2C}
wget https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-4.1.5.tar.gz
tar -xvf openmpi-4.1.5.tar.gz
cd openmpi-4.1.5/
./configure --prefix=${RD2C}
make
make install

export LDFLAGS="-ljsoncpp -L${RD2C}/include/jsoncpp/"
cd ${RD2C} 
mkdir PUSH 
cd PUSH 
git clone https://github.com/pnnl/NATIG.git 
cd ../
git clone https://github.com/nsnam/ns-3-dev-git.git
mv ns-3-dev-git ns-3-dev
cd ns-3-dev
git checkout ns-3.35
cp -r ../PUSH/NATIG/RC/code/make.sh .
cp -r ../PUSH/NATIG/RC/code/helics-backup/ contrib/helics
cp -r ../PUSH/NATIG/RC/code/fncs/ src
cp -r ../PUSH/NATIG/RC/code/dnp3/ src
cp -r ../PUSH/NATIG/RC/code/applications/* src/applications/
cp -r ../PUSH/NATIG/RC/code/internet/* src/internet/
cp -r ../PUSH/NATIG/RC/code/lte/* src/lte/
cp -r ../PUSH/NATIG/RC/code/point-to-point-layout/* src/point-to-point-layout/
if [ "$1" == "5G" ]; then
    echo "installing 5G"
    cd contrib
    git clone https://gitlab.com/cttc-lena/nr.git
    cd nr
    git checkout 5g-lena-v1.2.y
    cd ../../
    cp -r ../PUSH/NATIG/RC/code/nr/* contrib/nr/
fi
./make ${RD2C}
cd ../
cp -r PUSH/NATIG/integration/ .
cd integration/control/
cp ../../PUSH/NATIG/RC/code/ns3-helics-grid-dnp3-5G.cc .
cp ../../PUSH/NATIG/RC/code/ieee8500.glm .
