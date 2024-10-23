#!/bin/bash
#SBATCH --time=64:15:00

# ==== set root and output
export RD2C=$1
export FNCS_INSTALL=${RD2C}
export PATH=$PATH:${FNCS_INSTALL}/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FNCS_INSTALL}/lib64
export PATH=$PATH:${FNCS_INSTALL}/include
export GLPATH=${RD2C}/lib/gridlabd:${RD2C}/lib

export OMPI_ALLOW_RUN_AS_ROOT=1
export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

ROOT_PATH="${PWD}" # "/rd2c/integration/control"
OUT_DIR="${ROOT_PATH}/tut_output"
if test ! -d ${OUT_DIR}
then
  echo "==== Simulation output folder does not exist yet. Creating ... ===="
  mkdir ${OUT_DIR}
else
  echo "==== Simulation output folder already exists. ===="
fi

# ===== starting HELICS broker =====
helicsLOGlevel=1
helicsOutFile="${OUT_DIR}/helics_broker.log"
if test -e $helicsOutFile
then
  echo "$helicsOutFile exists. Deleting..."
  rm $helicsOutFile
fi

#Copying the points file to the config file
if [[ "$5" == "conf" ]]
then
cp ../../PUSH/NATIG/RC/code/points-${4}/* config/
fi

if [[ "$2" == "4G" ]]
then
if [[ "$5" == "conf" ]]
then
    cp -r ../../PUSH/NATIG/RC/code/4G-conf-${4}/*.json config/
    cp -r ../../PUSH/NATIG/RC/code/4G-conf-${4}/*.glm .
fi
modelName="ns3-helics-grid-dnp3-4G"
fi
if [[ "$2" == "5G" ]]
then
if [[ "$5" == "conf" ]]
then
    cp -r ../../PUSH/NATIG/RC/code/5G-conf-${4}/*.json config/
    cp -r ../../PUSH/NATIG/RC/code/5G-conf-${4}/*.glm .
fi
modelName="ns3-helics-grid-dnp3-5G"
fi
if [[ "$2" == "3G" ]]
then
if [[ "$5" == "conf" ]]
then
    cp -r ../../PUSH/NATIG/RC/code/3G-conf-${4}/*.json config/
    cp -r ../../PUSH/NATIG/RC/code/3G-conf-${4}/*.glm .
fi
modelName="ns3-helics-grid-dnp3"
fi

#sbatch run-helics.sh ${helicsLOGlevel} ${helicsOutFile}
v2=$( grep 'brokerPort' ${PWD}/config/gridlabd_config.json | sed -r 's/^[^:]*:(.*)$/\1/' | sed 's/,//' | sed 's/ //' )

cd ${ROOT_PATH} && \
helics_broker --slowresponding --federates=2 --port=$v2 --loglevel=${helicsLOGlevel} >> ${helicsOutFile} 2>&1 & \
 #helics_app tracer test.txt --config-file endpoints.txt --loglevel 7 --timedelta 1 >> tracer.txt 2>&1 & \
cd -

# ===== starting GridLAB-D ===== 
gldDir="${ROOT_PATH}"
gldOutFile="${OUT_DIR}/gridlabd.log"
if [[ "$4" == "9500" ]]
then
  gldModelFile="${gldDir}/ieee8500.glm"
fi
if [[ "$4" == "123" ]]
then
  gldModelFile="${gldDir}/IEEE_123_Dynamic.glm"
fi
if test -e $gldOutFile
then
  echo "$gldOutFile exists. Deleting..."
  rm $gldOutFile
fi
#sbatch run-gridlabd.sh ${OUT_DIR} ${gldModelFile} ${gldOutFile}
cd ${gldDir} && \
   gridlabd -D OUT_FOLDER=${OUT_DIR} ${gldModelFile} >> ${gldOutFile} 2>&1 & \
   cd -



#========Setting up Python Federate===========
ccDir="${ROOT_PATH}/tutorial"
ccOutFile="${OUT_DIR}/tutorial.log"

if test -e $ccOutFile
then
	echo "$ccOutFile exists. Deleting..."
	rm $ccOutFile
fi

cd ${ccDir} && \
	python example.py>> ${ccOutFile} 2>&1 & \
	cd ${ROOT_PATH}

#======end of python federate===========


exit 0
