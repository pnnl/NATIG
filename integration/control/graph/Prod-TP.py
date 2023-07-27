import numpy as np
import glob

def isfloat(num):
    try:
        float(num)
        return True
    except ValueError:
        return False

file_list = glob.glob("collected-prod/*.txt")
for i in file_list:
    print(i)
    f = open(i,"r")
    data = f.read()
    f.close()
    data = data.split("\n")
    data = np.array(data)
    data = data[:-1]
    t = {}
    for j in range(len(data)): 
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
                xd.append(float(txd)*mul) 
        t[addr].append(xd)
    for j in t.keys():
        av = []
        temp_av = np.array(t[j])
        for d in range(len(t[j][0])):
            av.append(sum(temp_av[:,d])/len(temp_av[:,d]))
        t[j] = np.array(av)
    
    TP_per_path = {}
    for j in t.keys():
        key = j.split("XXXXX")[-1]
        if key not in TP_per_path.keys():
            TP_per_path[key] = []
            #t[j][0] = t[j][0]/float(j.split("XXXXX")[0])
            #t[j][1] = t[j][1]/float(j.split("XXXXX")[0])
        else:
            ttx = np.array(TP_per_path[key])
            t[j][0] = t[j][0]-sum(ttx[:,0])
            t[j][1] = t[j][1]-sum(ttx[:,1])
            t[j][2] = t[j][2]-sum(ttx[:,2])
        TP_per_path[key].append(t[j])
    for j in TP_per_path.keys():
        TP_arr = np.array(TP_per_path[j][1:])
        ss = []
        for x in range(TP_arr.shape[1]):
            div = 1
            mul = 1
            if x < 2:
                div = 0.4
            if x < 1:
                mul = 8
            ss.append(((sum(TP_arr[:,x])/len(TP_arr[:,x]))*mul)/div)
        TP_per_path[j] = np.array(ss)
    print(TP_per_path)
