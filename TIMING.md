## Performance of out of the box examples

For the following 3 tables the poll request is sent every 15ms. The experiments were ran in docker on a computer running a 2.6 GHz 6-Core Intel Core i7 processor with 16 GB 2667 MHz DDR4 memory.

The following time data was collected for a simulation of 60 simulated seconds and with the debug print statements active
| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) |
|---|---|---|---|---|---|
| 4G LTE | 9500 | 45 | 121 | No attacks | 34369.21 |
| 4G LTE | 9500 | 45 | 121 | DDoS with 2 attackers | 41737.71 |
| 4G LTE | 9500 | 45 | 121 | MIM with 2 attacker | 35085.39 |
| 4G LTE | 123 | 17 | 16 | No attack | 4372.48 |
| 4G LTE | 123 | 17 | 16 | DDoS with 1 attackers | 14684.11 |
| 4G LTE | 123 | 17 | 16 | MIM with 3 attackers | 5796.20 |

The following data is the same amount of simulated seconds but with no print lines but data collection is still active
| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) |
|---|---|---|---|---|---|
| 5G | 9500 | 45 | 121 | no attack | 28885.06 |
| 5G | 9500 | 45 | 121 | DDoS with 2 attacker | 85648.90 |
| 5G | 9500 | 45 | 121 | MIM with 2 attackers | 30580.91 |
| 5G | 123 | 17 | 16 | no attack | 5824.01 |
| 5G | 123 | 17 | 16 | DDoS with 1 attacker | 17698.04 |
| 5G | 123 | 17 | 16 | MIM with 3 attackers | 5351.68 |
| 4G LTE | 9500 | 45 | 121 | no attacker | 30048.81 |
| 4G LTE | 9500 | 45 | 121 | DDoS with 2 attackers | 24697.42 |
| 4G LTE | 9500 | 45 | 121 | MIM with 2 attackers | 36660.07 |
| 4G LTE | 123 | 17 | 16 | no attack | 4868.21 |
| 4G LTE | 123 | 17 | 16 | DDoS with 1 attacker | 19900.36 |
| 4G LTE | 123 | 17 | 16 | MIM with 3 attackers | 2794.67 |
| (3G) Mesh | 9500 | 23 | 121 | no attack | 17616.97 |
| (3G) Mesh | 9500 | 23 | 121 | DDoS with 1 attacker | 17339.23 |
| (3G) Mesh | 9500 | 23 | 121 | MIM 2 attackers | 15537.93 |
| (3G) Mesh | 123 | 9 | 16 | no attack | 2106.03 |
| (3G) Mesh | 123 | 9 | 16 | DDoS with 1 attacker | HERE |
| (3G) Mesh | 123 | 9 | 16 | MIM with 3 attackers | |
| (3G) Star | 9500 | 23 | 11 | no attack | |
| (3G) Star | 9500 | 23 | 11 | DDoS with 2 attackers | |
| (3G) Star | 9500 | 23 | 11 | MIM with 2 attackers | |
| (3G) Star | 123 | 9 | 4 | no attack | |
| (3G) Star | 123 | 9 | 4 | DDoS with 1 attacker | |
| (3G) Star | 123 | 9 | 4 | MIM with 3 attackers | |

The following data is the same amount of simulated seconds but with no print statement or data collection
| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) |
|---|---|---|---|---|---|
| 5G | 9500 | 45 | 121 | no attack | 45732.18 |
| 5G | 9500 | 45 | 121 | DDoS with 2 attackers | 74192.72|
| 5G | 9500 | 45 | 121 | MIM with 2 attackers | |
| 5G | 123 | 17 | 16 | no attack | 5795.19 |
| 5G | 123 | 17 | 16 | DDoS with 1 attacker | 10591.63 |
| 5G | 123 | 17 | 16 | MIM with 3 attackers | 7997.78 |
| 4G LTE | 9500 | 45 | 121 | no attack | 26244.69 |
| 4G LTE | 9500 | 45 | 121 | DDoS with 2 attackers | 36607.44 |
| 4G LTE | 9500 | 45 | 121 | MIM with 2 attackers | 24107.69 |
| 4G LTE | 123 | 17 | 16 | no attack | 4098.09 |
| 4G LTE | 123 | 17 | 16 | DDoS with 1 attacker | 8354.13 |
| 4G LTE | 123 | 17 | 16 | MIM with 3 attackers | 2683.45 |
| (3G) Mesh | 9500 | 23 | 121 | no attack | 17391.63 |
| (3G) Mesh | 9500 | 23 | 121 | DDoS with 1 attackers | 19135.84 |
| (3G) Mesh | 9500 | 23 | 121 | MIM with 2 attackers | 15076.19 |
| (3G) Mesh | 123 | 9 | 16 | no attack | 2385.79 |
| (3G) Mesh | 123 | 9 | 16 | DDoS with 1 attacker | |
| (3G) Mesh | 123 | 9 | 16 | MIM with 3 attackers | |
| (3G) Star | 9500 | 23 | 11 | no attack | |
| (3G) Star | 9500 | 23 | 11 | DDoS with 2 attackers | |
| (3G) Star | 9500 | 23 | 11 | MIM with 2 attacker | |
| (3G) Star | 123 | 9 | 4 | no attack | |
| (3G) Star | 123 | 9 | 4 | DDoS with 1 attacker | |
| (3G) Star | 123 | 9 | 4 | MIM with 3 attacker | |

The following 5G run was run with 2X higher bandwidth. We went from sending a packet every 15 ms to every 7ms

| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Data collection? | Time (s) |
|---|---|---|---|---|---|---|
| 5G | 123 | 17 | 16 | no attack | yes | 8460.03 |
| 5G | 123 | 17 | 16 | DDoS with 1 attacker | yes | 19895.78 |
| 5G | 123 | 17 | 16 | MIM with 3 attacker | yes | 5715.21 | 
| 5G | 123 | 17 | 16 | no attack | no |4321.34 |
| 5G | 123 | 17 | 16 | DDoS with 1 attacker | no | 8121.72 |
| 5G | 123 | 17 | 16 | MIM with 3 attackers | no | 7420.40 |

Running the 5G example with the IEEE 9500 bus model and debug statements activated on a shared computing system using slurm
| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) |
|---|---|---|---|---|---|
| 5G | 9500 | 45 | 121 | no attack | 6736.61 |
| 5G | 9500 | 45 | 121 | DDoS with 2 attackers | 8269.92 |
| 5G | 9500 | 45 | 121 | MIM with 2 attackers | 7376.86 |

Running the 5G example with the 9500 bus model and the debug statements turned off and the data collection still running on a shared computing system using slurm

| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) |
|---|---|---|---|---|---|
| 5G | 9500 | 45 | 121 | no attack | 7077.42 |
| 5G | 9500 | 45 | 121 | DDoS with 2 attackers | 8309.60 | 
| 5G | 9500 | 45 | 121 | MIM with 2 attackers | 7381.56 |

Running the 5G example with the 9500 bus model and the debug statements turned off and the data collection in turned off on a shared computing system using slurm

| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) |
|---|---|---|---|---|---|
| 5G | 9500 | 45 | 121 | no attack | 7026.49 |
| 5G | 9500 | 45 | 121 | DDoS with 2 attackers | 8320.26 |
| 5G | 9500 | 45 | 121 | MIM with 2 attackers | 8143.94 | 

Running the 5G example with the 9500 bus model and the debug statements turned off and the data collection in turned off on a shared computing system using slurm with the poll request rate increased from every 15ms to every 7ms

| topology tested | IEEE model | Number of Nodes | Number of Paths | Attack? | Time (s) |
|---|---|---|---|---|---|
| 5G | 9500 | 45 | 121 | no attack | 11679.67 |
| 5G | 9500 | 45 | 121 | DDoS with 2 attackers | |
