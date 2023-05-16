HAGEN repo to build a docker image required for CPS modeling

## Available configurations

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
  a. **topology.json**: Used to control the topology of the ns3 network
  b. **grid.json**: Used to relate nodes of glm files to ns3 nodes. This file is also used to control the attack parameters.
  c. **ns_config.json**: Used as configuration file for Helics
