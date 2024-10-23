#!/bin/bash

RD2C=$1
cp -r tutorial ${RD2C}/integration/control/
cp -r tutorial.sh ${RD2C}/integration/control/
echo "Saving the current configuration files"
dt=$(date '+%d-%m-%Y-%H:%M:%S')
mkdir $dt
cp ${RD2C}/integration/control/*.glm $dt
cp ${RD2C}/integration/control/config/*.json $dt
echo "Updating the configuration files with the python federate examples"
cp -r glm/*.glm ${RD2C}/integration/control/
cp -r glm/*.json ${RD2C}/integration/control/config/
