docker build --network=host -f Dockerfile -t rd2c:dev_clean .
docker run -it --rm --name=cps_clean rd2c:dev_clean bash
