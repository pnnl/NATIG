## How to add a node to the configuration and interact with it.

Step 1: If a user wants to add a node to interact with in the examples they need to access three types of files:

1. grid.json: This file is used to asssociate the specific node ID to a Microgrid
    1. The Microgrid section as shown in the example has the mapping between node IDs and the Microgrid
    2. Ex: in the example below we can see that microgrid\_switch4 is located in mg1 (Microgrid 1) and in mg2 (Microgrid 2). On the other hand we can see that load\_35 is only located in mg1.  

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
