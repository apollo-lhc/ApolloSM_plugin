from matplotlib import figure
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import sys
import datetime

if(3 != len(sys.argv)):
    print("Wrong number of arguments")
    sys.exit()

file = sys.argv[1]
title = sys.argv[2]

# get data
data = np.genfromtxt(file,delimiter=' ')

print("parsing file")

# load data in
x=data[:,0]
y=data[:,1]
z=data[:,2]

print("parsing file done")

#print(z)

# For the zeros. Anything zero is registered as the lowest BER we currently support
#precision = 0.000001 # 10^-9
#for i in range(len(z)):
#    if(z[i] < precision):
#        z[i] = precision

# get rid of copies (ie I don't want -127 V 20 differnet times)
x=np.unique(x)
y=np.unique(y)

# meshgrid
X, Y = np.meshgrid(x,y,sparse=True)
# reshape z some kind of square
Z = z.reshape(len(y), len(x))
z_min, z_max = -abs(z).max(), abs(z).max()

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

#pcm = ax.pcolor(X, Y, Z, norm=mpl.colors.LogNorm(vmin=abs(Z.min()), vmax=Z.max()), cmap='jet', shading='auto')
pcm = ax.pcolor(X,Y,Z, cmap='jet', vmin=0, vmax=z_max, shading="auto")
# display color bar
fig.colorbar(pcm, ax=ax) 

print("Saving now")

# -4 because we don't want '.txt'

fig.savefig(file[:-4] + '.png')

print("Saved")
