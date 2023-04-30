import helics as h
import json

fedinitstring = "--federates=1"
deltat = 0.01

#configObj = {"name":"HAGEN Recevier Federate","endpoints":[{"name":"mg1_trip_shad_inv1$Qref","destination":"GLD/mg1_trip_shad_inv1$Qref"}]}
configObj = {"name":"HAGEN","endpoints":[{"name":"hagenmg1"}]}

helicsversion = h.helicsGetVersion()

print("PI RECEIVER: Helics version = {}".format(helicsversion))

# Create Federate Info object that describes the federate properties */
print("PI RECEIVER: Creating Federate Info")
fedinfo = h.helicsCreateFederateInfo()

# Set Federate name
print("PI RECEIVER: Setting Federate Info Name")
h.helicsFederateInfoSetCoreName(fedinfo, "HAGEN PYTHON RECEIVER")
#h.helicsFederateInfoSetBrokerPort(fedinfo, 23408)
# Set core type from string
print("PI RECEIVER: Setting Federate Info Core Type")
h.helicsFederateInfoSetCoreTypeFromString(fedinfo, "zmq")

# Federate init string
print("PI RECEIVER: Setting Federate Info Init String")
h.helicsFederateInfoSetCoreInitString(fedinfo, fedinitstring)

# Set the message interval (timedelta) for federate. Note that
# HELICS minimum message time interval is 1 ns and by default
# it uses a time delta of 1 second. What is provided to the
# setTimedelta routine is a multiplier for the default timedelta.

# Set one second message interval
print("PI RECEIVER: Setting Federate Info Time Delta")
h.helicsFederateInfoSetTimeProperty(fedinfo, h.helics_property_time_delta, deltat)

# Create value federate
print("PI RECEIVER: Creating Value Federate")
#vfed = h.helicsCreateValueFederate("HAGEN RECEIVER Federate", fedinfo)
fed = h.helicsCreateCombinationFederateFromConfig(json.dumps(
        configObj))
print("PI RECEIVER: Value federate created")
endpoint = h.helicsFederateGetEndpoint(fed, "hagenmg1")


# Subscribe to PI SENDER's publication
#sub = h.helicsFederateRegisterSubscription(vfed, "GLD/mg1_trip_shad_inv1$Qref", "")
#print("PI RECEIVER: Subscription registered")

h.helicsFederateEnterExecutingMode(fed)
print("PI RECEIVER: Entering execution mode")

#endpoint = h.helicsEndpointGetName("GLD/mg1_trip_shad_inv1$Qref")



value = 0.0
prevtime = 0

currenttime = -1
while currenttime <= 100:
   #print(h.helicsEndpointHasMessage(endpoint))
   #if h.helicsEndpointHasMessage(endpoint):
   currenttime = h.helicsFederateRequestTime(fed, currenttime+2)
   value = h.helicsEndpointGetMessage(endpoint)
   #helics_message = h.helicsEndpointGetMessageObject(endpoint)
   helics_output = h.helicsMessageGetString(value)
   print(f"====current time is {currenttime} ====")
   print(value)
