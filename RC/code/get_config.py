import numpy as np
import glob
import glm
import json
import os
import sys
import difflib
import random
from matplotlib import pyplot as plt
from matplotlib import pylab
from matplotlib import colors as mcolors
import networkx as nx
from argparse import ArgumentParser

if not os.path.exists("test_conf"):
    os.mkdir("test_conf")

def get_overlap(s1, s2):
    s = difflib.SequenceMatcher(None, s1, s2)
    pos_a, pos_b, size = s.find_longest_match(0, len(s1), 0, len(s2))
    return s1[pos_a:pos_a+size]

def generate_edges(graph): 
    edges = [] 
  
    # for each node in graph 
    for node in graph: 
          
        # for each neighbour node of a single node 
        for neighbour in graph[node]: 
              
            # if edge exists then append 
            edges.append((node, neighbour)) 
    return edges 

def save_graph(graph,file_name):
    #initialze Figure
    plt.figure(num=None, figsize=(20, 20), dpi=80)
    plt.axis('off')
    fig = plt.figure(1)
    pos = nx.spring_layout(graph)
    nx.draw_networkx_nodes(graph,pos)
    nx.draw_networkx_edges(graph,pos)
    nx.draw_networkx_labels(graph,pos)

    cut = 1.00
    xmax = cut * max(xx for xx, yy in pos.values())
    ymax = cut * max(yy for xx, yy in pos.values())
    plt.xlim(0, xmax)
    plt.ylim(0, ymax)

    plt.savefig(file_name,bbox_inches="tight")
    pylab.close()
    del fig

"""
How to use with the 3000 model: python3 get_config.py -f 3000-model-glm/3000_model.glm -nm 310 -na 1 -aID "6" -div "swt_g9343_48332_sw,swt_ln4651075_sw,swt_ln4641075_sw,swt_ln0956471_sw,swt_ln4625713_sw,swt_ln0863704_sw,swt_ln0895780_sw,swt_ln0742811_sw,swt_ln0621886_sw,swt_tsw30473047_sw,swt_ln0108145_sw" -r "swt_hvmv69s3b2_sw"
"""

parser = ArgumentParser()
parser.add_argument("-f", "--file", dest="filename", default="ieee8500.glm", help="write report to FILE", metavar="FILE")
parser.add_argument("-nm", "--NumMicro", dest="micro", default=11, help="write report to NUMMICRO", metavar="NUMMICRO")
parser.add_argument("-na", "--NumAtt", dest="numAtt", default=3, help="write report to NUMATT", metavar="NUMATT")
parser.add_argument("-aID", "--AttID", dest="AttID", default="0,1,2", help="write report to ATTID", metavar="ATTID")
parser.add_argument("-p", "--port", dest="port", default=9000, help="write report to PORT", metavar="PORT")
parser.add_argument("-div", "--division", dest="div", default="default_div", help="write report to DIV", metavar="DIV")
parser.add_argument("-r", "--root", dest="root", default="root", help="write report to ROOT", metavar="ROOT")


args = parser.parse_args()

list_files = glob.glob(args.filename) #sys.argv[2]) #"../ieee8500.glm")

l1 = glm.load(list_files[0])

l2 = {}
l2["name"] = "GLD"
l2["loglevel"] = 1
l2["coreType"] = "zmq"
l2["period"] = 1
l2["slow_responding"] = "True"
l2["broker"] = "127.0.0.1:" + str(args.port)
l2["brokerPort"] = args.port
l2["broker_rank"] = 0
l2["endpoints"] = []

microgrids = {}
confi = {}
connections = {}
for i in l1["objects"]:
    if "configuration" in i["attributes"].keys():
        tt = i["attributes"]["configuration"].split("_")[0]
        if tt not in confi.keys():
            confi[tt] = []
        if "from" in i["attributes"].keys() and i["attributes"]["from"] not in confi[tt]:
            confi[tt].append(i["attributes"]["from"])
        if "to" in i["attributes"].keys() and i["attributes"]["to"] not in confi[tt]:
            confi[tt].append(i["attributes"]["to"])
    if "from" in i["attributes"].keys(): #and ("sx" not in i["attributes"]["name"] or "sx" not in i["attributes"]["to"] or "sx" not in i["attributes"]["from"] ): 
        if i["attributes"]["from"] not in connections.keys():
            connections[i["attributes"]["from"]] = [i["attributes"]["name"]]
        else:
            connections[i["attributes"]["from"]].append(i["attributes"]["name"])
        if i["attributes"]["name"] not in connections.keys():
            connections[i["attributes"]["name"]] = [i["attributes"]["from"]]
            connections[i["attributes"]["name"]] = [i["attributes"]["to"]]
        else:
            connections[i["attributes"]["name"]].append(i["attributes"]["to"])
            connections[i["attributes"]["name"]].append(i["attributes"]["from"])
        if i["attributes"]["to"] not in connections.keys():
            connections[i["attributes"]["to"]] = [i["attributes"]["name"]]
        else:
            connections[i["attributes"]["to"]].append(i["attributes"]["name"])
    elif "parent" in i["attributes"].keys() and "recorder" not in i["name"]:
        if i["attributes"]["parent"] not in connections.keys():
            connections[i["attributes"]["parent"]] = [i["attributes"]["name"]]
        else:
            connections[i["attributes"]["parent"]].append(i["attributes"]["name"])
        if i["attributes"]["name"] not in connections.keys():
            connections[i["attributes"]["name"]] = [i["attributes"]["parent"]]
        else:
            connections[i["attributes"]["name"]].append(i["attributes"]["parent"])

mm = {}
for s in connections.keys():
    if len(connections[s]) not in mm.keys():
        mm[len(connections[s])] = []
    mm[len(connections[s])].append(s)

con_keys = sorted(list(mm.keys()))
con_keys = con_keys[::-2]
micro_nodes = []
for i in con_keys:
    for x in mm[i]:
        micro_nodes.append(x)
tree = {}
ff = []

recorders = []
swing_nodes = []
microgrid_source = {}
microgrid_loads = {}
for i in l1["objects"]:
    if "bustype" in i["attributes"].keys():
        if "SWING" in i["attributes"]["bustype"]:
            swing_nodes.append(i["attributes"]["name"])
    if "recorder" in i["name"] and "group" not in i["name"]:
        recorders.append(i["attributes"]["parent"])
    if "parent" in i["attributes"].keys() and "recorder" not in i["name"]:
        if "load" in i["name"]:
            microgrid_loads[i["attributes"]["name"]] = [i["attributes"]["nominal_voltage"]]
        if "inverter" in i["name"]:
            microgrid_source[i["attributes"]["name"]] = [i["attributes"]["P_Out"]]
            microgrid_source[i["attributes"]["name"]].append(i["attributes"]["Q_Out"])
        if len(microgrids.keys()) == 0:
            microgrids["SS_1"] = [i["attributes"]["parent"], i["attributes"]["name"]]
        else:
            found = False
            for xx in microgrids.keys():
                if i["attributes"]["parent"] in microgrids[xx]:
                    microgrids[xx].append(i["attributes"]["name"])
                    found = True
                elif i["attributes"]["name"] in microgrids[xx]:
                    microgrids[xx].append(i["attributes"]["parent"])
                    found = True
                #else:
                #    for items in  microgrids[xx]:
                #        if len(get_overlap(i["attributes"]["parent"], items)) > 5:
                #            microgrids[xx].append(i["attributes"]["parent"])
                #            microgrids[xx].append(i["attributes"]["name"])
                #            found = True
                #            break
            if not found:
                # print("SS_"+str(len(microgrids.keys())+1))
                microgrids["SS_"+str(len(microgrids.keys())+1)] = [i["attributes"]["parent"], i["attributes"]["name"]]
    elif "from" in i["attributes"].keys(): 
        if len(microgrids.keys()) == 0:
            microgrids["SS_1"] = [i["attributes"]["from"], i["attributes"]["name"],i["attributes"]["to"]]
        else:
            found = False
            for xx in microgrids.keys():
                if i["attributes"]["from"] in microgrids[xx]:
                    microgrids[xx].append(i["attributes"]["name"])
                    microgrids[xx].append(i["attributes"]["to"])
                    found = True
                elif i["attributes"]["to"] in microgrids[xx]:
                    microgrids[xx].append(i["attributes"]["name"])
                    microgrids[xx].append(i["attributes"]["from"])
                    found = True
                #else:
                #    for items in  microgrids[xx]:
                #        if len(get_overlap(i["attributes"]["from"], items)) > 5:
                #            microgrids[xx].append(i["attributes"]["from"])
                #            microgrids[xx].append(i["attributes"]["name"])
                #            microgrids[xx].append(i["attributes"]["to"])
                #            found = True
                #            break
            if not found:
                # print("SS_"+str(len(microgrids.keys())+1))
                microgrids["SS_"+str(len(microgrids.keys())+1)] = [i["attributes"]["from"], i["attributes"]["name"], i["attributes"]["to"]]

G = generate_edges(connections)
g2 = nx.DiGraph()
g2.add_edges_from(G)

from networkx import *
import itertools
print("node degree clustering")
k = int(args.micro) #int(len(list(G))/20)
def divide_graph(graph, division_nodes):
    """
    Divides a graph into subgroups based on specified division nodes.

    Parameters:
    -----------
    graph : nx.Graph
        The input NetworkX graph to be divided
    division_nodes : List[Any]
        List of node IDs that will serve as division points

    Returns:
    --------
    List[Set[Any]]
        A list of sets, where each set contains nodes belonging to one subgroup
    """
    # Ensure all division nodes exist in the graph
    for node in division_nodes:
        if node not in graph.nodes:
            raise ValueError(f"Node {node} not found in the graph")

    # Create a copy of the graph to work with
    work_graph = graph.copy()

    # Remove division nodes from the graph
    work_graph.remove_nodes_from(division_nodes)

    # Find connected components (these will be our groups)
    components = list(nx.connected_components(work_graph))

    # For each division node, determine which component it should join
    # based on its connections in the original graph
    for div_node in division_nodes:
        # Get neighbors of the division node in the original graph
        neighbors = set(graph.neighbors(div_node))

        # Find which components these neighbors belong to
        connected_components = []
        for i, component in enumerate(components):
            if neighbors & component:  # If there's an intersection
                connected_components.append(i)

        # If division node is connected to exactly one component, add it to that component
        if len(connected_components) == 1:
            components[connected_components[0]].add(div_node)
        # If connected to multiple or no components, create a new component for it
        else:
            components.append({div_node})

    updated_components = components[1:-len(division_nodes)]
    chunks = []
    for i in updated_components:
        chunks.append(list(i))
    updated_chunks = []
    for i in range(len(chunks)):
        t = []
        t.append(division_nodes[i])
        for j in chunks[i]:
            t.append(j)
        updated_chunks.append(t)
    return updated_chunks
def nx_chunk(graph, chunk_size):
    """
    Chunk a graph into subgraphs with the specified minimum chunk size.

    Inspired by Lukes algorithm.
    """

    # convert to a tree;
    # a balanced spanning tree would be preferable to a minimum spanning tree,
    # but networkx does not seem to have an implementation of that
    tree = nx.minimum_spanning_tree(graph)

    # select a root that is maximally far away from all leaves
    leaves = [node for node, degree in tree.degree() if degree == 1]
    prop_div = args.div.split(",")
    #print(prop_div)
    #print(leaves)
    minimum_distance_to_leaf = {node : tree.size() for node in tree.nodes()}
    for leaf in leaves:
        distances = nx.single_source_shortest_path_length(tree, leaf)
        for node, distance in distances.items():
            if distance < minimum_distance_to_leaf[node]:
                minimum_distance_to_leaf[node] = distance
    root = max(minimum_distance_to_leaf, key=minimum_distance_to_leaf.get)
    #print("-----------------------------------------------")
    #print(root)

    # make the tree directed and compute the total descendants of each node
    tree = nx.dfs_tree(tree, args.root) #"swt_hvmv69s3b2_sw")
    total_descendants = get_total_descendants(tree)
    chunks = []
    if "default_div" not in args.div:
        for x in prop_div:
            print(x + ": " + str(total_descendants[x]))
            for node in tree.nodes():
                if x in node:
                    #print("found: "+ x)
                    #print(nx.ancestors(tree, node))
                    tt = []
                    start_End = ""
                    for x2 in prop_div:
                        if x2 not in x:
                            #print("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz")
                            for path in nx.all_simple_paths(tree, source=node, target=x2):
                                found = False
                                for x3 in prop_div:
                                    if x3 in path[1:-1]:
                                        found = True
                                if not found and len(path[1:-1]) > len(tt) and len(path[1:-1]) > 8:
                                    tt = path #[1:-1]
                                    start_End = node + "," + x2
                    if len(tt) < 1:
                        ancestor = list(nx.ancestors(tree, node))
                        found = False
                        for x3 in prop_div:
                            if x3 in ancestor and x3 not in node:
                                found = True
                        if not found:
                            #print("Using ancestors")
                            tt = ancestor
                        else:
                            descendants = list(nx.descendants(tree, node))
                            found = False
                            for x3 in prop_div:
                                if x3 in descendants and x3 not in node:
                                    found = True
                            if not found:
                                print("Using descendants")
                                tt = descendants
                    #print(start_End)
                    print(len(tt))
                    if len(tt) > 0:
                        chunks.append(tt)
                    #print("----------------------------------------")
    else:
        # prune chunks, starting from the leaves
        max_descendants = np.max(list(total_descendants.values()))
        while (max_descendants + 1 > chunk_size) & (tree.size() >= 2 * chunk_size):
            for node in list(nx.topological_sort(tree))[::-1]: # i.e. from leaf to root
                if (total_descendants[node] + 1) >= chunk_size:
                    chunk = list(nx.descendants(tree, node))
                    chunk.append(node)
                    chunks.append(chunk)

                    # update relevant data structures
                    tree.remove_nodes_from(chunk)
                    total_descendants = get_total_descendants(tree)
                    max_descendants = np.max(list(total_descendants.values()))

                    break

        # handle remainder
        chunks.append(list(tree.nodes()))

    return chunks


def get_total_descendants(dag):
    return {node : len(nx.descendants(dag, node)) for node in dag.nodes()}
g2 = g2.to_undirected()
node_list = args.div.split(",")
chunks = divide_graph(g2, node_list)
#print(out)
#chunks = nx_chunk(g2, k)
print(len(chunks))
tt = {}
for i in range(len(chunks)):
    k = "SS_"+str(i+1)
    tt[k] = chunks[i]
microgrids = tt
g2 = g2.to_directed()
print("----------------------------------------")

keys = list(microgrids.keys())
print(microgrids.keys())


num_group = len(list(microgrids.keys())) #int(sys.argv[1]) #len(list(microgrids.keys()))
tt = {}
kk = sorted(list(microgrids.keys()))
print("Size of keys: "+str(len(kk)))
for i in range(0,len(kk)):
    if "SS_"+str(i%num_group + 1) not in tt.keys():
        tt["SS_"+str(i%num_group + 1)] = []
    for x in microgrids[kk[i]]:
        tt["SS_"+str(i%num_group + 1)].append(x)
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
        if "inverter" in i["name"]:
            print(i["attributes"]["name"] in res_micro.keys())
            print(i["attributes"]["name"])
        if i["attributes"]["name"] in res_micro.keys() and ("switch" in i["name"] or "inverter" in i["name"] or "node" in i["name"] or "diesel" in i["name"]):
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
                if "flags" not in xx and "continuous" not in xx and "emergency" not in xx and "frequency_measure_type" not in xx:
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
grid["Simulation"][0]["SimTime"] = 60
grid["Simulation"][0]["StartTime"] = 0.0
grid["Simulation"][0]["PollReqFreq"] = 15
grid["Simulation"][0]["includeMIM"] = int(input("Do you want to simulate MIM? (0|1)"))
grid["Simulation"][0]["UseDynTop"] = 0
grid["Simulation"][0]["MonitorPerf"] = 0
grid["Simulation"][0]["StaticSeed"] = 0
grid["Simulation"][0]["RandomSeed"] = 777
grid["microgrid"] = []
for c in sorted(list(types_.keys())):
    t = {}
    t["name"] = c
    t["dest"] = "ns3/"+c
    for i in sorted(list(types_[c].keys())):
        t[i] = types_[c][i]
    grid["microgrid"].append(t)

grid["Controller"] = [{}]
grid["Controller"][0]["use"] = 0
grid["Controller"][0]["actionFile"] = "AgentDecisions11.txt"
grid["Controller"][0]["NodesControlled"] = [1,2,3,4]
grid["DDoS"] = [{}]
grid["DDoS"][0]["NumberOfBots"] = 4
grid["DDoS"][0]["threadsPerAttacker"] = 2
grid["DDoS"][0]["Active"] = int(input("Do you want to simulate DDoS? (0|1)"))
grid["DDoS"][0]["Start"] = 1
grid["DDoS"][0]["End"] = 11
grid["DDoS"][0]["TimeOn"] = 10.0
grid["DDoS"][0]["TimeOff"] = 0.0
grid["DDoS"][0]["PacketSize"] = 1500
grid["DDoS"][0]["Rate"] = "80Mb/s"
grid["DDoS"][0]["NodeType"] = ["subNode"]
grid["DDoS"][0]["NodeID"] = [2]
grid["DDoS"][0]["endPoint"] = "CC"

#grid["Controller"] = [{}]
#grid["Controller"][0]["use"] = 1

#grid["DDoS"] = [{}]
#grid["DDoS"][0]["NumberOfBots"] = 4
#grid["DDoS"][0]["Active"] = 1
#grid["DDoS"][0]["Start"] = 10
#grid["DDoS"][0]["End"] = 35
#grid["DDoS"][0]["TimeOn"] = 35.0
#grid["DDoS"][0]["TimeOff"] = 0.0
#grid["DDoS"][0]["PacketSize"] = 1500
#grid["DDoS"][0]["Rate"] = "80480kb/s"
#grid["DDoS"][0]["NodeType"] = ["MIM"]
#grid["DDoS"][0]["NodeID"] = [2]
#grid["DDoS"][0]["endPoint"] = "CC"

#grid["MIM"] = []
#overview_MIM = {}
#grid["MIM"].append(overview_MIM)
#count = 0
#IDMIM = {}
#IDM = ""
#for i in MIM_switches:
#    cc = 0
#    for x in microgrids.keys():
#        if i in microgrids[x]:
#            if cc not in IDMIM.keys():
#                IDMIM[cc] = []
#                IDM += str(cc) + ","
#            IDMIM[cc].append(i)
#        cc += 1
grid["MIM"] = []
overview_MIM = {}
overview_MIM["NumberAttackers"] = int(args.numAtt)
overview_MIM["listMIM"] = args.AttID
grid["MIM"].append(overview_MIM)
count = 0
attackerID = args.AttID.split(",")
# Setting the keys up so that the SS_10 and above and happening after SS_9
key_IDs = list(types_.keys())
dd = {}
for x in key_IDs:
    dd[int(x.replace("SS_", ""))] = x
#for c in sorted(list(types_.keys())):
for x in sorted(list(dd.keys())):
    c = dd[x]
    t = {}
    f = False
    attID = 0
    for x in attackerID:
        print(c)
        print(int(x)+1)
        if int(x)+1 == int(c.replace("SS_", "")):
            f = True
            attID = int(x)
    if f: #count == int(attackerID[count]):
        default = input("Do you want to use the default value for MIM "+str(attID)+"?(n/y)")
        if "N" in default or "n" in default:
            t["name"] = "MIM"+str(attID)
            cont = True
            AV = ""
            AC = ""
            AT = ""
            SID = ""
            RV = ""
            NI = ""
            PI = ""
            S = []
            Ss = ""
            E = []
            Es = ""
            while cont:
                IDs = sorted(list(dd.keys()))
                for ww in range(len(IDs)):
                    IDs[ww] = dd[IDs[ww]]
                available_types = list(types_[IDs[count]].keys())
                type_nodes = input("Which type of node do you want to attack? Available types: " + str(available_types) + " ")
                while type_nodes not in available_types:
                    type_nodes = input("Please choose from the available types: " + str(available_types)+ " ")
                available_nodes = types_[IDs[count]][type_nodes]
                NodeID = input("Which "+type_nodes+" do you want to attack? Available types: " + str(available_nodes)+ " ")
                while NodeID not in available_nodes:
                    NodeID = input("Please choose from the available "+type_nodes+": " + str(available_nodes)+ " ")
                PointID = input("What point do you want to attack? Example for switches you could attack the status ")
                Real_val = input ("What is the value of the chosen point outside of attacks? If you choose NA, your point will not be reset after the attack ends ")
                Attack_Val = input ("What is the attack value of your chosen point? ")
                Attack_Start = int(input("When does the attack start? "))
                Attack_End = int(input("When does the attack end? "))
                AttackChance = float(input("What is the attack chance of success? "))
                scenarioID = input("What scenario id are you choosing? format: \"(a|b)\". For attack type 4, if you choose a then the attack value is set once and does not fluctuate. If you select b, the attack value is random fluctuated between the selected attack value and the real value. If you are choosing attack type 3, which is an attack done on switches, this input is not use. Please default to b: ")
                attackType =  input("What attack type are you choosing? format: \"(3|4)\". 3 is the attack type that allows you to modify points that use strings as inputs like the status of a switch. 4 is the attack type that allows you to modify points that use numbers as inputs like the Pref value of inverters. ")
                AV += Attack_Val + ","
                RV += Real_val + ","
                NI += NodeID + ","
                PI += PointID + ","
                SID += scenarioID + ","
                AT += attackType + ","
                AC += str(AttackChance) + ","
                S.append(Attack_Start)
                Ss += str(Attack_Start) + ","
                E.append(Attack_End)
                Es += str(Attack_End) + ","
                cont = int(input("Attack another node? (0|1): "))


            t["attack_val"] = AV[:-1]
            t["real_val"] = RV[:-1]
            t["node_id"] = NI[:-1]
            t["point_id"] = PI[:-1]
            t["scenario_id"] = SID[:-1] 
            t["attack_type"] = AT[:-1] 
            t["attack_chance"] = AC[:-1]
            t["Start"] = min(S)
            t["End"] = max(E)

            t["PointStart"] = Ss[:-1]
            t["PointStop"] = Es[:-1]
        else:
            t["name"] = "MIM"+ str(int(c.replace("SS_", ""))-1)
            t["attack_val"] = "TRIP"
            t["real_val"] = "NA"
            t["node_id"] = "microgrid_switch1"
            t["point_id"] = "status"
            t["scenario_id"] = "b"
            t["attack_type"] = "3"
            t["attack_chance"] = "1.0"
            t["Start"] = "10"
            t["End"] = "35"
            t["PointStart"] = "10"
            t["PointStop"] = "35"
    else:
        t["name"] = "MIM"+ str(int(c.replace("SS_", ""))-1)
        t["attack_val"] = "TRIP"
        t["real_val"] = "NA"
        t["node_id"] = "microgrid_switch1"
        t["point_id"] = "status"
        t["scenario_id"] = "b"
        t["attack_type"] = "3"
        t["attack_chance"] = "1.0"
        t["Start"] = 10
        t["End"] = 35
        t["PointStart"] = 10
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
topology["Channel"][0]["delay"] = 0 
topology["Gridlayout"] = [{}]
topology["Gridlayout"][0]["MinX"] = 0
topology["Gridlayout"][0]["MinY"] = 0
topology["Gridlayout"][0]["DeltaX"] = 40
topology["Gridlayout"][0]["DeltaY"] = 80
topology["Gridlayout"][0]["GridWidth"] = len(list(types_.keys()))
topology["Gridlayout"][0]["distance"] = len(list(types_.keys()))
topology["Gridlayout"][0]["GnBH"] = 10.0
topology["Gridlayout"][0]["UEH"] = 1.5
topology["Gridlayout"][0]["LayoutType"] = "RowFirst"
topology["Gridlayout"][0]["SetPos"] = 0
topology["5GSetup"] = [{}]
topology["5GSetup"][0]["S1uLinkDelay"] = 0
topology["5GSetup"][0]["N1Delay"] = 0.01
topology["5GSetup"][0]["N2Delay"] = 0.01
qw = len(list(types_.keys()))
xxw = 5
if qw*2 > 160:
    xxw = 320
elif qw*2 > 80:
    xxw = 160
elif qw*2 > 40:
    xxw = 80
elif qw*2 > 20:
    xxw = 40
elif qw*2 > 10:
    xxw = 20
elif qw*2 > 5:
    xxw = 10
topology["5GSetup"][0]["Srs"] = xxw
topology["5GSetup"][0]["UeRow"] = int((qw-2)/2)
topology["5GSetup"][0]["UeCol"] = int(qw - 2) 
topology["5GSetup"][0]["GnBRow"] = int(qw - 2)
topology["5GSetup"][0]["GnBCol"] = int((qw-2)/2) 
topology["5GSetup"][0]["numUE"] = qw
topology["5GSetup"][0]["numEnb"] = qw
topology["5GSetup"][0]["CentFreq1"] = 28e9
topology["5GSetup"][0]["CentFreq2"] = 28.2e9
topology["5GSetup"][0]["Band1"] = 150e6
topology["5GSetup"][0]["Band2"] = 150e6
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
