import numpy as np
import glob
import glm
import json
import os

list_files = glob.glob("../ieee9500*.glm")

l1 = glm.load(list_files[0])

l2 = {}
l2["name"] = "GLD"
l2["loglevel"] = 1
l2["coreType"] = "zmq"
l2["period"] = 1
l2["slow_responding"] = "True"
l2["broker"] = "127.0.0.1:6000"
l2["brokerPort"] = 6000
l2["broker_rank"] = 0
l2["endpoints"] = []

microgrids = {}
confi = {}
for i in l1["objects"]:
    if "configuration" in i["attributes"].keys():
        tt = i["attributes"]["configuration"].split("_")[0]
        if tt not in confi.keys():
            confi[tt] = []
        if "from" in i["attributes"].keys() and i["attributes"]["from"] not in confi[tt]:
            confi[tt].append(i["attributes"]["from"])
        if "to" in i["attributes"].keys() and i["attributes"]["to"] not in confi[tt]:
            confi[tt].append(i["attributes"]["to"])
    if "from" in i["attributes"].keys(): 
        if len(microgrids.keys()) == 0:
            microgrids["SS_0"] = [i["attributes"]["from"],i["attributes"]["to"]]
        else:
            found = False
            for xx in microgrids.keys():
                if i["attributes"]["from"] in microgrids[xx]:
                    microgrids[xx].append(i["attributes"]["to"])
                    found = True
                elif i["attributes"]["to"] in microgrids[xx]:
                    microgrids[xx].append(i["attributes"]["from"])
                    found = True
            if not found:
                microgrids["SS_"+str(len(microgrids.keys()))] = [i["attributes"]["from"], i["attributes"]["to"]]

res_micro = {}
for i in microgrids.keys():
    for c in microgrids[i]:
        res_micro[c] = i

res = {}
rr = {}
types = {}
types_ = {}
for i in l1["objects"]:
    if i["name"] not in types.keys():
        types[i["name"]] = []
    if i["attributes"]["name"] not in types[i["name"]]:
        types[i["name"]].append(i["attributes"]["name"])

for i in l1["objects"]:
    if i["attributes"]["name"] in res_micro.keys() and ("switch" in i["name"] or "inverters" in i["name"] or "node" in i["name"]):
        att = []
        for xx in types.keys():
            for ww in types[xx]:
                if i["attributes"]["name"] in ww:
                    if res_micro[i["attributes"]["name"]] not in types_.keys():
                        types_[res_micro[i["attributes"]["name"]]] = {}
                    if xx not in types_[res_micro[i["attributes"]["name"]]].keys():
                        types_[res_micro[i["attributes"]["name"]]][xx] = []
                    if ww not in types_[res_micro[i["attributes"]["name"]]][xx]:
                        types_[res_micro[i["attributes"]["name"]]][xx].append(ww)
        for xx in list(i["attributes"].keys())[1:]: 
            ss = {}
            ss["name"] = res_micro[i["attributes"]["name"]]+"_"+i["attributes"]["name"]+"$"+xx
            ss["type"] = "string"
            ss["global"] = "false"
            ss["info"] = "{\""+i["attributes"]["name"]+"\":\""+xx+"\"}"
            att.append(xx)
            if res_micro[i["attributes"]["name"]] not in res.keys():
                res[res_micro[i["attributes"]["name"]]] = []
            res[res_micro[i["attributes"]["name"]]].append(ss)
        if res_micro[i["attributes"]["name"]] not in rr.keys():
            rr[res_micro[i["attributes"]["name"]]] = {}
        rr[res_micro[i["attributes"]["name"]]][i["attributes"]["name"]] = att

grid = {}
grid["Simulation"] = [{}]
grid["Simulation"][0]["SimTime"] = 180
grid["Simulation"][0]["StartTime"] = 0.0
grid["Simulation"][0]["PollReqFreq"] = 5
grid["Simulation"][0]["includeMIM"] = 0
grid["Simulation"][0]["UseDynTop"] = 1
grid["Simulation"][0]["MonitorPerf"] = 1
grid["microgrid"] = []
for c in sorted(list(types_.keys())):
    t = {}
    t["name"] = c
    t["dest"] = "ns3/"+c
    for i in sorted(list(types_[c].keys())):
        t[i] = types_[c][i]
    grid["microgrid"].append(t)

grid["DDoS"] = [{}]
grid["DDoS"][0]["NumberOfBots"] = 50
grid["DDoS"][0]["Active"] = 1
grid["DDoS"][0]["Start"] = 1
grid["DDoS"][0]["End"] = 11
grid["DDoS"][0]["TimeOn"] = 10.0
grid["DDoS"][0]["TimeOff"] = 0.0
grid["DDoS"][0]["PacketSize"] = 100000
grid["DDoS"][0]["Rate"] = "600000kb/s"
grid["DDoS"][0]["NodeType"] = ["MIM"]
grid["DDoS"][0]["NodeID"] = [2]

grid["MIM"] = []
overview_MIM = {}
overview_MIM["NumberAttackers"] = 3
overview_MIM["listMIM"] = "0,1,2"
grid["MIM"].append(overview_MIM)
count = 0
for c in sorted(list(types_.keys())):
    t = {}
    t["name"] = "MIM"+str(count)
    t["attack_val"] = "NA"
    t["real_val"] = "NA"
    t["node_id"] = "NA"
    t["point_id"] = "NA"
    t["scenario_id"] = "NA"
    t["attack_type"] = "NA"
    t["Start"] = 120
    t["End"] = 180
    grid["MIM"].append(t)
    count += 1

grid["controlCenter"] = {}
grid["controlCenter"]["name"] = "Monitor1"

for x in sorted(list(res.keys())):
    for ss in res[x]:
        l2["endpoints"].append(ss)

    w = {}
    w["name"] = x
    w["type"] = "string"
    w["global"] = "false"
    w["destination"] = "ns3/"+x
    w["info"] = rr[x]
    l2["endpoints"].append(w)


for i in confi.keys():
    print(len(confi[i]))


#topology configuration

topology = {}
topology["Channel"] = [{}]
topology["Channel"][0]["P2PDelay"] = "2ms"
topology["Channel"][0]["CSMAdelay"] = "6560"
topology["Channel"][0]["dataRate"] = "5Mbps"
topology["Channel"][0]["jitterMin"] = 10
topology["Channel"][0]["jitterMax"] = 100
topology["Channel"][0]["WifiPropagationDelay"] = "ConstantSpeedPropagationDelayModel"
topology["Channel"][0]["WifiRate"] = "DsssRate1Mbps"
topology["Channel"][0]["WifiStandard"] = "80211b"
topology["Gridlayout"] = [{}]
topology["Gridlayout"][0]["MinX"] = 0
topology["Gridlayout"][0]["MinY"] = 0
topology["Gridlayout"][0]["DeltaX"] = 5000
topology["Gridlayout"][0]["DeltaY"] = 5000
topology["Gridlayout"][0]["GridWidth"] = 5
topology["Gridlayout"][0]["LayoutType"] = "RowFirst"
topology["Gridlayout"][0]["SetPos"] = 1
topology["5GSetup"] = [{}]
topology["5GSetup"][0]["S1uLinkDelay"] = 1
topology["5GSetup"][0]["N1Delay"] = 0.01
topology["5GSetup"][0]["N2Delay"] = 0.01
topology["5GSetup"][0]["Srs"] = 320
topology["5GSetup"][0]["numUE"] = len(list(types_.keys()))
topology["5GSetup"][0]["numEnb"] = len(list(types_.keys()))
topology["5GSetup"][0]["scenario"] = "UMi-StreetCayon"
topology["5GSetup"][0]["txPower"] = 40
topology["Node"] = []
for i in range(len(list(types_.keys()))):
    t = {}
    t["name"] = i
    if i+1 < len(types_.keys()):
        t["connections"] = [i+1]
    else:
        t["connections"] = [0]
    t["UseCSMA"] = 0
    t["MTU"] = 1500
    t["UseWifi"] = 1
    if i%2 == 0:
        t["x"] = i+2
        t["y"] = 5*(i+10)
    else:
        t["x"] = i+8
        t["y"] = i*4
    t["error"] = "0.001"

    topology["Node"].append(t)


grid_name = "../test_conf/grid.json"
name = "../test_conf/gridlabd_config.json"
topology_name = "../test_conf/topology.json"

with open(name, "w") as outfile:
    json.dump(l2, outfile, indent = 8)

with open(grid_name, "w") as outfile:
    json.dump(grid, outfile, indent = 8)

with open(topology_name, "w") as outfile:
    json.dump(topology, outfile, indent = 8)

