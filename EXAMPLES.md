## Out of the box examples

The following table describes the available example in NATIG and the models that have been used as part of the grid simulation

| Example | Description | Development Stage | 
|---|---|---|
| 3G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using directly connected network | Works
| 3G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using directly connected network | Works
| 4G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using 4G network | Works
| 4G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using 4G network | Works
| 5G 123 IEEE bus | connects the microgrids of the IEEE 123 bus model using 5G network | Works
| 5G 9500 IEEE bus | connects the microgrids of the IEEE 9500 bus model using 5G network | Works

## Available configurations and how to change them

__3G star__: this is a directly connected network that has the control center at the center of the topology and the rest of the nodes are connected in a star pattern

1. To enable this topology, open the grid.json either in /rd2c/PUSH/NATIG/RC/code/<exampleTag>-conf-<modelID> when using the conf input or in the /rd2c/integration/control/config folder when using the noconf input (see section __Commands to run the examples__ for more details about conf vs noconf).
2. In section "Simulation" change UseDynTop to 0. When running the 3G example it will default the setup to a star topology

```
     "Simulation": [
                {
                        "SimTime": 60,
                        "StartTime": 0.0,
                        "PollReqFreq": 15,
                        "includeMIM": 0,
                        "UseDynTop": 0,
                        "MonitorPerf": 0,
                        "StaticSeed": 0,
                        "RandomSeed": 777
                }
        ],
```

__3G ring__: This is directly connected network that has middle nodes connected using a ring pattern. Then the microgrid ns3 nodes and the control center ns3 node are connected to the individual middle node. 

1. To enable this topology, open the grid.json either in /rd2c/PUSH/NATIG/RC/code/<exampleTag>-conf-<modelID> when using the conf input or in the /rd2c/integration/control/config folder when using the noconf input (see section __Commands to run the examples__ for more details about conf vs noconf).
2. In section "Simulation" change UseDynTop to 1 as seen in the following output:
```
     "Simulation": [
                {
                        "SimTime": 60,
                        "StartTime": 0.0,
                        "PollReqFreq": 15,
                        "includeMIM": 0,
                        "UseDynTop": 1,
                        "MonitorPerf": 0,
                        "StaticSeed": 0,
                        "RandomSeed": 777
                }
        ],
```
3. By setting UseDynTop to 1 it will enable the use of the __Node__ in the topology.json file located in the same folder as the grid.json file. Here is an example setup for oa ring topology using the IEEE 123 bus model:
```
"Node": [
          {
                    "name":0,
                    "connections": [
                            1
                    ],
                    "UseCSMA": 1,
                    "MTU": 1500,
                    "UseWifi": 0,
                    "x": 2,
                    "y": 50,
                    "error": "0.001"
            
      },
      {
                    "name":1,
                    "connections":[
                            2	
                    ],
                    "UseCSMA":1,
                    "UseWifi":0,
                    "x":100,
                    "y":200,
                    "error":"0.001"
	  },
	  {
                    "name":2,
                    "connections":[
                            3
                    ],
                    "UseCSMA":1,
                    "UseWifi":0,
                    "x":200,
                    "y":50,
                    "error":"0.001"
      },
      {
                    "name":3,
                    "connections":[
                            4
                    ],
                    "UseCSMA":1,
                    "MTU":1500,
                    "UseWifi":0,
                    "x":300,
                    "y":400,
                    "error":"0.001"
      },
      {
                    "name":4,
                    "connections":[
                            0
                    ],
                    "UseCSMA":1,
                    "MTU":1600,
                    "UseWifi":0,
                    "x":50,
                    "y":350,
                    "error":"0.001"
      }
]

```
4. the connection section controls which nodes the node defined in name is connected to. Both the name input and the connections input are indexes of the middle nodes that are connected together following a topology.  
5. Other inputs in the __Node__ section:
    1. UseCSMA: Can be set to either 1 or 0 (true/false). This input controls whether or not CSMA is used to connect the node defined in the name sections to the nodes in the connection list. CSMA (Carrier Sense Multiple Access) connections use a network protocol that listens for carrier signals before transmitting data to avoid collisions. Commonly utilized in Ethernet and Wi-Fi networks, it enhances communication efficiency by ensuring that only one device transmits at a time, reducing the likelihood of data packet collisions.
    2. UseWifi: Can be set to either 1 or 0 (true/false). This input controls whether or not Wifi is used to connect the node defined in the name sections to the nodes in the connection list. 
        1. If both 1 and 2 are set to 0, the connections default to point to point (P2P) connection types. Point-to-point connections refer to a direct data link between two network nodes, bypassing intermediaries. Common in telecommunications and computer networks, they ensure dedicated and high-speed communication channels. These connections are often used for secure data transfer, high-performance computing, and reliable inter-device communication in various applications.
        2. If either 1 or 2 are not defined in the __Node__ section then they are default to not used.
    3. the x and y values are coordinates to the middle nodes over a 2D grid
    4. The error input is used to add some noise to the network.

__3G mesh__: this is a directly connected network that has middles connected in a mesh pattern. Then the microgrid ns3 nodes and the control center ns3 node are connected to the individual middle node. 

1. To enable this topology, open the grid.json either in /rd2c/PUSH/NATIG/RC/code/<exampleTag>-conf-<modelID> when using the conf input or in the /rd2c/integration/control/config folder when using the noconf input (see section __Commands to run the examples__ for more details about conf vs noconf).
2. In section "Simulation" change UseDynTop to 1 as seen in the following output:
```
     "Simulation": [
                {
                        "SimTime": 60,
                        "StartTime": 0.0,
                        "PollReqFreq": 15,
                        "includeMIM": 0,
                        "UseDynTop": 1,
                        "MonitorPerf": 0,
                        "StaticSeed": 0,
                        "RandomSeed": 777
                }
        ],
```
3. Example __Node__ section when simulating an all to all mesh using the IEEE 123 bus model:

```
"Node": [
          {
		  "name":0,
		  "connections": [
			1,
			2,
			3,
			4
		  ],
		  "UseCSMA": 1,
                  "MTU": 1500,
                  "UseWifi": 0,
                  "x": 2,
                  "y": 50,
                  "error": "0.001"
            
	  },
	  {
                  "name":1,
		  "connections":[
			0,
		        2,
			3,
			4
			
		  ],
		  "UseCSMA":1,
		  "UseWifi":0,
		  "x":100,
		  "y":200,
		  "error":"0.001"
	  },
	  {
                  "name":2,
		  "connections":[
			0,
			1,
			3,
			4
		  ],
		  "UseCSMA":1,
		  "UseWifi":0,
		  "x":200,
		  "y":50,
		  "error":"0.001"
	  },
	  {
                  "name":3,
		  "connections":[
			0,
			1,
			2,
			4
		  ],
		  "UseCSMA":1,
		  "MTU":1500,
		  "UseWifi":0,
		  "x":300,
		  "y":400,
		  "error":"0.001"
	  },
	  {
                  "name":4,
	          "connections":[
			0,
			1,
			2,
			3
		  ],
	          "UseCSMA":1,
	          "MTU":1600,
	          "UseWifi":0,
		  "x":50,
		  "y":350,
		  "error":"0.001"
	  }
    ]
```

__4G and 5G topology__: currently the topology type of these two communication networks are statically set in the code. The Microgrid NS3 nodes are connected to the middle nodes using an all to all mesh. Then each middle nodes are connected to an individual User Equipment node using a direct connection. Each User Equipment can communicate to any Relay Antenna (GnB/EnB) nodes. Once the signal is past the cellular network part of the network, the signal is sent through a single connection link that connects the control center to the rest of the network. 

1. To configure some of the elements of the 4G or 5G examples, open the topology.json either in /rd2c/PUSH/NATIG/RC/code/<exampleTag>-conf-<modelID> when using the conf input or in the /rd2c/integration/control/config folder when using the noconf input (see section __Commands to run the examples__ for more details about conf vs noconf).
2. The channel section. The values in that section are used accross all three available example (3G|4G|5G)
    1. __P2PDelay__: No longer used
    2. __CSMADelay__: Used to add some delay to the CSMA connections
    3. __dataRate__: No longer used
    5. __jitterMin__ and __jitterMax__: Controls the amount of jitter added to the network. Used as part of the Dnp3 Application at the bottom of the .cc files that control the examples. 
    6. All three options in that section that start with Wifi are used by the 3G examples where Wifi can be configured as a connection type:
       1. __WifiPropagationDelay__: Used to set the delay for the wifi links. 
       2. __WifiRate__: Used to set the rate of the Wifi connection
       3. __WifiStandard__: used to set the wifi standard for the wifi link
    7. __P2PRate__: Used to control the rate of the point to point (P2P) rates.
    8. __MTU__: Maximum Transmissable Units. This is used to define the maximum size of the packets that can be transmitted through non-cellular links. 
    9. __delay__: used to define any added delay that should be added to the connections of the network. 

```
"Channel": [
         {
            "P2PDelay": "2ms",
            "CSMAdelay": "3260",
            "dataRate": "5Mbps",
            "jitterMin": 10,
            "jitterMax": 100,
            "WifiPropagationDelay": "ConstantSpeedPropagationDelayModel",
            "WifiRate": "DsssRate1Mbps",
            "WifiStandard": "80211b",
            "P2PRate": "60Mb/s",
	    "MTU": 1500,
	    "delay": 0
        }
      ],
```
3. The Gridlayout section. These values are used accross the 3G, 4G and 5G examples. They are mainly used to setup the 2D grid where the nodes are going to be placed.
    1. __MinX__ and __MinY__ are used to set the minimum x and y coordinates for the nodes over the 2D.
    2. __DeltaX__ and __DeltaY__ are the x and y spaces between buildings. For 5G that space needs to be closest to reduce inteference.
    3. __GridWidth__ is the number of objects layed out on a line
    4. __distance__ called in the code but no longer being used
    5. __GnBH__ and __UEH__ are the height of the nodes in the cellular network like the GnB/EnB and UE nodes
    6. __LayoutType__ determines whether positions are allocated row first or column first.
    7. __SetPos__ determines whether or not the specific coordinates defined in the __Node__ section of the same file is used. (Only available in 3G)

```
 "Gridlayout": [
         {
            "MinX": 0,
            "MinY": 0,
            "DeltaX": 20,
            "DeltaY": 20,
            "GridWidth": 10,
	    "distance": 15,
	    "GnBH": 10.0,
	    "UEH": 1.5,
            "LayoutType": "RowFirst",
            "SetPos": 1
        }
    ],
```

4. The 5GSetup section. These values are used accross the 3G, 4G and 5G examples. This is the mainly used for the configuration of the cellular antennas in 4G and 5G.
    1. __S1uLinkDelay__ is the delay to be used for the next S1-U link to be created
    2. __N1Delay__ is the minimum processing delay (UE side) from the end of DL Data reception to the earliest possible start of the corresponding ACK/NACK transmission
    3. __N2Delay__ is the minimum processing delay needed to decode UL DCI and prepare UL data
    4. __SRS__  is a reference signal sent in the UL (from UE to GnB/EnB)
    5. __UeRow__ and __UECol__ control the size of the antenna on the UE side of the network
    6. __GnBRow__ and __GnBCol__ control the size of the antenna on the GnB/EnB side of the network
        1. Keep in mind that the larger the UE and GnB/EnB antenna size get the slower the simulation will get
    7. __numUE__ and __numEnb__ control the number of UE and the number of GnB/EnB nodes in the network. Currently we only have tested with these values being equal to the number of Microgrids in the network. 


```
"5GSetup": [
            {
                "S1uLinkDelay": 0,
                "N1Delay": 0.01,
                "N2Delay": 0.01,
                "Srs": 10,
                "UeRow": 4,
                "UeCol": 8,
                "GnBRow": 8,
                "GnBCol": 4,
                "numUE": 4,
                "numEnb": 4,
		"CentFreq1": 28e9,
		"CentFreq2": 28.2e9,
		"Band1": 150e6,
		"Band2": 150e6,
	        "num1": 2,
        	"num2": 0,
                "scenario": "UMi-StreetCayon",
                "txPower": 40
          }
    ],
```

## Commands to run the examples

The following steps are assuming that the examples are run on docker:

1. ` cd /rd2c/integration/control `: this command will bring you where the main code and configurations for the run will be located
2. Running the examples out of the box. These commands are useful when you do not want to make any changes to the configurations or if you just pulled an updated version of the setup and want to make sure that the configurations files bring in any new input. The ` conf ` input will trigger the code to overwrite the configuration files in /rd2c/integration/control/config/ and the glm files in /rd2c/integration/control with the files found in /rd2c/PUSH/NATIG/RC/code/<exampleTag>-conf-<modelID> folders. For example, if running the 3G example with the IEEE 123 bus model, the configuration files and the glm files will be taken from /rd2c/PUSH/NATIG/RC/code/3G-conf-123/
    1. 3G with 123: ` sudo bash run.sh /rd2c/  3G "" 123 conf `
    2. 3G with 9500: ` sudo bash run.sh /rd2c/  3G "" 9500 conf `
    3. 4G with 123: ` sudo bash run.sh /rd2c/  4G "" 123 conf `
    4. 4G with 9500: ` sudo bash run.sh /rd2c/  4G "" 9500 conf `
    5. 5G with 123: ` sudo bash run.sh /rd2c/  5G "" 123 conf `
    6. 5G with 9500: ` sudo bash run.sh /rd2c/  5G "" 9500 conf `
3. Running the examples with local changes to the config json files and glm files. Using the noconf input it will trigger the code to read the configuration files direct from /rd2c/integration/control/config and the glm files directly from /rd2c/integration/control/ without overwritting them with the files found in /rd2c/PUSH/NATIG/RC/code/<exampleTag>-conf-<modelID>. We recommand at least to do one run of the example with a conf input and then doing noconf changes once you have an updated version of the config file. 
    1. 3G with 123: ` sudo bash run.sh /rd2c/  3G "" 123 noconf `
    2. 3G with 9500: ` sudo bash run.sh /rd2c/  3G "" 9500 noconf `
    3. 4G with 123: ` sudo bash run.sh /rd2c/  4G "" 123 noconf `
    4. 4G with 9500: ` sudo bash run.sh /rd2c/  4G "" 9500 noconf `
    5. 5G with 123: ` sudo bash run.sh /rd2c/  5G "" 123 noconf `
    6. 5G with 9500: ` sudo bash run.sh /rd2c/  5G "" 9500 noconf `

## Tag descriptions and definitions

1. 5G: this tag is used to describe the 5G example that we have developed using the LENA NR core (https://cttc-lena.gitlab.io/nr/html/)
2. 4G: this tag is used to describe the 4G LTE example that we have develloped using the existing 4G module in NS3
3. 3G: this tag is used to describe non-cellular network examples, like directly connected network using csma links.
4. Middle Nodes: These are nodes located in the middle of the networks. Some of examples of these are node located between the Microgrid NS3 nodes and the Control Center for the 3G example, or the nodes located between the Microgrid NS3 nodes and the cellular network in the case of the 4G and 5G examples. This is also the nodes used by NATIG to simulate the Man-In-The-Middle attacks on the network. 
5. User Equipments (UE): These are the equivalent of cellular router. They are used to connect the nodes between the Microgrid and the User Equipments to the Control Center connected on the other side of the cellular network. 

## IEEE bus models

1. IEEE 9500 bus model: The IEEE 9500 bus model is a comprehensive test system used for research and development in the field of electric distribution systems. Comprising 9500 nodes, this model emulates a real-world distribution network with detailed configurations, including various load types, distributed generation, and complex switching operations. It offers a benchmark for evaluating the performance of new technologies, algorithms, and methodologies in areas such as power flow analysis, fault management, and smart grid innovations. The IEEE 9500 bus model aims to enhance the reliability, efficiency, and resilience of modern electric distribution networks through robust simulations and analyses. (https://github.com/GRIDAPPSD/CIMHub/tree/master/ieee9500)

2. IEEE 123 bus model: The IEEE 123 bus model is a standard test system extensively used for research and development in the electric power distribution sector. It represents a moderately-sized distribution network with 123 buses, featuring diverse load profiles, various distribution lines, and multiple voltage levels. This model includes detailed characteristics such as distributed generation sources, capacitor banks, voltage regulators, and switches. The IEEE 123 bus model serves as a crucial tool for evaluating and validating new technologies, simulation tools, and optimization algorithms, thereby supporting advancements in grid reliability, efficiency, and resilience. It provides a realistic framework for studying modern distribution network challenges and solutions. (https://github.com/gridlab-d/tools/blob/master/IEEE%20Test%20Models/123node/IEEE-123.glm)
