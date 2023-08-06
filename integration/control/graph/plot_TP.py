import numpy as np
import glob
import matplotlib.pyplot as plt
import math
import statistics
plt.rcParams.update({'font.size': 11})

list_files = glob.glob("collected/*.txt")
fid = {}
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
            dict_fin[addr].append(float(tt.split(" ")[2])) #((float(tt.split(" ")[-4])*float(tt.split(" ")[-2]))/float(tt.split(" ")[5]))*1000000000)
            seen.append(temp2[0])
    print(i)
    ID_arr = []
    val_arr = []
    kk = sorted(list(dict_fin.keys()))
    for j in kk:
        dict_fin[j] = np.array(dict_fin[j])
        if "UDP 1." in j:
            tt = j.split("-->")
            p = tt[0].split("/ ")
            p = p[-1]
            tt = tt[-1].split(".")
            ID = "CC/"+str(int(p))+" --> MG"+str(int(float(tt[1])-17))
            x = []
            for s in range(1,len(dict_fin[j])):
                x.append((float(dict_fin[j][s]))) #-float(dict_fin[j][s-1]))/0.4)
            val = sum(x)/len(x)
            if val == 0:
                val = float(dict_fin[j][0])
            print(ID +" : "+str(val))
            ID_arr.append(ID)
            val_arr.append(val)
        elif "UDP 172." in j:
            tt = j.split(".")
            p = j.split("-->")
            p = p[1].split("/ ")
            ID = "MG"+str(int(float(tt[1])-17))+" --> CC/"+str(int(p[-1]))
            x = []
            for s in range(1, len(dict_fin[j])):
                x.append((float(dict_fin[j][s]))) #-float(dict_fin[j][s-1]))/0.4)
            val = sum(x)/len(x)
            if val == 0:
                val = float(dict_fin[j][0])
            print(ID+" : "+str(val))
            ID_arr.append(ID)
            val_arr.append(val)
    fid[i] = [ID_arr, val_arr]

max_length = 0
all_ID = []
for key in fid.keys():
    if len(fid[key][1]) > max_length:
        max_length = len(fid[key][1])
        all_ID = fid[key][0]

for key in fid.keys():
    if len(fid[key][1]) < max_length:
        new_ID = []
        new_val = []
        for x in range(len(all_ID)):
            if all_ID[x] not in fid[key][0]:
                new_ID.append(all_ID[x])
                new_val.append(0)
            else:
                new_ID.append(all_ID[x])
                new_val.append(fid[key][1][fid[key][0].index(all_ID[x])])
        fid[key] = [new_ID, new_val]

x = np.arange(len(all_ID))  # the label locations
width = 0.25  # the width of the bars
multiplier = 0
fig, ax = plt.subplots()
for s in fid.keys():
    offset = width * multiplier
    lab = s.replace(".txt", "")
    lab = lab.split("/")[-1]
    rects = ax.bar(x + offset, fid[s][1], width=width, label=lab)
    #ax.bar_label(rects, padding=3)
    multiplier += 1

ax.set_ylabel('Throughput Kbps')
ax.set_title('Path measured')
ax.set_xticks(x + width)
ax.set_xticklabels(all_ID, rotation=45)
ax.legend(loc='upper left') #, ncols=3)
plt.tight_layout()
plt.savefig('TP.png')
