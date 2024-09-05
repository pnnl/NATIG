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
