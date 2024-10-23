# Example how to use a python federate within NATIG

This folder contains all the code needed to run a simple workflow using the IEEE 123 bus model with 3 receivers and one sender (the python federate). This example also shows how to modify different gridlabd point values.

## Setup the example

Run the following command:
```
bash setup-python-federate.sh <Base folder where the integration/folder is located (example: on docker, this would be "/rd2c/")>
```

Example setup command on Docker:

```
bash setup-python-federate.sh /rd2c/
```

## Command to run the example:

1. go to integration/control folder
2. run ` sudo bash tutorial.sh /rd2c/ 3G "" 123 noconf `

