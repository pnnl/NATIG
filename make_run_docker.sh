docker build --network=host -f Dockerfile -t pnnl/natig:natigbase .
docker run -it --rm --name=cps_clean pnnl/natig:natigbase bash
