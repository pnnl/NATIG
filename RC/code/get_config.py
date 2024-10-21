import numpy as np
import glob
import glm
import json
import os
import sys
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("-f", "--file", dest="filename", default="ieee8500.glm", help="write report to FILE", metavar="FILE")
parser.add_argument("-nm", "--NumMicro", dest="micro", default=11, help="write report to NUMMICRO", metavar="NUMMICRO")
parser.add_argument("-na", "--NumAtt", dest="numAtt", default=3, help="write report to NUMATT", metavar="NUMATT")
parser.add_argument("-aID", "--AttID", dest="AttID", default="0,1,2", help="write report to ATTID", metavar="ATTID")

args = parser.parse_args()

list_files = glob.glob(args.filename) #sys.argv[2]) #"../ieee8500.glm")

l1 = glm.load(list_files[0])

l2 = {}
l2["name"] = "GLD"
l2["loglevel"] = 1
l2["coreType"] = "zmq"
l2["period"] = 10
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


num_group = int(args.micro) #sys.argv[1]) #len(list(microgrids.keys()))
tt = {}
kk = sorted(list(microgrids.keys()))
print("Size of keys: "+str(len(kk)))
for i in range(0,len(kk)):
    if "SS_"+str(i%num_group) not in tt.keys():
        tt["SS_"+str(i%num_group)] = []
    for x in microgrids[kk[i]]:
        tt["SS_"+str(i%num_group)].append(x)
microgrids = tt

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
    if "name" in i["attributes"].keys():
        if i["attributes"]["name"] not in types[i["name"]]:
            types[i["name"]].append(i["attributes"]["name"])

points = {}
for i in l1["objects"]:
    if "name" in i["attributes"].keys():
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
                point_type = "ANALOG"
                if not "voltage" in xx and not "current" in xx and not "Pref" in xx and not "Qref" in xx:
                    point_type = "BINARY"
                if "flags" not in xx:
                    ss = {}
                    ss["name"] = res_micro[i["attributes"]["name"]]+"_"+i["attributes"]["name"]+"$"+xx
                    ss["type"] = "string"
                    ss["global"] = False
                    ss["info"] = "{\""+i["attributes"]["name"]+"\":\""+xx+"\"}"
                    att.append(xx)
                    if res_micro[i["attributes"]["name"]] not in res.keys():
                        res[res_micro[i["attributes"]["name"]]] = []
                    res[res_micro[i["attributes"]["name"]]].append(ss)
                    if res_micro[i["attributes"]["name"]] not in points.keys():
                        points[res_micro[i["attributes"]["name"]]] = ""
                    if "ANALOG" in point_type:
                        points[res_micro[i["attributes"]["name"]]] += point_type +","+i["attributes"]["name"] + "$" + xx + ".real,0\n"
                        points[res_micro[i["attributes"]["name"]]] += point_type +","+i["attributes"]["name"] + "$" + xx + ".imag,0\n"
                    else:
                        points[res_micro[i["attributes"]["name"]]] += point_type +","+i["attributes"]["name"] + "$" + xx + ",0\n"
            if res_micro[i["attributes"]["name"]] not in rr.keys():
                rr[res_micro[i["attributes"]["name"]]] = {}
            rr[res_micro[i["attributes"]["name"]]][i["attributes"]["name"]] = att


for i in sorted(list(points.keys())):
    f = open("test_conf/points_"+i+".csv", "w")
    f.write(points[i])
    f.close()

grid = {}
grid["Simulation"] = [{}]
grid["Simulation"][0]["SimTime"] = 35
grid["Simulation"][0]["StartTime"] = 0.0
grid["Simulation"][0]["PollReqFreq"] = 5
grid["Simulation"][0]["includeMIM"] = 0
grid["Simulation"][0]["UseDynTop"] = 0
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
overview_MIM["NumberAttackers"] = int(args.numAtt)
overview_MIM["listMIM"] = args.AttID
grid["MIM"].append(overview_MIM)
count = 0
attackerID = args.AttID.split(",")
for c in sorted(list(types_.keys())):
    t = {}
    f = False
    for x in attackerID:
        if count == int(x):
            f = True
    if f: #count == int(attackerID[count]):
        default = input("Do you want to use the default value?(n/y)")
        if "N" in default or "n" in default:
            t["name"] = "MIM"+str(count)
            t["attack_val"] = input("What is the attack value? format: \"Val1,Val2,...,ValN\" with N being the number of attacked point: ")
            t["real_val"] = input("What is the real value? format: \"Val1,Val2,...,ValN\" with N being the number of attacked point: ")
            t["node_id"] = input("What nodes are under attack? format: \"ID1,ID2,...,IDN\" with N being the number of attacked point. Ex ID: node101: ")
            t["point_id"] = input("What points are under attacked? format: \"Point1,Point2,...,PointN\" with N being the number of attacked point. Ex Point: status. ")
            t["scenario_id"] = input("What scenario id are you choosing? format: \"(a|b)\". For attack type 4, if you choose a then the attack value is set once and does not fluctuate. If you select b, the attack value is random fluctuated between the selected attack value and the real value. If you are choosing attack type 3, which is an attack done on switches, this input is not use. Please default to b: ")
            t["attack_type"] = input("What attack type are you choosing? format: \"(3|4)\". 3 is the attack type that allows you to modify points that use strings as inputs like the status of a switch. 4 is the attack type that allows you to modify points that use numbers as inputs like the Pref value of inverters. ")
            t["Start"] = input("What time does the current round of attack starts? format: \"Start of current round of attacks in seconds\". Ex: \"10\". ")
            t["End"] = input("What time does the current round of attack ends? format: \"End of current round of attacks in seconds\". Ex: \"35\". ")
            t["PointStart"] = input("What time does the individual point get attacked? format: \"Start1, Start2, ..., StartN\" with N being the number of points under attack. It has to be greater then or equal to the overall start value. Ex: \"10,12\". ")
            t["PointStop"] = input("What time does the individual point get back to normal? format: \"End1, End2, ..., EndN\" with N being the number of points under attack. It has to be less then or equal to the overall end value. Ex: \"44,50\". ")
        else:
            t["name"] = "MIM"+str(count)
            t["attack_val"] = "TRIP"
            t["real_val"] = "NA"
            t["node_id"] = "microgrid_switch1"
            t["point_id"] = "status"
            t["scenario_id"] = "b"
            t["attack_type"] = "3"
            t["Start"] = 10
            t["End"] = 35
            t["PointStart"] = 10
            t["PointStop"] = 35
    else:
        t["name"] = "MIM"+str(count)
        t["attack_val"] = "TRIP"
        t["real_val"] = "NA"
        t["node_id"] = "microgrid_switch1"
        t["point_id"] = "status"
        t["scenario_id"] = "b"
        t["attack_type"] = "3"
        t["Start"] = 10
        t["End"] = 35
        t["PointStart"] = 10,
        t["PointStop"] = 35
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
    w["global"] = False
    w["destination"] = "ns3/"+x
    ss = "{"
    for zz in rr[x].keys():
        ss += "\"" + zz + "\":[\""
        for aa in rr[x][zz][:-1]:
            ss += aa + "\",\""
        ss += rr[x][zz][-1] + "\"],"
    w["info"] = ss[:-1]+"}"
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
topology["Channel"][0]["P2PRate"] = "150Mb/s"
topology["Channel"][0]["MTU"] = 1500
topology["Channel"][0]["delay"] = 0.1
topology["Gridlayout"] = [{}]
"""
 "MinX": 0,
 "MinY": 0,
 "DeltaX": 50,
 "DeltaY": 20,
 "GridWidth": 11,
 "distance": 11,
 "GnBH": 10.0,
 "UEH": 1.5,
 "LayoutType": "RowFirst",
 "SetPos": 0
"""
topology["Gridlayout"][0]["MinX"] = 0
topology["Gridlayout"][0]["MinY"] = 0
topology["Gridlayout"][0]["DeltaX"] = 50
topology["Gridlayout"][0]["DeltaY"] = 50
topology["Gridlayout"][0]["GridWidth"] = len(list(types_.keys()))
topology["Gridlayout"][0]["distance"] = len(list(types_.keys()))
topology["Gridlayout"][0]["LayoutType"] = "RowFirst"
topology["Gridlayout"][0]["SetPos"] = 0
topology["5GSetup"] = [{}]
topology["5GSetup"][0]["S1uLinkDelay"] = 0.01
topology["5GSetup"][0]["N1Delay"] = 0.01
topology["5GSetup"][0]["N2Delay"] = 0.01
qw = len(list(types_.keys()))
xxw = 5
if qw > 160:
    xxw = 320
elif qw > 80:
    xxw = 160
elif qw > 40:
    xxw = 80
elif qw > 20:
    xxw = 40
elif qw > 10:
    xxw = 20
elif qw > 5:
    xxw = 10
topology["5GSetup"][0]["Srs"] = xxw
topology["5GSetup"][0]["UeRow"] = 2 #qw/2 + 1
topology["5GSetup"][0]["UeCol"] = 4 # qw
topology["5GSetup"][0]["GnBRow"] = 4 #qw
topology["5GSetup"][0]["GnBCol"] = 8 #qw/2 + 1
topology["5GSetup"][0]["numUE"] = qw
topology["5GSetup"][0]["numEnb"] = qw
topology["5GSetup"][0]["CentFreq1"] = 28000000000.0
topology["5GSetup"][0]["CentFreq2"] = 28200000000.0
topology["5GSetup"][0]["Band1"] = 150000000.0
topology["5GSetup"][0]["Band2"] = 150000000.0
topology["5GSetup"][0]["num1"] = 2
topology["5GSetup"][0]["num2"] = 0
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


grid_name = "test_conf/grid.json"
name = "test_conf/gridlabd_config.json"
topology_name = "test_conf/topology.json"

with open(name, "w") as outfile:
    json.dump(l2, outfile, indent = 8)

with open(grid_name, "w") as outfile:
    json.dump(grid, outfile, indent = 8)

with open(topology_name, "w") as outfile:
    json.dump(topology, outfile, indent = 8)

