import sys
f = open("../"+sys.argv[1]+".glm", "r")
data = f.read()
f.close()
data = data.split("\n")
data = data[:-1]

nodeID = sys.argv[2]
collected_IDs = {}
count = 0
collect = False
locate_node = []
for i in range(1,len(data)):
    if "{" in data[i] and not collect:
        collected_IDs[count] = []
        collect = True
    if "};" in data[i]:
        if "}" in data[i+1]:
            collected_IDs[count].append(data[i])
            collected_IDs[count].append(data[i+1])
            collect = False
            count += 1
    elif collect and "}" in data[i] and not "};" in data[i]:
        collected_IDs[count].append(data[i])
        collect = False
        count += 1
    if collect:
        collected_IDs[count].append(data[i])
    elif "#" in data[i]:
        collected_IDs[count] = []
        collected_IDs[count].append(data[i])
        count += 1
    if "\""+nodeID+"\"" in data[i]: 
        print(nodeID+ ": " + str(count) +" "+ data[i]+ "\n")
        locate_node.append(count)

connected_points = []
for i in locate_node:
    print(collected_IDs[i])
    for x in collected_IDs[i]:
        if ("parent " in x and nodeID in x) or ("to " in x and nodeID not in x) or ("from " in x and nodeID not in x):
            tt = x.replace("from ", "")
            tt = tt.replace("to ", "")
            tt = tt.replace("parent", "")
            tt = tt.replace(" ", "")
            tt = tt.replace(";", "")
            connected_points.append(tt.replace("\"", ""))

print(connected_points)

nodeDef = []
for i in locate_node:
    for x in range(len(collected_IDs[i])):
        for w in connected_points:
            if "to" in collected_IDs[i][x] and nodeID in collected_IDs[i][x]:
                if "from" in collected_IDs[i][x-1] and w not in collected_IDs[i][x-1]:
                    collected_IDs[i][x] = collected_IDs[i][x].replace(nodeID, w)
            if "from" in collected_IDs[i][x] and nodeID in collected_IDs[i][x]:
                if "to" in collected_IDs[i][x+1] and w not in collected_IDs[i][x+1]:
                    collected_IDs[i][x] = collected_IDs[i][x].replace(nodeID, w)
        if "name" in collected_IDs[i][x]:
            nodeDef.append(i)
        print(collected_IDs[i][x])
print(nodeDef)

finalSel = {}
for i in list(collected_IDs.keys()):
    if not i in nodeDef:
        finalSel[i] = collected_IDs[i]
print(len(collected_IDs.keys()))
print(len(finalSel.keys()))


#with open('editedglm.glm', 'w') as f:
#    for key, value in finalSel.items():
#        for x in value:
#            f.write(f"{x}\n")
