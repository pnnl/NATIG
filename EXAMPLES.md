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

__3G ring__: This is directly connected network that has middle nodes that are connected in a ring. Then the microgrid ns3 nodes and the control center ns3 node are connected to the individual middle node. 

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

## Tag descriptions

1. 5G: this tag is used to describe the 5G example that we have developed using the LENA NR core (https://cttc-lena.gitlab.io/nr/html/)
2. 4G: this tag is used to describe the 4G LTE example that we have develloped using the existing 4G module in NS3
3. 3G: this tag is used to describe non-cellular network examples, like directly connected network using csma links.

## IEEE bus models

1. IEEE 9500 bus model: The IEEE 9500 bus model is a comprehensive test system used for research and development in the field of electric distribution systems. Comprising 9500 nodes, this model emulates a real-world distribution network with detailed configurations, including various load types, distributed generation, and complex switching operations. It offers a benchmark for evaluating the performance of new technologies, algorithms, and methodologies in areas such as power flow analysis, fault management, and smart grid innovations. The IEEE 9500 bus model aims to enhance the reliability, efficiency, and resilience of modern electric distribution networks through robust simulations and analyses. (https://github.com/GRIDAPPSD/CIMHub/tree/master/ieee9500)

2. IEEE 123 bus model: The IEEE 123 bus model is a standard test system extensively used for research and development in the electric power distribution sector. It represents a moderately-sized distribution network with 123 buses, featuring diverse load profiles, various distribution lines, and multiple voltage levels. This model includes detailed characteristics such as distributed generation sources, capacitor banks, voltage regulators, and switches. The IEEE 123 bus model serves as a crucial tool for evaluating and validating new technologies, simulation tools, and optimization algorithms, thereby supporting advancements in grid reliability, efficiency, and resilience. It provides a realistic framework for studying modern distribution network challenges and solutions. (https://github.com/gridlab-d/tools/blob/master/IEEE%20Test%20Models/123node/IEEE-123.glm)
