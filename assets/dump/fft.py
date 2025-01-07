import numpy as np
from numpy import genfromtxt

wave = genfromtxt('wave.txt')
fft = np.fft.fft(wave)
freq = np.fft.fftfreq(n=wave.size, d=1/44100)
np.savetxt("fft.csv", np.column_stack((freq.real, np.abs(fft).real)))
