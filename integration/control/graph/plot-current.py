import numpy as np
import glob
import sys
import matplotlib.pyplot as plt
import math
import statistics
plt.rcParams.update({'font.size': 15})

#Getting the switch data
list_of_files = glob.glob("../gen-*/MG_3_GFA_status.csv")
to_graph = {}
for i in list_of_files:
    if "MG" in i:
        temp = i.split("/")
        temp = temp[-1].split(".")
        temp = temp[0]
        f = open(i, "r")
        data = f.read()
        f.close()
        data = data.split("\n")
        data = data[:-1]
        t = []
        for j in data:
            if "2001" in j:
                tp = j.split(",")
                t.append(float(tp[2:][0]))
        data = np.array(t)
        to_graph[i] = data

#Getting the inverter data
list_of_files_inv = glob.glob("../inverter-scenario4.A/*")
Qref = {}
Pref = {}
current = {}
VAout = {}
for i in list_of_files_inv:
    f = open(i, "r")
    data = f.read()
    f.close()
    data = data.split("\n")
    data = data[:-1]
    t = []
    for j in data:
        if "2001" in j:
            tp = j.split(",")
            t.append(tp[1:])
    data = np.array(t)
    if "load" not in i:
        t = data[:,-1]
        temp = []
        for x in t:
            temp.append(float(x))
        Qref[i] = np.array(temp)
        t2 = data[:, -2]
        temp = []
        for x in t2:
            temp.append(float(x))
        Pref[i] = np.array(temp)
        t3 = data[:,0]
        temp = []
        for x in t3:
            temp.append(float(x))
        VAout[i] = np.array(temp)
    else:
        if i not in current.keys():
            current[i] = []
        for x in range(data.shape[1]):
            t = np.array(data[:,x])
            temp = []
            for q in t:
                tt = q[1:]
                delimiter = "+"
                if "-" in tt:
                    delimiter = "-"
                tt = tt.split(delimiter)
                tt = float(tt[0])
                temp.append(tt)
            temp = np.array(temp)
            current[i].append(temp)
        current[i] = np.array(current[i])
        

print(Pref)

colors = ["pink", "blue", "red", "green", "violet", "lightgreen", "orange", "lightblue", "black", "pink", "cyan", "tan", "lightblue", "firebrick", "gold", "magenta", "forestgreen", "coral", "c", "moccasin", "pink", "blue", "red", "green", "violet", "lightgreen", "orange", "lightblue", "black", "pink", "cyan", "tan", "lightblue", "firebrick", "gold","magenta"]


plt.figure(figsize=(12, 8))
count = 0
for i in to_graph.keys():
    x = np.array(range(len(to_graph[i])))
    plt.scatter(x, to_graph[i], s=1, label=i)
plt.axvline(x = 120000, linestyle='--', color = 'red', label = 'Start attack')
plt.xlabel('Timestep (ms)')
plt.ylabel('Frequency (Hz)')
plt.title("Effect of attack on power system frequency")
plt.legend()
plt.tight_layout()
plt.savefig("RD2C-figs/frequency_study.png")

#plt.figure(figsize=(12, 8))
count = 0
for i in current.keys():
    temp = []
    for j in range(len(current[i])):
        for c in range(len(current[i][j])):
            if len(temp) < len(current[i][j]):
                temp.append(current[i][j][c])
            else:
                temp[c] += current[i][j][c]
    plt.figure(figsize=(12,8))
    x = np.array(range(len(temp)))
    plt.scatter(x, temp, label="Sum of current")
    plt.axvline(x = 120000, linestyle='--', color = 'red', label = 'Start attack')
    plt.xlabel('Timestep (ms)')
    plt.ylabel('Current')
    plt.title("Effect of attack on current for inverter #"+str(count))
    plt.legend()
    plt.tight_layout()
    plt.savefig("RD2C-figs/current_study_"+str(count)+".png")
    count += 1

#plt.figure(figsize=(12, 8))
count = 0
keys = list(VAout.keys())
for i in keys:
    print(i)
    nn = "A"
    if "4.B" in i:
        nn = "B"
    plt.figure(figsize=(12,8))
    x = np.array(range(len(VAout[i][1000:])))
    plt.plot(x, VAout[i][1000:], label="Inverter-"+str(count))
    count +=1
    plt.axvline(x = 120000-1000, linestyle='--', color = 'red', label = 'Start attack')
    plt.xlabel('Timestep (ms)')
    #plt.xticks(x, list(range(1000,len(VAout[i]))))
    plt.ylabel('Output volatges (V)')
    plt.title("Effect of attack on the inverter #"+str(count)+" output voltages")
    plt.legend()
    plt.tight_layout()
    plt.savefig("RD2C-figs/VAout_study"+str(count)+"-"+nn+".png")
    

plt.figure(figsize=(12, 8))
count = 0
for i in Qref.keys():
    x = np.array(range(len(Qref[i])))
    plt.scatter(x, Qref[i], label="inverter-"+str(count))
    count += 1
plt.axvline(x = 120000, linestyle='--', color = 'red', label = 'Start attack')
plt.xlabel('Timestep (ms)')
plt.ylabel('Qref (kVAr)')
plt.title("Effect of attack on Qref value over 4 different inverters")
plt.legend()
plt.tight_layout()
plt.savefig("RD2C-figs/Qref_study.png")


plt.figure(figsize=(12, 8))
count = 0
for i in Pref.keys():
        x = np.array(range(len(Pref[i])))
        plt.scatter(x, Pref[i], label="inverter-"+str(count))
        count += 1
plt.axvline(x = 120000, linestyle='--', color = 'red', label = 'Start attack')
plt.xlabel('Timestep (ms)')
plt.ylabel('Pref (V)')
plt.title("Effect of attack on Pref value over 4 different inverters")
plt.legend()
plt.tight_layout()
plt.savefig("RD2C-figs/Pref_study.png")
