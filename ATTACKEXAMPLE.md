## Available attacks 

1. Man-In-The-Middle (MIM) Attacks: A Man-In-The-Middle (MITM) attack occurs when a malicious actor intercepts and potentially alters communication between two parties without their knowledge. This breach compromises data integrity and confidentiality, allowing the attacker to steal sensitive information, inject malicious content, or manipulate communication for fraudulent purposes.
2. Denial of Service (DDoS) Attacks: A Denial of Service (DoS) attack targets a network or service to make it unavailable to users by overwhelming it with excessive traffic or exploiting vulnerabilities. This results in service disruption, downtime, and potential financial loss, as legitimate users are unable to access the targeted resource.

## Configuring attacks on out of the box examples

1. DDoS configuration. This is the section that the code uses to setup the attack senario for DDoS attacks. 
    1. __NumberOfBots__ this is the total number of applications that will be evenly distributed across the attacker. Example if the __NumberOfBots__ is set to 4 and there are 2 attackers then there will be 2 applications per attackers. 
    2. __threadsPerAttacker__ this parameter controls the number of attackers in the network. 
    3. __Active__ is the parameter that controls if the attackers are active or not in the network
    4. __Start__ This parameter controls when the attack starts in seconds
    5. __End__ This parameter controls when the attack stops in seconds
    6. __TimeOn__ and __TimeOff__ control how long the attacker floods the network and how long it stops. These parameters can be controlled to conducted burst DDoS attacks. 
    7. __PacketSize__ is the input parameter that controls the size of the packets sent by the attacker to flood the network. 
    8. __Rate__ controls the rates at which the packets are sent by the attacker during the attack
    9. __usePing__ **Currently under development** Not currently used but will be used to run a ping attack as a DDoS attack
    10. __NodeType__ is the starting node where the attacker would be connected to and would flood the packets through
    11. __NodeID__ is the starting index of the attack. If the value is set to 2, this means that starting node 2 next next number of __threadsPerAttacker__ nodes will be under attack
    12. __endPoint__ This is the type of node that will be used as a target for the attack. This means that the attack will impact the paths between the __Nodetype__ and the __endPoint__

```
"DDoS": [
        {
            "NumberOfBots": 4,
	    "threadsPerAttacker": 1,
            "Active": 0,
            "Start": 10,
            "End": 35,
            "TimeOn": 25.0,
            "TimeOff": 0.0,
            "PacketSize": 1500,
            "Rate": "40480kb/s",
	    "usePing": 1,
            "NodeType": [
                      "UE"
            ],
            "NodeID": [
                        2
            ],
            "endPoint": "CC"
         }
    ],
```

2. MIM configuration. The following section controls the number of MIM attackers on the network.
    1. In the first section, the __Numberattackers__ and the __listMIM__ controls the number of attackers and lists their indexes. 
    2. The following sections, there are one section per MIM attacker. Currently there is the same number of attacker nodes as the number of Microgrids but not all of them need to be active to work correctly
    3. Description of the MIM configuration parameters:
        1. __name__ is the name parameter that is given to each of the attacker nodes
        2. __attack_val__ is the value that the point value will be set too
        3. __Start__ and __End__ values control when the attack starts and ends in seconds. 
        4. __real_val__ is the value that the point is set off the attack. When it is set to __NA__ the value of the point will not be set off the attack. When it is set an actual value, the point value will set back to that value after the attack is over.
        5. __node_id__ is the id of the nodes in gridlabd that is under attack. In the example bellow, the __MIM1__ is attacking the inverter that is labeled __trip_shad_inv1__. 
        6. __point_id__ is the aspect of the node that is under attack. In the case of the __MIM1__ attacker in the example below, the attacker is attacking the __Qref__ value of the __trip_shad_inv1__ inverter. 
        7. __scenario_id__ and __attack_type__ are the input that controls the type of attacks that can be run by the MIM attacker. 
            1. Scenario 4.a: Firstly, the microgrids get islanded. Then, a Man-In-The-Middle (MITM) attack changes the setpoint of node to introduce issues. The attack happened at approx. __Start__ seconds into data collection. __node_id__ has its __point_id__ setpoint changed from __real_val__ (default) to __attack_val__ (attack value).
            2. Scenario 4.b: Firstly, the microgrids get islanded. Then, selected nodes are attacked consistently to cause stability issues. __node_id__ has its __point_id__ value randomly toggled between __real_val__ (default) and __attack_val__ (attack value). The attack starts at approx. 2 minutes into data capture.
```
"MIM": [
	{
            "NumberAttackers": 3,
	    "listMIM": "0,1,2"
	},
        {
            "name": "MIM1",
	    "attack_val": "-50000",
	    "real_val": "0",
	    "node_id": "trip_shad_inv1",
	    "point_id": "Qref",
	    "scenario_id": "a",
	    "attack_type": 4,
	    "Start": 30,
	    "End": 60,
	    "PointStart": "30",
	    "PointStop": "60"
        },
	{
            "name": "MIM2",
	    "attack_val": "TRIP",
	    "real_val": "CLOSE",
	    "node_id": "microgrid_switch4",
	    "point_id": "status",
	    "scenario_id": "b",
	    "attack_type": 3,
	    "Start": 30,
	    "End": 60,
	    "PointStart": "30",
	    "PointStop": "60"
	},
	{
            "name": "MIM3",
	    "attack_val": "TRIP,TRIP",
	    "real_val": "CLOSE,CLOSE",
	    "node_id": "microgrid_switch2,microgrid_switch3",
	    "point_id": "status,status",
	    "scenario_id": "b",
	    "attack_type": 3,
	    "Start": 30,
	    "End": 60,
	    "PointStart": "30,30",
	    "PointStop": "60,60"
	},
	{
            "name": "MIM4",
	    "attack_val": "TRIP",
	    "real_val": "NA",
	    "node_id": "microgrid_switch1",
	    "point_id": "status",
	    "scenario_id": "b",
	    "attack_type": 3,
	    "Start": 30,
	    "End": 60,
	    "PointStart": "30",
	    "PointStop": "60"
	}
    ],
```
