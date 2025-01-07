import numpy as np
from matplotlib import pyplot as plt
from numpy import genfromtxt
import sys

if len(sys.argv) < 2:
    print("file arg?")
    exit(1)

wave = genfromtxt(sys.argv[1])

plt.plot(wave)
plt.show()
