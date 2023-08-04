#!/bin/bash

cd ../
cp TP.txt graph/collected/DDoS100Bots.txt 
cp TP-Prob.txt graph/collected-prod/DDoS100Bots.txt 
cd graph/ 
python Prod-TP.py 
python plot_TP.py
