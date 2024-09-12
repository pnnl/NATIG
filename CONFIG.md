## How to add a node to the configuration and interact with it.

If a user wants to add a node to interact with in the examples they need to access three types of files:

1. __grid.json__: This file is used to asssociate the specific node ID to a Microgrid
    1. The Microgrid section as shown in the example has the mapping between node IDs and the Microgrid
    2. Ex: in the example below we can see that microgrid\_switch4 is located in mg1 (Microgrid 1) and in mg2 (Microgrid 2). On the other hand we can see that load\_35 is only located in mg1.  
    3. The reason some of the node\_ids are shared between microgrids is because they are nodes that connect microgrids together. In most cases, those nodes are switches and they are used as ways to island the Microgrids from each other and the grids during some attacks. 

```
"microgrid": [
        {
            "name": "mg1",
            "dest": "ns3/mg1",
            "node": [
                "node_36",
                "node_40",
                "node_44",
                "node_135",
                "node_151"
            ],
            "load": [
                "load_35",
                "load_37",
                "load_38",
                "load_39",
                "load_41",
                "load_42",
                "load_43",
                "load_45",
                "load_46",
                "load_47",
                "load_48",
                "load_49",
                "load_50",
                "load_51"
            ],
            "switch": [
                "microgrid_switch4"
            ],
	    "inverter":[
                "trip_shad_inv1",
		"trip_shad_inv4"
	    ]
        },
        {
            "name": "mg2",
            "dest": "ns3/mg2",
            "node": [
                "node_101",
                "node_105",
                "node_108",
                "node_110",
                "node_197",
                "node_300"
            ],
            "load": [
                "load_102",
                "load_103",
                "load_104",
                "load_106",
                "load_107",
                "load_109",
                "load_111",
                "load_112",
                "load_113",
                "load_114"
            ],
            "switch": [
                "switch_300-350",
		"microgrid_switch4"
            ]
        },
```

2. __gridlabd config.json__: This file is used to define the points that can be interacted with during attacks or workflows. 
    1. In the example below, we can see that some of the points ids that could be targeted by an attacker attacking microgrid\_switch4 inlcude status, phase A, B and C, and current in A, B and C.
    2. This file is also used to coordinate information between gridlabd and ns3. 

```
"endpoints": [
        {
            "name": "mg1_node_36$voltage_A",
            "type": "string",
            "global": false,
            "info": "{\"node_36\": \"voltage_A\"}"
        },
        {
            "name": "mg1_node_36$voltage_B",
            "type": "string",
            "global": false,
            "info": "{\"node_36\": \"voltage_B\"}"
        },
        {
            "name": "mg1_node_36$voltage_C",
            "type": "string",
            "global": false,
            "info": "{\"node_36\": \"voltage_C\"}"
        },
        {
            "name": "mg1_node_40$voltage_A",
            "type": "string",
            "global": false,
            "info": "{\"node_40\": \"voltage_A\"}"
        },
...
        {
            "name": "mg1_microgrid_switch4$status",
            "type": "string",
            "global": false,
            "info": "{\"microgrid_switch4\": \"status\"}"
        },
        {
            "name": "mg1_microgrid_switch4$phase_A_state",
            "type": "string",
            "global": false,
            "info": "{\"microgrid_switch4\": \"phase_A_state\"}"
        },
        {
            "name": "mg1_microgrid_switch4$phase_B_state",
            "type": "string",
            "global": false,
            "info": "{\"microgrid_switch4\": \"phase_B_state\"}"
        },
        {
            "name": "mg1_microgrid_switch4$phase_C_state",
            "type": "string",
            "global": false,
            "info": "{\"microgrid_switch4\": \"phase_C_state\"}"
        },
        {
            "name": "mg1_microgrid_switch4$current_in_A",
            "type": "string",
            "global": false,
            "info": "{\"microgrid_switch4\": \"current_in_A\"}"
        },
        {
            "name": "mg1_microgrid_switch4$current_in_B",
            "type": "string",
            "global": false,
            "info": "{\"microgrid_switch4\": \"current_in_B\"}"
        },
        {
            "name": "mg1_microgrid_switch4$current_in_C",
            "type": "string",
            "global": false,
            "info": "{\"microgrid_switch4\": \"current_in_C\"}"
        },
	{
            "name": "mg1_trip_shad_inv1$Qref",
	    "type": "string",
	    "global": false,
	    "info": "{\"trip_shad_inv1\": \"Qref\"}"
	},
	{
            "name": "mg1_trip_shad_inv1$Pref",
	    "type": "string",
	    "global": false,
	    "info": "{\"trip_shad_inv1\": \"Pref\"}"
	},
	{
            "name": "mg1_trip_shad_inv4$Qref",
	    "type": "string",
	    "global": false,
	    "info": "{\"trip_shad_inv4\": \"Qref\"}"
	},
	{
            "name": "mg1_trip_shad_inv4$Pref",
	    "type": "string",
	    "global": false,
	    "info": "{\"trip_shad_inv4\": \"Pref\"}"
	},
        {
            "name": "mg1",
            "type": "string",
            "global": false,
            "destination": "ns3/mg1",
            "info": "{\"node_36\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"node_40\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"node_44\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"node_135\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"node_151\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_35\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_37\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_38\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_39\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_41\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_42\": [\"voltage_A\", \"voltage_B\", \"voltage_C\", \"Qref\"], \"load_43\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_45\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_46\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_47\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_48\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_49\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_50\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"load_51\": [\"voltage_A\", \"voltage_B\", \"voltage_C\"], \"microgrid_switch4\": [\"status\", \"phase_A_state\", \"phase_B_state\", \"phase_C_state\", \"current_in_A\", \"current_in_B\", \"current_in_C\"], \"trip_shad_inv1\":[\"Qref\", \"Pref\"], \"trip_shad_inv4\":[\"Qref\", \"Pref\"]}"
        },
```

3. __The point files__: These files include points_mg1.csv, points_mg2.csv, points_mg3.csv, and points_substation.csv located in the RC/code/points-123/ folder in the NATIG repository for the IEEE 123 bus model.
    1. The example below is shows the content of points_mg1.csv file. All the points files have similar structures. 
    2. ANALOG is used for values that have a numerical value like voltage or current
    3. BINARY is used values that either have a string input like "OPEN" or "CLOSE". Overall they are limited to a finite number of states. 
    4. Once you set whether the target point is a binary or analog point, you need to set the node_id$point_id value. This used by the grid during poll response to identify the value that needs to be sent back to the control center.
    5. Most ANALOG points will have a real and imaginary value. Make sure to add an input for both cases.
    6. Finally the last input is not directly used by the examples in NATIG. If not used, set to 0. 

```
ANALOG,node_36$voltage_A.real,0
ANALOG,node_36$voltage_A.imag,0
ANALOG,node_36$voltage_B.real,0
ANALOG,node_36$voltage_B.imag,0
ANALOG,node_36$voltage_C.real,0
ANALOG,node_36$voltage_C.imag,0
ANALOG,node_40$voltage_A.real,0
ANALOG,node_40$voltage_A.imag,0
ANALOG,node_40$voltage_B.real,0
ANALOG,node_40$voltage_B.imag,0
ANALOG,node_40$voltage_C.real,0
ANALOG,node_40$voltage_C.imag,0
ANALOG,node_44$voltage_A.real,0
ANALOG,node_44$voltage_A.imag,0
ANALOG,node_44$voltage_B.real,0
ANALOG,node_44$voltage_B.imag,0
ANALOG,node_44$voltage_C.real,0
...
ANALOG,load_51$voltage_A.real,0
ANALOG,load_51$voltage_A.imag,0
ANALOG,load_51$voltage_B.real,0
ANALOG,load_51$voltage_B.imag,0
ANALOG,load_51$voltage_C.real,0
ANALOG,load_51$voltage_C.imag,0
ANALOG,microgrid_switch4$current_in_A.real,0
ANALOG,microgrid_switch4$current_in_A.imag,0
ANALOG,microgrid_switch4$current_in_B.real,0
ANALOG,microgrid_switch4$current_in_B.imag,0
ANALOG,microgrid_switch4$current_in_C.real,0
ANALOG,microgrid_switch4$current_in_C.imag,0
ANALOG,trip_shad_inv1$Pref,0
ANALOG,trip_shad_inv1$Qref,0
ANALOG,trip_shad_inv4$Pref,0
BINARY,microgrid_switch4$phase_A_state,0
BINARY,microgrid_switch4$phase_B_state,0
BINARY,microgrid_switch4$phase_C_state,0
BINARY,microgrid_switch4$status,0
``` 
