#!/bin/bash

cd ../
cp TP.txt graph/collected/DDoS50BotsCC.txt 
cp TP-Prob.txt graph/collected-prod/DDoS50BotsCC.txt 
cd graph/ 
python Prod-TP.py 
python plot_TP.py
