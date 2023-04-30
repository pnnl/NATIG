# -*- coding: utf-8 -*-
"""
Created on Fri Apr 12 08:46:40 2019

@author: bhat538
"""

# -*- coding: utf-8 -*-
import helics as h
import logging
import json
import numpy
import random

helicsversion = h.helicsGetVersion()

logger = logging.getLogger(__name__)
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.DEBUG)
logger.info("Change Switch Status Federate: HELICS version = {}".format(helicsversion))

def create_federate(deltat=0.1, fedinitstring="--federates=1"):
  fedinfo = h.helicsCreateFederateInfo()
  # h.helicsFederateInfoSetCoreName(fedinfo, "PythonSwitchCommandFederate")
  h.helicsFederateInfoSetCoreName(fedinfo, "controlRCLFed")
  h.helicsFederateInfoSetCoreTypeFromString(fedinfo, "zmq")
  h.helicsFederateInfoSetCoreInitString(fedinfo, fedinitstring)
  h.helicsFederateInfoSetTimeProperty(fedinfo, h.helics_property_time_delta, deltat)
  # fed = h.helicsCreateCombinationFederate("PythonSwitchCommandFederate", fedinfo)
  fed = h.helicsCreateMessageFederate("controlRCLFed", fedinfo)
  return fed

def destroy_federate(fed, broker=None):
  status = h.helicsFederateFinalize(fed)
  state = h.helicsFederateGetState(fed)
  # assert state == 3
  # while (h.helicsBrokerIsConnected(broker)):
  #     time.sleep(1)
  h.helicsFederateFree(fed)
  h.helicsCloseLibrary()

if __name__ == "__main__":
  #  Registering  federate info from json      
  fed = h.helicsCreateCombinationFederateFromConfig('cc_config.json')
  federate_name = h.helicsFederateGetName(fed)
  logger.info('Federate name: {}'.format(federate_name))
  endpoint_count = h.helicsFederateGetEndpointCount(fed)
  logger.info('Number of endpoints: {}'.format(endpoint_count))
  #   Reference to Endpoints from index. 
  #   Categorize them as sender and receiver Endpoints    
  endid={}
  endid_sender = {}
  endid_receiver = {}
  end_sender_count = 0
  end_receiver_count = 0
  for i in range(0,endpoint_count):
      endid["m{}".format(i)] = h.helicsFederateGetEndpointByIndex(fed, i)
      if h.helicsEndpointGetDefaultDestination(endid["m{}".format(i)]):
          endid_sender["s{}".format(end_sender_count)] = endid["m{}".format(i)]
          logger.info( 'Registered Sender Endpoint ---> {}'.format(h.helicsEndpointGetName(endid_sender["s{}".format(end_sender_count)])))
          end_sender_count = end_sender_count+1
      else:
          endid_receiver["r{}".format(end_receiver_count)] = endid["m{}".format(i)]
          logger.info( 'Registered Receiver Endpoint ---> {}'.format(h.helicsEndpointGetName(endid_receiver["r{}".format(end_receiver_count)])))
          end_receiver_count = end_receiver_count+1
  logger.info('########################   Entering Execution Mode  ##########################################')
  
  Counter = 0
  TmpVar = 0 
  TmpVar2 = 0
  
  h.helicsFederateEnterExecutingMode(fed)
  # total simulation time for fed, and time step
  total_interval = int(8)
  simTime = 8
  grantedtime = -1
  currTime = h.helicsFederateGetCurrentTime(fed)
  grantedtime = h.helicsFederateRequestNextStep(fed)
  update_interval = 1 # 0.001000000000 

  logger.info('Number of received points: {}'.format(end_receiver_count))
  logger.info('Number of sending points: {}'.format(end_sender_count))
  while currTime < simTime:
    currTime = h.helicsFederateGetCurrentTime(fed)
    logger.info('===========================================')
    logger.info('Current time: {0}'.format(currTime))
    grantedtime = h.helicsFederateRequestNextStep(fed)
    logger.info('Granted time: {0}'.format(grantedtime))
    logger.info('===========================================')
    #   Reading from receiver endpoints
    for i in range (0,end_receiver_count):
      ep = endid_receiver["r{}".format(i)]
      logger.info('ep -- {0}'.format(ep))
      end_name = h.helicsEndpointGetName(ep)
      logger.info('end_name -- {0}'.format(end_name))
      while h.helicsEndpointHasMessage(ep):
        value = h.helicsEndpointGetMessage(ep) # value has these properties: time, data, length, messagID, flags, original_source, source, dest, original_dest	
        Data = value.data
        logger.info('Data -- {0}'.format(Data))
        sender_data = Data.replace('W', 'VA')
        logger.info('sender_data -- {0}'.format(sender_data))
        sender_name = value.original_source
        logger.info('sender_name -- {0}'.format(sender_name))
        logger.info('At time {} sec, Control Center received message {} with time signature {} from {} through {} at endpoint {}\r\n'.format(currTime, value.data, value.time, value.original_source, value.source, end_name))              
    
    if currTime > 1 and Counter == 0:
        for i in range (0, end_sender_count):
            ep = endid_sender["s{}".format(i)]
            end_name = h.helicsEndpointGetName(ep)
            logger.info(end_name)
            if ('status' in end_name):
                txt = "{\"microgrid_switch2\":{\"status\":\"OPEN\"}}"
                h.helicsEndpointSendBytesTo(ep, txt, "")
                logger.info("sent")
        Counter += 1

  # Destroying federate      
  logger.info("Destroying federate")
  destroy_federate(fed)
