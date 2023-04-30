# Helics integration with GridLAB-D, Helics, ns3, and DNP3 #

This is a project of Helics co-simulations integrated with GridLAB-D and ns3 with DNP3 applications.
This project has two Helics federates: GridLAB-D and NS3. The GridLAB-D model is based on the IEEE 123 Node Test Feeder model. The model is connected to three microgrids (mg1, mg2, mg3) and one substation that send sensor measures to one control center. The control center monitors sensor measurements sent from the microgrids and substation via DNP3. The control center corresponds to a master in DNP3 while the microgrids and substation are outstations in DNP3. The master pull data from the outstations every 4 seconds. The master also send control signal, i.e., direct operate in DNP3.



## Prerequisites ##

To run simulations, the following should be installed.
- Helics
- ns3
- GridLAB-D
- python
- jsoncpp


## Run ##

Simply, run `run.sh` bash script. This script will run `helics_broker`, `gridlabd`, and `ns3`. Log files are located at folder `output`.


## Description ##

- `cc_config.json`: Helics configuration file for the control center.
- `grid.json`: Network topology configuration file for NS3 nodes
- `gridlabd_config.json`: Helics configuration file for microgrids run on GridLAB-D.
- `killall.sh`: Kill processes run by `run.sh` bash script.
- `ns_config.json`: Helics configuration file for NS3 nodes corresponding to `cc_config.json` and `gridlabd_config.json`.
- `ns3-helics-grid-dnp3.cc`: A NS3 model file. The program need to load `grid.json` and `ns_config.json`.
- `run.sh`: Bash script file that run helics broker and three Helics federates: microgrids (GridLAB-D), filter (NS3), and control center (python).
- `IEEE_123_Dynamic.glm`: Main GridLAB-D model file based on IEEE 123 test feeder. It includes other submodules: `IEEE_123_Diesels.glm`, `IEEE_123_Inverters_Mixed.glm`, and `IEEE_123_Recorders.glm`. The model needs climate file `WA-Yakima.tmy2` climate, player files `Gen2_Pref.player`, `Gen3_Pref.player`, `Gen4_Pref.player`.
- `config_helper.py`: A python program that creates configuration files (`cc_config.json`, `grid.json`, `gridlabd_config.json`, `ns_config.json`) and point files (`points_mg1.csv`, `points_mg2.csv`, `points_mg3.csv`, `points_substation.csv`).


