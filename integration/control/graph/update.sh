#!/bin/bash

cd ../
cp TP.txt graph/collected/Baseline.txt 
cp TP-Prob.txt graph/collected-prod/Baseline.txt 
cd graph/ 
python Prod-TP.py 
python plot_TP.py
