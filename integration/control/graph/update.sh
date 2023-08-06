#!/bin/bash

cd ../
cp TP.txt graph/collected/DDoS50BotsUE2.txt 
cp TP-Prob.txt graph/collected-prod/DDoS50BotsUE2.txt 
cd graph/ 
python Prod-TP.py 
python plot_TP.py
