![alt text](https://github.com/pnnl/NATIG/blob/master/NSD_2294_BRAND_HAGEN-NATIG_final_color.png "NATIG")

**N**etwork **A**ttack **T**estbed **I**n [Power] **G**rid (**NATI[P]G**), a co-simulation environment for distribution power grid network using state-of-the-art simulators.

It is a standalone, containerized, and reusable environment to enable cyber analysts and researchers to run different cyber security and performance scenarios on powergrid and generate benchmark datasets. It supports IEEE123 and IEEE9500 as the reference powergrid models with communication model using mesh/ring/star topologies. The communication model supports CSMA, WiFi, 4G, and 5G (optional) protocols. 

We demonstrate few attack scenarios in the default framework and provide ways to design additional scenarios using configuration files.

## Table of contents
[How to get started](https://github.com/pnnl/NATIG/tree/master/README.md#how-to-get-started)

[Overview of out of the box examples](https://github.com/pnnl/NATIG/tree/master/README.md#out-of-the-box-examples)

[Overview of resource impact on the run time of 5G example](https://github.com/pnnl/NATIG/blob/master/README.md#timing-overview-of-examples)

[Detailed timing analysis of out of the box example](https://github.com/pnnl/NATIG/blob/master/TIMING.md)

[Detailed description of out of the box examples and how to run them](https://github.com/pnnl/NATIG/blob/master/EXAMPLES.md)

[How to stop the code](https://github.com/pnnl/NATIG/tree/master/README.md#How-to-stop-the-run)

[How to check if the code is running](https://github.com/pnnl/NATIG/tree/master/README.md#Is-the-code-running)

[Detailed description on how to know if the code is running](https://github.com/pnnl/NATIG/blob/master/DEBUG.md)

[How to update code](https://github.com/pnnl/NATIG/tree/master/README.md#Getting-an-updated-version-of-the-code)

[How to update the setup to run 5G](https://github.com/pnnl/NATIG/tree/master/README.md#How-to-update-the-setup-to-run-5G)

[Example output data](https://github.com/pnnl/NATIG/tree/master/README.md#example-output-data)

[How to run it on a unix cluster](https://github.com/pnnl/NATIG/tree/master/README.md#how-to-run-it-on-a-unix-cluster)

[Detailed description of the available attacks that can simulated in NATIG](https://github.com/pnnl/NATIG/blob/master/ATTACKEXAMPLE.md)

[Overview of simulated attack](https://github.com/pnnl/NATIG/tree/master/README.md#simulated-attacks)

[Citation](https://github.com/pnnl/NATIG/tree/master/README.md#Reference)

## How to get started
1. Have docker installed
2. clone the NATIG repo ```git clone https://github.com/pnnl/NATIG.git```
3. run the following commands:
   ```
   - cd NATIG
   - bash make_run_docker.sh
   ```
   - If you are using MINGW64 (such as gitbash) on windows, please edit the rundocker.sh: ```the input device is not a TTY.  If you are using mintty, try prefixing the command with 'winpty' ```
     
**NOTE: The default docker container does not come with 5G enabled**

Once the setup is done you can run the out of the box examples
1. go to the control folder inside the integration folder that is located in the home rd2c folder (full path: _/rd2c/integration/control_)
2. make sure that all TP*.txt files are removed by running ` rm -r TP*.txt `
3. run sudo bash run.sh _Full path of work directory (example for docker: /rd2c/)_  _[3G|4G|5G]_ _[RC|emptyString]_ _[9500|123]_ _[conf|noconf]_

**Note: example command for docker to run 4G example: ` sudo bash run.sh /rd2c/ 4G "" 9500 conf ` . Change the 3rd parameter to RC when running of docker in a unix cluster that uses slurm. The 4th parameter is the model number that will be used by gridlabd for the simulation.**

When refering to the 3G example, we are talking about topologies that just use a combination of point to point connections, CSMA connections and wifi connections. There is no 4G or 5G in these examples. 

Finally, when collecting data from recorders in gridlabd, with the IEEE 9500 bus model the files get fully populated at the end of the run. _currently under investigation_

## Code structure

The main folder where all the code is run is ```<Root>/integration/control/```. In docker this folder is located at ```/rd2c/integration/control/```.

The examples code containing the different communication topologies are: 
1. ns3-helics-grid-dnp3-5G.cc (5G with NR core)
2. ns3-helics-grid-dnp3-4G.cc (4G LTE)
3. ns3-helics-grid-dnp3.cc (Non cellular, which refer to as 3G)

These .cc files are located in the integration/control folder

The configuration files (The json files) that are used by NATIG to setup the run are read from the integration/control/config folder.

When the "conf" parameter is passed into the bash run command, the json files are copied over from the ``` <Root>/PUSH/NATIG/RC/code/<topology ID>-conf-<GLM ID> folder.

The GLM files are read directly from the integration/control folder. 

## Labels used to describe experiments

### Execution environmnent

**RC**: This label is used to signal the run script that the slurm system will be used so the main run file needs to sleep instead of ending right after the start of the run. Use this label when you want to use the slurm system.

**_Empty string_**: This label is used to signal the run script that docker is used and therefore the main script can stop after the subscripts have started.

### Communication types labels

Non-cellular Networks:

**3G**: This label is used to describe directly connected networks. These networks are networks that make use of ethernet and fiber connections to connect individual nodes in a network to one another. We used the 3G label for this example to keep the labeling consistent with the other experiments.  

Cellular Networks:

**4G**: These are cellular networks that use LTE architecture to transfer data between the control center and the microgrids.

**5G**: This cellular network example leverages the New Radio core, developed by the LENA group, to simulate a simplified version of a non-standalone 5G network as described in research literature with minimal use of the LTE framework.

### IEEE 9500 bus model

The IEEE 9500 bus model simulates a medium-voltage distribution system. An example of such a system is a suburban neighborhood with a mix of houses, apartment buildings, and a small shopping center. 

Composition of a medium-voltage distribution system: 

The power for such a system originates at a high-voltage substation, likely several miles away. The voltage at this substation could be anywhere from 34.5 kV to 138 kV, depending on the local grid configuration. A large transformer located at the edge of the neighborhood steps down the high voltage to a medium voltage level, typically between 12.4 kV and 25 kV. Underground cables, insulated for the medium voltage level, run along main roads or easements throughout the neighborhood.  These cables are the "backbone" of the system, delivering power to various points. At regular intervals along these main feeders, there will be pad-mounted transformers. These transformers further step down the voltage to a low-voltage level (usually 480 V or 240 V) suitable for powering homes and businesses. From the pad-mounted transformers, overhead or underground lines (depending on local regulations and aesthetics) distribute the low voltage power to individual buildings. These lines connect to transformers on utility poles outside each building, which may further reduce the voltage to levels usable by appliances (typically 120 V).

### IEEE 123 bus model

The IEEE 123 bus model simulates a radial distribution system. An example of such a system is a power grid covering a rural town with a mix of farms, houses, and a small school.

Composition of a radial distribution system:

This is a simplified type of power grid where electricity flows from a single source (like a substation) outwards to various consumers like homes and businesses. Unlike a meshed network, there are no redundant paths for electricity to flow. While the specific voltage might vary depending on the model variation, it typically represents a medium-voltage distribution system. This means the voltage level is likely in the range of a few kilovolts (kV) to tens of kV. The IEEE 123 bus model is known for being unbalanced, meaning the loads on each of the three phases (conductors) can be unequal. This is a more realistic representation of real-world distribution systems where loads can vary depending on the types of consumers connected.

Similar to the IEEE 9500 bus model, the power originates at a substation several miles away, likely stepping down the voltage from a high-voltage transmission grid (around 34.5 kV to 138 kV) to a medium voltage level (around 12.4 kV to 25 kV). Once the power has been generated, it is sent through a single overhead power line, insulated for medium voltage, that runs from the substation towards the town, acting as the backbone of the system. At key points along the main feeder, transformers are located on utility poles. These transformers step down the voltage again to a low voltage level (typically 480 V or 240 V) suitable for powering homes and businesses. From these transformers, smaller overhead or underground lines branch out, reaching individual buildings. These lines connect to transformers on poles outside each building, further reducing the voltage to levels usable by appliances (typically 120 V).

IEEE 123 vs IEEE 9500 bus models:

Compared to the more complex IEEE 9500 bus model, the 123 bus system represents a smaller and less geographically expansive area. It's often used for studying specific aspects of distribution system behavior, such as voltage regulation, power losses, or protection schemes.


## Out of the box examples

The following table is for both the 9500 and 123 bus models:

| Toplogy Type | Development Stage |
|---|---|
| 3G Star | Works for DDos and MIM
| 3G Mesh | Works for MIM and DDoS
| 3G Ring | Works for MIM and DDoS
| 4G Mesh and Star hybrid | Works for DDoS and MIM
| 5G Mesh and Star hybrid | Works for DDoS and MIM

NOTE: Mesh and Start hybrid means that there is a Mesh topology connecting the micrigrids to the 4G/5G network in a all to all connection and a Star topology connecting the control center to the 4G/5G network. 

Table containing the status for the MIM examples:

| Example | Description | Development Stage | 
|---|---|---|
| 3G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using directly connected network | Works
| 3G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using directly connected network | Works
| 4G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using 4G network | Works
| 4G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using 4G network | Works
| 5G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using 5G network | Works
| 5G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using 5G network | Works


Table containing the status for the DDoS examples:

| Example | Description | Development Stage | 
|---|---|---|
| 3G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using directly connected network | Works
| 3G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using directly connected network | Works
| 4G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using 4G network | Works
| 4G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using 4G network | Works
| 5G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using 5G network | Works
| 5G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using 5G network | Works

### Timing overview of examples:

Running the 5G example with the 9500 bus model and the debug statements turned off and the data collection in turned off

| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) on slurm | Time (s) on docker | 
|---|---|---|---|---|---|---|
| 5G | 9500 | 45 | 121 | no attack | 7026.49 | 45732.18 |
| 5G | 9500 | 45 | 121 | DDoS with 2 attackers | 8320.26 | 74192.72 |
| 5G | 9500 | 45 | 121 | MIM with 2 attackers | 8143.94 | 31666.34 |

### Interesting outputted data:

1. TP.txt this file contains the performance of each path full path between the control center and the substation. To read this file only take the 20 last inputs per timesteps. if the number of substation changes, only read the last 2 X the number of substations rows per timesteps.

  - column IDs in the file: Timesteps,  path ID , ( sourceAddress / sourcePort --> destinationAddress / destinationPort ) , Throughput of path, lostPackets, Total received bytes since the start of the simulation , Total transmitted bytes since the start of the simulation, loss packet rate, delay per received packets, total transmitted packets since the start of the simulation ,total received packets since the start of the simulation, jitter per received packet


## How to stop the run?

If running using Docker, to stop the run run ` bash killall.sh ` to make sure most threads stop

  - Once script has finished runing run ` ps aux | grep root ` to check that no other thread related to ns3 is still running

  - If some threads have been found run ` kill -9 \<ID number\> ` to kill the remaining thread

## Is the code running?

Once the code has started there are several ways to track the progress:

1. run ` ps aux | grep root ` to see if you can see ns3, helics and gridlabd threads

2. run ` tail -\< Number of lines chosen \> output/ns3-helics-grid-dnp3-\< chosen example ID\>.log ` to see the most recent outputed lines from the run

3. run ` cat TP.txt ` to see the throughput monitoring file being populated

## Getting an updated version of the code

If you want to get an updated version of the NATIG repository run the following instructions (__Currently being tested__):
NOTE: Make sure that there are no un-commited changes in the PUSH/NATIG folder. If you do have some changes just run ` git stash `
NOTE2: before running ` git stash ` make sure that you have a copy of the config files that you changed somewhere else.
```
cd /rd2c/PUSH/NATIG/
# 5G disabled
./update_workstation.sh 4G
# 5G enabled
./update_workstation.sh 5G
```

## How to update the setup to run 5G

To enable 5G capabilities:
1. request access to https://gitlab.com/cttc-lena/nr
2. run ``` ./build_ns3.sh 5G /<root folder that the simulation is run on>/ ``` from the NATIG folder in the PUSH folder
  - in the case of Docker run ``` ./build_ns3.sh 5G /rd2c/ ```

## Example output data

NS3 example output data:

Code generates a file containing the performance of paths between the Substations and the Control center

Location: File called TP.txt is located in integration/control/ folder

Column IDs: Timesteps,  path ID , ( sourceAddress / sourcePort --> destinationAddress / destinationPort ) , Throughput of path, lostPackets, Total received bytes since the start of the simulation , Total transmitted bytes since the start of the simulation, loss packet rate, delay per received packets, total transmitted packets since the start of the simulation ,total received packets since the start of the simulation, jitter per received packet

Example output for DDoS out of the box example on 5G network:

```
1.05 20000 (UDP 1.0.0.2 / 20000 --> 172.17.0.3 / 20000) 62.4727 0 343 343 0 0.0027614 7 7 0.00143351
1.05 40000 (UDP 172.107.0.3 / 20000 --> 1.0.0.2 / 40000) 2409.84 0 9918 21028 0.528571 0.00702642 70 33 0.000376239
1.05 40001 (UDP 172.108.0.3 / 20000 --> 1.0.0.2 / 40001) 3654.13 0 15578 25676 0.395604 0.00445615 91 55 0.000375062
1.05 40002 (UDP 172.109.0.3 / 20000 --> 1.0.0.2 / 40002) 5027.92 0 24951 33663 0.258929 0.00431755 112 83 0.000253965
1.05 40003 (UDP 172.110.0.3 / 20000 --> 1.0.0.2 / 40003) 5094.66 0 21982 36883 0.404762 0.00457615 126 75 0.000253877
1.05 40004 (UDP 172.111.0.3 / 20000 --> 1.0.0.2 / 40004) 5634.63 0 26154 44226 0.408163 0.00504666 147 87 0.000363581
1.05 40005 (UDP 172.112.0.3 / 20000 --> 1.0.0.2 / 40005) 4569.61 0 16290 53844 0.697802 0.00578028 182 55 0.00045446
1.05 40006 (UDP 172.113.0.3 / 20000 --> 1.0.0.2 / 40006) 12935.8 0 50112 59521 0.157635 0.00483181 203 171 0.000196482
1.05 40007 (UDP 172.114.0.3 / 20000 --> 1.0.0.2 / 40007) 7145.32 0 31503 71393 0.558824 0.00595131 238 105 0.000302485
1.05 40008 (UDP 172.115.0.3 / 20000 --> 1.0.0.2 / 40008) 10829.2 0 49198 84511 0.417857 0.00569313 280 163 0.000209076
1.05 40009 (UDP 172.116.0.3 / 20000 --> 1.0.0.2 / 40009) 13533.5 0 58298 100436 0.419643 0.00586743 336 195 0.000153357
1.05 49153 (UDP 1.0.0.2 / 49153 --> 172.116.0.3 / 20000) 62.474 0 343 343 0 0.00247497 7 7 0.00129082
1.05 49154 (UDP 1.0.0.2 / 49154 --> 172.105.0.3 / 20000) 62.4741 0 343 343 0 0.00190355 7 7 0.00100508
1.05 49155 (UDP 1.0.0.2 / 49155 --> 172.83.0.3 / 20000) 62.4741 0 343 343 0 0.00333212 7 7 0.00171937
1.05 49156 (UDP 1.0.0.2 / 49156 --> 172.61.0.3 / 20000) 62.4741 0 343 343 0 0.00161216 7 7 0.000719382
1.05 49157 (UDP 1.0.0.2 / 49157 --> 172.50.0.3 / 20000) 62.4741 0 343 343 0 0.00190357 7 7 0.00100508
1.05 49158 (UDP 1.0.0.2 / 49158 --> 172.28.0.3 / 20000) 62.4741 0 343 343 0 0.00190355 7 7 0.0010051
1.05 49159 (UDP 1.0.0.2 / 49159 --> 172.72.0.3 / 20000) 62.4741 0 343 343 0 0.00390983 7 7 0.00200508
1.05 49160 (UDP 1.0.0.2 / 49160 --> 172.39.0.3 / 20000) 62.4741 0 343 343 0 0.0011836 7 7 0.000290798
1.05 49161 (UDP 1.0.0.2 / 49161 --> 172.94.0.3 / 20000) 62.4741 0 343 343 0 0.00218926 7 7 0.00114794
1.1 20000 (UDP 1.0.0.2 / 20000 --> 172.17.0.3 / 20000) 58.3214 0 686 686 0 0.0018276 14 14 0.000716774
1.1 40000 (UDP 172.107.0.3 / 20000 --> 1.0.0.2 / 40000) 2944.77 0 31550 42056 0.25 0.00614299 140 105 0.000432111
...
...
1.1 49156 (UDP 1.0.0.2 / 49156 --> 172.61.0.3 / 20000) 58.322 0 686 686 0 0.00125248 14 14 0.000359708
1.1 49157 (UDP 1.0.0.2 / 49157 --> 172.50.0.3 / 20000) 58.322 0 686 686 0 0.00139818 14 14 0.00050255
1.1 49158 (UDP 1.0.0.2 / 49158 --> 172.28.0.3 / 20000) 58.322 0 686 686 0 0.00139817 14 14 0.000502563
1.1 49159 (UDP 1.0.0.2 / 49159 --> 172.72.0.3 / 20000) 58.322 0 686 686 0 0.00240132 14 14 0.00100255
1.1 49160 (UDP 1.0.0.2 / 49160 --> 172.39.0.3 / 20000) 58.322 0 686 686 0 0.0010382 14 14 0.00014542
1.1 49161 (UDP 1.0.0.2 / 49161 --> 172.94.0.3 / 20000) 58.3221 0 686 686 0 0.00154102 14 14 0.000573984
```


Gridlabd example output data:

TBD

## How to run it on a unix cluster

Step 1: move the RC folder outside of the repository folder. 

Step 2: go to the RC folder and run ./make.sh

Step 3: WIP note make sure that all hard coded links are changed to the location of your code. _Will be fixed soon_ 

Step 4: run the make.sh bash script located in the ns-3-dev folder with the location of the RC folder as an input: ` ./make.sh _location of RC folder_ `

If your system has slurm nodes:

Command to run the simulation: ` sbatch --exclusive run.sh _location of RC folder_ _[3G|4G|5G]_ _[RC|emptyString]_ _[9500|123]_ _[conf|noconf]_ `

If your system does not have slurm nodes:

Command to run the simulation: ` ./run.sh _location of RC folder_  _[3G|4G|5G]_ _[RC|emptyString]_ _[9500|123]_ _[conf|noconf]_ `


## Simulated attacks

### Currently working
1. Injection attacks (Man-in-the-middle): The attacker trips relays connecting the microgrids to one another and to the grid causing islanding of the microgrid 
2. Parameter changes (Man-in-the-middle): The attacker modifies the Pref and Qref values of two inverters in Microgrid 1 while the microgrids are islanded
3. DDoS: The attacker generates a number of bots that will flood 1 to n nodes in the network to slow down the network performance
4. Grid islanding (Man-In-The-Middle): The attacker can trip the switches that connects the microgrids to other microgrids and the grid. This causes the microgrids to rely on the internal generation.  

### In progress
1. MPI capabilities for the code


### Reference
```
@article{bel2023co,
  title={Co-Simulation Framework For Network Attack Generation and Monitoring},
  author={Bel, Oceane and Kim, Joonseok and Hofer, William J and Maharjan, Manisha and Purohit, Sumit and Niddodi, Shwetha},
  journal={arXiv preprint arXiv:2307.09633},
  year={2023}
}
```
