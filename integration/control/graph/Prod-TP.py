import numpy as np
import glob
import statistics
import matplotlib.pyplot as plt
import math
plt.rcParams.update({'font.size': 11})

def isfloat(num):
    try:
        float(num)
        return True
    except ValueError:
        return False

file_list = glob.glob("collected-prod/*.txt")
TP_dict = {}
all_ID = []
for i in file_list:
    print(i)
    f = open(i,"r")
    data = f.read()
    f.close()
    data = data.split("\n")
    data = np.array(data)
    data = data[:-1]
    t = {}
    for j in range(4224): #int(len(data))): 
        temp = data[j].split(" (")
        temp2 = data[j].split(") ")
        tt = temp2[-1]
        addr = temp[-1].split(") ")
        timed = temp[0].split(" ")
        addr = timed[0] +"XXXXX"+ addr[0]
        if addr not in t.keys():
            t[addr] = []
        te = tt.split(" ")
        xd = []
        for xw in te:
            if isfloat(xw):
                xd.append(float(xw))
            else:
                txd = xw.split("e+")
                mul = 1
                if len(txd)>1:
                    mul = pow(10,float(txd[1].replace("ns", "")))
                txd = txd[0][1:]
                txd = txd.replace("ns", "")
                xd.append(float(txd)*mul) #/xd[0]) 
        t[addr].append(xd)
    count = 1
    for j in t.keys():
        av = []
        temp_av = np.array(t[j])
        for d in range(len(t[j][0])):
            if d == 2:
                av.append(sum(temp_av[:,d])/sum(temp_av[:,0]))
            else:
                av.append(sum(temp_av[:,d])) #/len(temp_av[:,d]))
        t[j] = np.array(av)
    TP_per_path = {}
    for j in t.keys():
        key = j.split("XXXXX")[-1]
        if key not in TP_per_path.keys():
            TP_per_path[key] = []
        else:
            ttx = np.array(TP_per_path[key])
            #t[j][0] = t[j][0]-(ttx[-1,0])
            #t[j][1] = t[j][1]-sum(ttx[:,1])
            #t[j][2] = t[j][2]-sum(ttx[:,2])
        TP_per_path[key].append(t[j])
    for j in TP_per_path.keys():
        TP_arr = np.array(TP_per_path[j]) #[1:])
        ss = []
        #int(len(TP_arr)/2)
        for x in range(TP_arr.shape[1]):
            div = 1
            if x == 0:
                div = 0.4
            ss.append((sum(TP_arr[:,x])/len(TP_arr[:,x]))/div) #/div) #((sum(TP_arr[:,x])/len(TP_arr[:,x]))*mul)/div)
            #ss.append((statistics.stdev(TP_arr[:int(len(TP_arr[:,x])/2),x])*mul)/div)
        TP_per_path[j] = np.array(ss)
    keys = sorted(list(TP_per_path.keys()))
    for j in keys:
        if i not in TP_dict.keys():
            TP_dict[i] = {}
        ID = ""
        if "UDP 1." in j:
            tt = j.split("-->")
            p = tt[0].split("/ ")
            p = p[-1]
            tt = tt[-1].split(".")
            ID = "CC --> MG"+str(int(float(tt[1])-17)) 
            #if ID not in all_ID:
            #    all_ID.append(ID)
        elif "UDP 172." in j:
            tt = j.split(".")
            p = j.split("-->")
            p = p[1].split("/ ")
            ID = "MG"+str(int(float(tt[1])-17))+" --> CC"
            if ID not in all_ID:
                all_ID.append(ID)
            TP_dict[i][ID] = TP_per_path[j][0] #/(TP_per_path[j][-1]/1000000000)
print(TP_dict)
tt = {}
for i in TP_dict.keys():
    t = []
    for j in all_ID:
        t.append(TP_dict[i][j])
    i = i.replace("DDoS", "")
    tt[i] = t
print("===========================")
TP_dict = tt
print(TP_dict)

x = np.arange(len(all_ID))  # the label locations
width = 0.1  # the width of the bars
multiplier = 0
fig, ax = plt.subplots()
kk = sorted(list(TP_dict.keys()))
kk = kk[::-1]
#t = kk[1]
#kk[1] = kk[2]
#kk[2] = t
#t = kk[-1]
#kk[-1] = kk[-2]
#kk[-2] = t
for s in kk:
    offset = width * multiplier
    lab = s.replace(".txt", "")
    lab = lab.split("/")[-1]
    add = ""
    if "Baseline" not in lab:
        add = "DDoS"
    rects = ax.bar(x + offset, TP_dict[s], width=width, label=add+lab)
    multiplier += 1

ax.set_ylabel("Throughput (B/s)") #'Delay per bytes (ns/B)')
ax.set_title('Path measured')
ax.set_xticks(x + width)
ax.set_xticklabels(all_ID, rotation=20)
ax.legend(loc='upper left') #, ncols=3)
plt.tight_layout()
plt.savefig('TP-Prob.png')
