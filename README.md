**N**etwork **A**ttack **T**estbed **I**n [Power] **G**rid (**NATI[P]G**), a co-simulation environment for distribution power grid network using state-of-the-art simulators.

It is a standalone, containerized, and reusable environment to enable cyber analysts and researchers to run different cyber security and performance scenarios on powergrid.

## How to get started
1. Have docker installed
2. clone the NATIG repo
3. run the following commands:
   - cd NATIG
   - ./buildimage.sh 
   - Once the docker finishes building successfully, run: ./rundocker.sh


NOTE: The default docker container does not come with 5G enabled


To enable 5G capabilities:
1. request access to https://gitlab.com/cttc-lena/nr
2. run ``` ./build_ns3.sh 5G ``` from the NATIG folder in the PUSH folder
3. Some updates that need to be done to the code before it can be compiled:
   - the nr-gnb-net-device.cc in the model folder needs the following function:   
     ```
     std::vector<uint16_t>
     NrGnbNetDevice::GetCellIds () const
     {
	    std::vector<uint16_t> cellIds;
	
	    for (auto &it: m_ccMap)
	    {
		    cellIds.push_back (it.second->GetCellId ());
	    }
	    return cellIds;
     }
     ```
     
     NOTE: dont forget to add **std::vector<uint16_t> GetCellIds () const** to nr-gnb-net-device.h
  
4. return to the main ns-3-dev folder
5. run sudo ./make.sh


Once the setup is done you can start to run the code
1. go to the control folder inside the integration folder that is located in the home rd2c folder (full path: _/rd2c/integration/control_)
2. open the run.sh script
3. Lines 69, 70 and 71 contain 3 versions of the network setup
   - line 69 is the 4G setup
   - line 70 is the 5G setup
   - line 71 is the base setup
4. Select the setup
5. Save the changes
6. run ./run.sh

   

## Available configurations

NOTE: using the configuration files, a user can create a ring topology that uses wifi as a connection type, for example. Currently with th eexception of 5G and 4G a user can mix and match any connection types with any topologies.

Existing error: When using a mesh topology with wifi, we do run into the ressource issue with the docker container. If a user want to run such a topology please limit the number of connections per nodes at 2 to 3 connections per nodes. *currently under investigation*

### Topologies
1. Ring
2. Mesh
3. Star

### Connection types:
1. point to point connections (p2p)
2. CSMA
3. Wifi
5. 4G
6. 5G

### IEEE models (working)
1. 123-node bus model
2. 9500-node bus model (Current it is called using ieee8500.glm, but it is the ieee9500 model)

NOTE: to automatically generate the input json files use the get\_config.py from the graph folder. If you want to change the glm file that is used either replace the content of ieee8500.glm with the content of your glm file or replace in the python from the name of the glm file that is passed in by the name of the glm file that you want to use

#### How to generate config json file
1. go to graph folder
2. run ``` python get_config.py <Number of Microgrids> ```
3. the number of Microgrids is the number of Microgrids you want your model to have. Currently only tested even number of Microgrids. 

## Configuration files

1. Location: integration/control/config
2. Available config files:
   - **topology.json**: Used to control the topology of the ns3 network
   - **grid.json**: Used to relate nodes of glm files to ns3 nodes. This file is also used to control the attack parameters.
   - **ns_config.json**: Used as configuration file for Helics

### topology.json
1. Channel parameter: settings for the communication parameter for point to point, csma and wifi networks
2. Gridlayout parameter: settings for 2D layout for wifi networks. This will be enabled in the future for other types of networks. 
3. 5GSetup parameter: settings used in both 5G and 4G networks.
4. Node parameter: settings for each node of the network. The configuration file contains default values for each of the nodes. 

### grid.json
1. Microgrids: connects the gridlabd components from glm with the NS3 nodes. Ex: mg1 is 1 NS3 node.
2. MIM: parameter settings for the man-in-the-middle attacks (injection and parameter changes) 
3. DDoS: parameters for the DDoS attacker (ex: number of bots)
4. Simulation: general parameters for the simulation (ex: the start and end time of the simulation)

### gridlabd\_config.json
1. Helics parameters: Helics broker setup parameters (ex: IP address and port number for helics setup)
2. Endpoint: Gridlabd endpoint

NOTE: to change the helics broker's port, the run.sh, the gridlabd\_config.json and the ns3-*.cc need to be updated. All 3 of the files have a reference to the port used by the helics broker. 

## Simulated attacks

### Currently working
1. Injection attacks (Man-in-the-middle): The attacker trips relays connecting the microgrids to one another and to the grid causing islanding of the microgrid 
2. Parameter changes (Man-in-the-middle): The attacker modifies the Pref and Qref values of two inverters in Microgrid 1 while the microgrids are islanded
3. DDoS: The attacker generates a number of bots that will flood 1 to n nodes in the network to slow down the network performance

### Comming soon
1. Insider attack
2. DDoS attack on base network
3. 5G attack vectors, example attack at the SDN level, attacks on slice configurations, etc.


### Reference
```
@article{bel2023co,
  title={Co-Simulation Framework For Network Attack Generation and Monitoring},
  author={Bel, Oceane and Kim, Joonseok and Hofer, William J and Maharjan, Manisha and Purohit, Sumit and Niddodi, Shwetha},
  journal={arXiv preprint arXiv:2307.09633},
  year={2023}
}
```
