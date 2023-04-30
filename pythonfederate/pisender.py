import time
import helics as h
from math import pi
import json

fedinitstring = "--federates=1"
deltat = 0.01
configObj = {"name":"HAGEN Python Federate","endpoints":[{"name":"mg1_trip_shad_inv1$Qref","destination":"GLD/mg1_trip_shad_inv1$Qref"}]}

helicsversion = h.helicsGetVersion()

print("PI SENDER: Helics version = {}".format(helicsversion))

# Create Federate Info object that describes the federate properties #
fedinfo = h.helicsCreateFederateInfo()

# Set Federate name #
#h.helicsFederateInfoSetCoreName(fedinfo, "HAGEN Python Federate")
#h.helicsFederateInfoSetBrokerPort(fedinfo, 23408)

# Set core type from string #
#h.helicsFederateInfoSetCoreTypeFromString(fedinfo, "zmq")

# Federate init string #
#h.helicsFederateInfoSetCoreInitString(fedinfo, fedinitstring)

# Set the message interval (timedelta) for federate. Note th#
# HELICS minimum message time interval is 1 ns and by default
# it uses a time delta of 1 second. What is provided to the
# setTimedelta routine is a multiplier for the default timedelta.

# Set one second message interval #
h.helicsFederateInfoSetTimeProperty(fedinfo, h.helics_property_time_delta, deltat)

# Create value federate #
#"mg1_trip_shad_inv1$Qref"

#vfed = h.helicsCreateValueFederate("TestA Federate", fedinfo)
#fed = h.helicsCreateCombinationFederate("HAGEN Python Federate", fedinfo)
fed = h.helicsCreateCombinationFederateFromConfig(json.dumps(configObj))
print("PI SENDER: Combination federate created")
endpoint = h.helicsFederateGetEndpoint(fed, "mg1_trip_shad_inv1$Qref")




# Register the publication #
#pub = h.helicsFederateRegisterGlobalTypePublication(vfed, "testA", "double", "")
print("PI SENDER: Endpoing registered")
# {"trip_shad_inv1": {"Qref": 0.98765} }
# Enter execution mode #
h.helicsFederateEnterExecutingMode(fed)
print("PI SENDER: Entering execution mode")




currenttime = h.helicsFederateRequestTime(fed, 3)
val = {"trip_shad_inv1": {"Qref":0.98765}} 
msg = h.helicsFederateCreateMessageObject(fed)
h.helicsMessageSetString(msg, json.dumps(val))
print('sending message ')
h.helicsEndpointSendMessage(endpoint, msg)
print('message sent')
    
h.helicsFederateRequestTime(fed,3)
print('checking message')
if h.helicsEndpointHasMessage(endpoint):
    print('getting message')
    msg = h.helicsEndpointGetMessage(endpoint)
    inverter_val = h.helicsMessageGetString(msg)
    print(inverter_val)

currenttime = h.helicsFederateRequestTime(fed, 4)
val = {"trip_shad_inv1": {"Qref":0.0}} 
msg = h.helicsFederateCreateMessageObject(fed)
h.helicsMessageSetString(msg, json.dumps(val))
print('sending message ')
h.helicsEndpointSendMessage(endpoint, msg)
print('message sent')


currenttime = h.helicsFederateRequestTime(fed, 5)
val = {"trip_shad_inv1": {"Qref":0.01234}} 
msg = h.helicsFederateCreateMessageObject(fed)
h.helicsMessageSetString(msg, json.dumps(val))
print('sending message ')
h.helicsEndpointSendMessage(endpoint, msg)
print('message sent')
h.helicsFederateFinalize(fed)
print("PI SENDER: Federate finalized")
h.helicsFederateFree(fed)
