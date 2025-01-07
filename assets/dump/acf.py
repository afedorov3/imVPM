from numpy.fft import fft,ifft
import numpy as np
from numpy import genfromtxt
#from matplotlib import pyplot as plt
#import sys

def calculate_acf(x):
    """Calculate the autocorrelation function using fast fourier transform"""
    N = float(len(x))
    pow2 = int(2**np.ceil(np.log2(len(x))))
    x_new = np.zeros(pow2,float)
    x_new[:len(x)] = x
    FT = fft(x_new)
    acf = (ifft(FT*np.conjugate(FT)).real)/N
    acf /= acf.max()
    acf = acf[:int(acf.size/2)]
    return acf

#if len(sys.argv) < 2:
#    print("file arg?")
#    exit(1)

#wave = genfromtxt(sys.argv[1])
wave = genfromtxt("acf.txt")
acf = calculate_acf(wave)
np.savetxt("acf.csv", acf)

#plt.plot(acf)
#plt.show()
