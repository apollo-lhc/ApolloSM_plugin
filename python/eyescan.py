from matplotlib import figure
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import sys
import datetime
import math

if(5 != len(sys.argv)):
    print("Wrong number of arguments")
    print("<file> <title> <prescale> <bus_width>")
    sys.exit()

file = sys.argv[1]
title = sys.argv[2]
prescale = int(sys.argv[3])
bus_width = int(sys.argv[4])

# get data
data = np.genfromtxt(file,delimiter=' ')

print("parsing file")

# load data in
x=data[:,0]
y=data[:,1]
z=data[:,2]

print("parsing file done")

#print(z)
# count=65535*2**(prescale+1)*bus_width
# precision=math.log(.005)/(-count)
# # For the zeros. Anything zero is registered as the lowest BER we currently support
# print(count)
# print(precision)
# for i in range(len(z)):
#    if(z[i] < precision):
#        z[i] = precision

# get rid of copies (ie I don't want -127 V 20 differnet times)
x=np.unique(x)
y=np.unique(y)

# meshgrid
X, Y = np.meshgrid(x,y,sparse=True)
# reshape z some kind of square
Z = z.reshape(len(y), len(x))
z_min, z_max = abs(Z).min(), abs(Z).max()
# print(Z)
# print(z_min)
# print(z_max)
print("meshgrid and reshape done")

#########################
# plotting

fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_xlabel('phase offset (UI fraction)')
ax.set_ylabel('voltage offset (unknown units)')
ax.set_title(title)

# log norm to set color bar scaling
# cmap to set red-blue color scheme
pcm = ax.pcolor(X, Y, Z, norm=mpl.colors.LogNorm(vmin=z_min, vmax=z_max), cmap='jet')
#pcm = ax.pcolor(X, Y, Z,vmin=0, vmax=z_max, cmap='jet')
# display color bar
fig.colorbar(pcm, ax=ax, label="BER") 

print("Saving now")

# -4 because we don't want '.txt'
fig.savefig(file[:-4] + '.png')

print("saved")
