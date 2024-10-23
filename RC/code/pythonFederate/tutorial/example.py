# -*- coding: utf-8 -*-
import helics as h
import logging
import json
import random
import time
import sys
import shutil
import os

helicsversion = h.helicsGetVersion()

logger = logging.getLogger(__name__)
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.DEBUG)
logger.info("Change Switch Status Federate: HELICS version = {}".format(helicsversion))


def destroy_federate(fed, broker=None):
  h.helicsFederateFree(fed)
  h.helicsCloseLibrary()
  return None


if __name__ == "__main__":
  #  Registering  federate info from json
  # Initializing time
  #Additional note: check documentation at https://docs.helics.org/en/helics2/doxygen/MessageFederate_8h.html for additional information regarding the function calls
  start_time = time.time()

  #Federate Registration
  #Initialize the federate using cc_config
  fed = h.helicsCreateCombinationFederateFromConfig('cc_config_test.json') #creates an endpoint usings specs from the config file
  federate_name = h.helicsFederateGetName(fed) #Get the name of the federate
  logger.info('Federate name: {}'.format(federate_name))

  #Get the number of endpoints
  endpoint_count = h.helicsFederateGetEndpointCount(fed) #get the federate name to get thr total number of sender and receiver endpoints
  logger.info('Number of endpoints: {}'.format(endpoint_count))


  #Initialize variables that are partitioned by sender and receive endpoints
  endid = {}
  endid_sender = {}
  endid_receiver = {}
  end_sender_count = 0
  end_receiver_count = 0

  #Loop over all the endpoints to separate out the sender and receiver endpoints
  for i in range(0, endpoint_count):
    endid["m{}".format(i)] = h.helicsFederateGetEndpointByIndex(fed, i) #Get the helics endpoint

    #this if else statement check if the specific endpoint has a "destination" attribute.
    #if so, then -> sender endpoint. Else it is a receiver endpoint
    if h.helicsEndpointGetDefaultDestination(endid["m{}".format(i)]):
      endid_sender["s{}".format(end_sender_count)] = endid["m{}".format(i)]
      logger.info('Registered Sender Endpoint ---> {}'.format(
        h.helicsEndpointGetName(endid_sender["s{}".format(end_sender_count)])))
      end_sender_count = end_sender_count + 1
    else:
      endid_receiver["r{}".format(end_receiver_count)] = endid["m{}".format(i)]
      logger.info('Registered Receiver Endpoint ---> {}'.format(
        h.helicsEndpointGetName(endid_receiver["r{}".format(end_receiver_count)])))
      end_receiver_count = end_receiver_count + 1
  logger.info('########################   Entering Execution Mode  ##########################################')
  Counter = 0
  TmpVar = 0
  TmpVar2 = 0

  # Entering execution phase. At this point the Python federate will wait for other federates have finsihed up registrationa and initialization
  h.helicsFederateEnterExecutingMode(fed)
  # total simulation time for fed, and time step
  total_interval = int(8)

  #Changing simTime from 180 -> 10 seconds
  simTime = 20
  grantedtime = -1
  currTime = h.helicsFederateGetCurrentTime(fed)

  #Note, we also tried the helicsFederateRequestTime() but it did not work.
  #The next step = currTime + 1s as indicated in the cc_config_test.json
  #time resolution = 1 second
  grantedtime = h.helicsFederateRequestNextStep(fed)

  logger.info('Number of received points: {}'.format(end_receiver_count))
  logger.info('Number of sending points: {}'.format(end_sender_count))

  while currTime < simTime:
    currTime = h.helicsFederateGetCurrentTime(fed)
    logger.info('===========================================')
    logger.info('Current time: {0}'.format(currTime))
    grantedtime = h.helicsFederateRequestNextStep(fed)
    logger.info('Granted time: {0}'.format(grantedtime))
    logger.info('===========================================')

    #Resetting all the microgrid arrays
    logger.info("Resetting all microgrid control parameters....")

    #   Reading from receiver endpoints
    for i in range(0, end_receiver_count):
      ep = endid_receiver["r{}".format(i)]
      logger.info('ep -- {0}'.format(ep))
      end_name = h.helicsEndpointGetName(ep)
      logger.info('end_name -- {0}'.format(end_name))
      while h.helicsEndpointHasMessage(ep):
        # Read data from the receiver endpoints
        value = h.helicsEndpointGetMessage(
          ep)  # value has these properties: time, data, length, messagID, flags, original_source, source, dest, original_dest
        Data = value.data
        # logger.info('Data -- {0}'.format(Data))
        sender_data = Data.replace('W', 'VA')
        # logger.info('sender_data -- {0}'.format(sender_data))
        sender_name = value.original_source
        logger.info('sender_name -- {0}'.format(sender_name))
        logger.info(
          'At time {} sec, Control Center received message {} with time signature {} from {} through {} at endpoint {}\r\n'.format(
            currTime, value.data, value.time, value.original_source, value.source, end_name))


    #-------------------End of receiver endpoints--------------------------------------
    #---GO over s

    #Set P_max and P_minn for the inverters. These will be sent to GLD
    P_max = 0.9
    P_min = 0.3

    ###Adding in the block of code for sender counts
    for i in range(0, end_sender_count):
      #getting information about the sender
      ep = endid_sender["s{}".format(i)]
      logger.info('Troubleshooting sender info')
      # logger.info('ep -- {0}'.format(ep))
      end_name = h.helicsEndpointGetName(ep)
      logger.info('end_name -- {0}'.format(end_name))

      if end_name == "CC/mg1_meter_ld_39$constant_power_B":
        # logger.info("Setting controllable loads")
        msg_dict = {"load_39": {"constant_power_B": "28000+12000j"}}
      elif end_name == "CC/mg1_inv6$Pmin":
        # logger.info("Testing out the Inverter")
        msg_dict = {"trip_shad_inv6": {"Pmin": P_min}}  # in pu
      elif end_name == "CC/mg1_inv6$Pmax":
        # logger.info("Testing out the Inverter")
        msg_dict = {"trip_shad_inv6": {"Pmax": P_max}} #in pu

      #Turning off the message passing
      #if currTime >= 0:
      msg = h.helicsFederateCreateMessage(fed) #create the message object
      #sending message
      h.helicsMessageSetString(msg, json.dumps(msg_dict)) #tie the message object with the string
      # logger.info(msg)
      h.helicsEndpointSendMessage(ep, msg)
      #logger.info(f"msg sent for {end_name}")
  # Destroying federate
  logger.info("Destroying federate")
  destroy_federate(fed)

  #calculate the end_time
  end_time = time.time()
  logger.info("Federate Ended")
  logger.info(f"The total execution time is {(end_time - start_time)/60.0} mins")



