#!/bin/bash

# ==== set root and output 
ROOT_PATH="/rd2c/integration"
OUT_DIR="${ROOT_PATH}/output"
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

cd ${ROOT_PATH} && \
  helics_broker --federates=3 --loglevel=${helicsLOGlevel} >> ${helicsOutFile} 2>&1 & \
  cd -

# ===== starting GridLAB-D ===== 
gldDir="${ROOT_PATH}"
gldOutFile="${OUT_DIR}/gridlabd.log"
cd ${gldDir} && \
   gridlabd -D OUT_FOLDER=${OUT_DIR} ${gldDir}/test_IEEE123.glm  >> ${gldOutFile} 2>&1 & \
   cd -


# ===== starting control center =====
ccDir="${ROOT_PATH}"
ccOutFile="${OUT_DIR}/ctrlCenter.log"
if test -e $ccOutFile
then
  echo "$ccOutFile exists. Deleting..."
  rm $ccOutFile
fi

cd ${ccDir} && \
    python MicrogridControlCenter.py >> ${ccOutFile} 2>&1 & \
    cd -


# ===== setting up ns-3 configurations =====
ns3Dir="/rd2c/ns-3-dev"
ns3Scratch="${ns3Dir}/scratch"
modelName="ns3-helics-grid"
ns3Model="${ns3Scratch}/${modelName}"
helicsConfig="${ROOT_PATH}/ns_config.json"
microGridConfig="${ROOT_PATH}/grid.json"
ns3OutFile="${OUT_DIR}/${modelName}.log"

if test -e $ns3OutFile
  then
    echo "$ns3OutFile exists. Deleting..."
    rm $ns3OutFile
  fi

cd ${ns3Dir} && \
cp ${ROOT_PATH}/${modelName}.cc ${ns3Model}.cc && \
  ./waf --run "scratch/${modelName} --helicsConfig=${helicsConfig} --microGridConfig=${microGridConfig}" >> ${ns3OutFile} 2>&1 & \
  cd -

exit 0