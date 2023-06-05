**N**etwork **A**ttack **T**estbed **I**n [Power] **G
r**rid (**NATI[P]G**), a co-simulation environment for distribution power grid network using state-of-the-art simulators.

It is a standalone, containerized, and reusable environment to enable cyber analysts and researchers to run different cyber security and performance scenarios on powergrid.

## How to get started
1. Have docker installed
2. clone the NATIG repo
3. Make sure you have access to the NS3 stash repo at: https://stash.pnnl.gov/projects/HAGEN/repos/ns-3-dev/browse
4. run the following commands:
   - cd NATIG
   - ./buildimage.sh 
   - Once the docker finishes building successfully, run: ./rundocker.sh


NOTE: The default docker container does not come with 5G enabled


To enable 5G capabilities:
1. request access to https://gitlab.com/cttc-lena/nr
2. clone the nr repo in the contrib folder of ns-3-dev folder
3. checkout the branch labeled 5g-lena-v1.2.y
5. Some updates that need to be done to the code before it can be compiled:
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
   - the nr-net-device.cc in the model folder needs the following function:
     ```
      Ptr<Queue<Packet>>
      NrNetDevice::GetQueue (void) const
      {
	  NS_LOG_FUNCTION_NOARGS ();
	 return 0;
      }
     ```
     NOTE: dont forget to add **Ptr<Queue<Packet\>\> GetQueue (void) const** to nr-net-device.h
   - In nr-point-to-point-epc-helper.cc in the helper folder replace the DoAddX2Interface function with the following:
     ```
     void
     NrPointToPointEpcHelper::DoAddX2Interface (const Ptr<EpcX2> &gnb1X2, const Ptr<NetDevice> &gnb1NetDev,
                                           const Ipv4Address &gnb1X2Address,
                                           const Ptr<EpcX2> &gnb2X2, const Ptr<NetDevice> &gnb2NetDev,
                                           const Ipv4Address &gnb2X2Address) const
     {
       NS_LOG_FUNCTION (this);
       Ptr<NrGnbNetDevice> gnb1NetDevice = gnb1NetDev->GetObject<NrGnbNetDevice> ();
       Ptr<NrGnbNetDevice> gnb2NetDevice = gnb2NetDev->GetObject<NrGnbNetDevice> ();
       uint16_t gnb1CellId = gnb1NetDevice->GetCellId ();
       uint16_t gnb2CellId = gnb2NetDevice->GetCellId ();

       NS_ABORT_IF (gnb1NetDevice == nullptr);
       NS_ABORT_IF (gnb2NetDevice == nullptr);

       NS_LOG_LOGIC ("NrGnbNetDevice #1 = " << gnb1NetDev << " - CellId = " << gnb1CellId);
       NS_LOG_LOGIC ("NrGnbNetDevice #2 = " << gnb2NetDev << " - CellId = " << gnb2CellId);

       std::vector<short unsigned int> gnb1 = gnb1NetDevice->GetCellIds();
       std::vector<short unsigned int> gnb2 = gnb2NetDevice->GetCellIds();

       gnb1X2->AddX2Interface (gnb1CellId, gnb1X2Address, gnb2, gnb2X2Address);
       gnb2X2->AddX2Interface (gnb2CellId, gnb2X2Address, gnb1, gnb1X2Address);

       gnb1NetDevice->GetRrc ()->AddX2Neighbour (gnb2.at(0));
       gnb2NetDevice->GetRrc ()->AddX2Neighbour (gnb1.at(0));
     }
     ```
   - 
7. return to the main ns-3-dev folder
8. run ./make.sh

   

## Available configurations

NOTE: using the configuration files, a user can create a ring topology that uses wifi as a connection type, for example. Currently with th eexception of 5G and 4G a user can mix and match any connection types with any topologies.

Existing error: When using a mesh topology with wifi, we do run into the ressource issue with the docker container. If a user want to run such a topology please limit the number of connections per nodes at 2 to 3 connections per nodes. *currently under investigation*

### Topologies
1. Ring
2. Mesh
3. Star
4. 5G
#### Future topologies
1. 4G

### Connection types:
1. point to point connections (p2p)
2. CSMA
3. Wifi

## Configuration files

1. Location: integration/control/config
2. Available config files:
   - **topology.json**: Used to control the topology of the ns3 network
   - **grid.json**: Used to relate nodes of glm files to ns3 nodes. This file is also used to control the attack parameters.
   - **ns_config.json**: Used as configuration file for Helics

## Simulated attacks

### Currently working
1. Injection attacks: The attacker trips relays connecting the microgrids to one another and to the grid causing islanding of the microgrid 
2. Man-in-the-middle: The attacker modifies the Pref and Qref values of two inverters in Microgrid 1 while the microgrids are islanded

### Comming soon
1. Insider attack
2. DDoS attack
3. 5G attack vectors, example attack at the SDN level, attacks on slice configurations, etc.
