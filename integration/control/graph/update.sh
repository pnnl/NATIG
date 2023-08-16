#!/bin/bash

cd ../
cp TP.txt graph/collected/DDoS400Bots4MIM.txt 
cp TP-Prob.txt graph/collected-prod/DDoS400Bots4MIM.txt 
cd graph/ 
python Prod-TP.py 
python plot_TP.py
