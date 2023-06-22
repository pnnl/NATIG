import numpy as np
import glob
import matplotlib.pyplot as plt
import math
import statistics
plt.rcParams.update({'font.size': 22})

list_files = glob.glob("2UE/*.txt")

for i in list_files:
    f = open(i, "r")
    data = f.read()
    f.close()
    data = data.split("\n")
    data = data[:-1]
    dict_fin = {}
    seen = []
    for j in range(len(data)):
        temp = data[j].split(" (")
        temp2 = data[j].split(") ")
        tt = temp[0] + " " + temp2[-1]
        addr = temp[-1].split(") ")
        addr = addr[0]
        if addr not in dict_fin:
            dict_fin[addr] = []
        #preventing duplicates
        if temp2[0] not in seen:
            dict_fin[addr].append(float(tt.split(" ")[2]))
            seen.append(temp2[0])
    print(i)
    for j in dict_fin.keys():
        dict_fin[j] = np.array(dict_fin[j])
        print(j)
        print(sum(dict_fin[j])/len(dict_fin[j]))
    #print(dict_fin)
