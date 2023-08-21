import numpy as np
import sys
import matplotlib.pyplot as plt
import math
import statistics
plt.rcParams.update({'font.size': 11})

num = int(sys.argv[1])

UE = []
GnB = []
yvalue = 0
xValue = 0.0
for i in range(1,num+1):
    if i%2:
        yvalue = i*5
    else:
        yvalue = int(yvalue*1.5)
    GnB.append([0, yvalue])
    for j in range(i, i+1):
        if j % 2 != 0:
            xValue = j
        else:
            xValue = -xValue
        if yvalue % 2 == 0:
            UE.append([xValue, 10])
        else:
            UE.append([xValue, 0.0])

UE = np.array(UE)
GnB = np.array(GnB)

colors = ["pink", "blue", "red", "green", "violet", "lightgreen", "orange", "lightblue", "black", "pink", "cyan", "tan", "lightblue", "firebrick", "gold", "magenta", "forestgreen", "coral", "c", "moccasin", "pink", "blue", "red", "green", "violet", "lightgreen", "orange", "lightblue", "black", "pink", "cyan", "tan", "lightblue", "firebrick", "gold","magenta"]
fig, ax = plt.subplots()
for i in range(len(UE)):
    ax.plot([UE[i,0]], [UE[i,1]], 'o', color=colors[i])
    ax.plot([GnB[i,0]], [GnB[i,1]], 'o', color=colors[i])
    plt.annotate('UE%s(%s, %s)' % (i,UE[i,0], UE[i,1]), xy=(UE[i,0], UE[i,1]), textcoords='offset points', xytext=(0,10), ha='center')
    plt.annotate('GnB%s(%s, %s)' % (i,GnB[i,0], GnB[i,1]), xy=(GnB[i,0], GnB[i,1]), textcoords='offset points', xytext=(0,10), ha='center')
#for i in range(len(UE)):
#    plt.quiver(UE[i,1], UE[i,0], GnB[i,1], GnB[i,0], color=colors[i] ,units='xy', scale=1, label="UE"+str(i)+" to GnB"+str(i))


#plt.xlim(-150, 150)

plt.xlim(-4.5, 4.5)
plt.ylim(-110, 110)

plt.title('Map of UE and GnB coordinates')
#plt.legend(loc='upper left')
plt.tight_layout()
plt.savefig('LOC.png')
print(UE)
print(GnB)
