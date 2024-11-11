import sys
f = open(sys.argv[1]+".glm", "r")
data = f.read()
f.close()
data = data.split("\n")
data = data[:-1]

f = open(sys.argv[1]+"edited.glm", "a")
f.write(data[0]+"\n")
for i in range(1,len(data)):
    if "node" in data[i-1] or "load" in data[i-1]: # or "diesel_dg" in data[i-1]: # or "object inverter" in data[i-1]: # or "object solar" in data[i-1]:
        f.write(data[i]+ "\n")
        if "DELTAMODE" not in data[i+1]:
            f.write("        flags DELTAMODE;\n")
        if "frequency_measure_type" not in data[i+2]:
            f.write("        frequency_measure_type PLL;\n")
    elif "object solar" in data[i-1] or "object inverter" in data[i-1] or "diesel_dg" in data[i-1]:
        f.write(data[i]+ "\n")
        if "DELTAMODE" not in data[i+1]:
            f.write("        flags DELTAMODE;\n")
    elif "power_1" not in data[i-1] and "power_1" not in data[i]:
        f.write(data[i]+"\n")
f.close()
