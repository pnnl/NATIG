# Workflow

1. Download helics-ns3 from Stash in your Docker container
  - Source: `https://stash.pnnl.gov/scm/hagen/helics-ns3.git`
  - Branch: `dev-dnp3`
```shell
cd /rd2c/ns-3-dev/contrib
git clone https://stash.pnnl.gov/scm/hagen/helics-ns3.git
git checkout dev-dnp3
```
2. Update NS3 in your Docker container
  - Source: `https://stash.pnnl.gov/scm/hagen/ns-3-dev.git`
  - Branch: `dev-helics-dnp3`
```shell
cd /rd2c/ns-3-dev
git pull
git checkout dev-helics-dnp3
```
3. Download an example code for integration of helics-ns3, dnp3, and GridLAB-D in your host machine
  - Source: `https://stash.pnnl.gov/scm/hagen/cps_modeling.git`
  - Branch: `develop`
```shell
# Assume that you clone it already
cd cps_modeling
git pull
git checkout develop
```
  - Copy cps_modeling/integration in the host to /rd2c/integration in the docker container.
  - You need to know your container id.
```shell
docker ps

docker cp integration/. your_container_id:/rd2c/integration
```


# Run simulations

1. Change directory into the example.
  - `cd /rd2c/integration/control`
2. Run `run.sh` script.
  - `bash run.sh`
3. Check out output logs.
  - `ls output`
4. Check out PCAP output files.
  - `ls /rd2c/ns-3-dev/star*.pcap`

If errors occur in running simulations, you need to kill processes.

```shell
pkill -9 helics_broker
pkill -9 gridlabd
pkill -9 python
pkill -9 ns3-helics-grid-dnp3
```

If you don't have ps, `apt-get install procps`.

