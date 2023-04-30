# Helics integration with GridLAB-D and NS3 #

This is a simple project of Helics simulations integrated with GridLAB-D and NS3.
This project has three Helics federates to run as follows:
- Microgrids: Three microgrids (mg1, mg2, and mg3) that send sensor measures to the control center. This Helics federate is run on GridLAB-D. 
- Control Center: Control center that monitors sensor measures sent from microgrids. This Helics federate is run on a simple python code via Helics interface.
- Helics filter: Network simulation that captures all network interactions between three microgrids and the control center. Instead of built-in Helics filters, this project uses NS3 as a filter. The Helics federate is run on NS3. This NS3 model has a star topology where the control center is located in the center and microgrids are located at edges.


## Prerequisites ##

The following should be installed.
- Helics
- NS3
- GridLAB-D
- python
- jsoncpp


## Run ##

Simply, run `run.sh` bash script. This script will run `helics_broker`, `gridlabd`, `ns3`, and `MicrogridControlCentor.py`.


## Description ##

- `cc_config.json`: Helics configuration file for the control center.
- `grid.json`: Network topology configuration file for NS3 nodes
- `gridlabd_config.json`: Helics configuration file for microgrids run on GridLAB-D.
- `killall.sh`: Kill processes run by `run.sh` bash script.
- `library.glm`: GridLAB-D model file dedicated for GridLAB-D configuration as a sub-model of `test_IEEE123.glm`.
- `MicrogridControlCenter.py`: A simple control center logic in python.
- `ns_config.json`: Helics configuration file for NS3 nodes corresponding to `cc_config.json` and `gridlabd_config.json`.
- `ns3-helics-grid.cc`: A NS3 model file. The program need to load `grid.json` and `ns_config.json`.
- `run.sh`: Bash script file that run helics broker and three Helics federates: microgrids (GridLAB-D), filter (NS3), and control center (python).
- `test_IEEE123.glm`: Main GridLAB-D model file based on IEEE 123 test feeder.
