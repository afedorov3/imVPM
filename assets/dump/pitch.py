import numpy as np
from numpy.fft import rfft
from numpy import asarray, argmax, mean, diff, log, copy
from scipy.signal import correlate, decimate
from scipy.signal.windows import kaiser, hann
from numpy import genfromtxt

def parabolic(f, x):
    """
    Quadratic interpolation for estimating the true position of an
    inter-sample maximum when nearby samples are known.

    f is a vector and x is an index for that vector.

    Returns (vx, vy), the coordinates of the vertex of a parabola that goes
    through point x and its two neighbors.

    Example:
    Defining a vector f with a local maximum at index 3 (= 6), find local
    maximum if points 2, 3, and 4 actually defined a parabola.

    In [3]: f = [2, 3, 1, 6, 4, 2, 3, 1]

    In [4]: parabolic(f, argmax(f))
    Out[4]: (3.2142857142857144, 6.1607142857142856)
    """
    if int(x) != x:
        raise ValueError('x must be an integer sample index')
    else:
        x = int(x)
    xv = 1/2. * (f[x-1] - f[x+1]) / (f[x-1] - 2 * f[x] + f[x+1]) + x
    yv = f[x] - 1/4. * (f[x-1] - f[x+1]) * (xv - x)
    return (xv, yv)

def find(condition):
    "Return the indices where ravel(condition) is true"
    res, = np.nonzero(np.ravel(condition))
    return res

def freq_from_crossings(signal, fs, interp='linear'):
    """
    Estimate frequency by counting zero crossings

    Works well for long low-noise sines, square, triangle, etc.

    Pros: Fast, accurate (increasing with signal length).

    Cons: Doesn't work if there are multiple zero crossings per cycle,
    low-frequency baseline shift, noise, inharmonicity, etc.
    """
    signal = asarray(signal) + 0.0

    # Find all indices right before a rising-edge zero crossing
    indices = find((signal[1:] >= 0) & (signal[:-1] < 0))

    if interp == 'linear':
        # More accurate, using linear interpolation to find intersample
        # zero-crossings (Measures 1000.000129 Hz for 1000 Hz, for instance)
        crossings = [i - signal[i] / (signal[i+1] - signal[i])
                     for i in indices]
    elif interp == 'none' or interp is None:
        # Naive (Measures 1000.185 Hz for 1000 Hz, for instance)
        crossings = indices
    else:
        raise ValueError('Interpolation method not understood')

        # TODO: Some other interpolation based on neighboring points might be
        # better.  Spline, cubic, whatever  Can pass it a function?

    return fs / mean(diff(crossings))

def freq_from_fft(signal, fs):
    """
    Estimate frequency from peak of FFT

    Pros: Accurate, usually even more so than zero crossing counter
    (1000.000004 Hz for 1000 Hz, for instance).  Due to parabolic
    interpolation being a very good fit for windowed log FFT peaks?
    https://ccrma.stanford.edu/~jos/sasp/Quadratic_Interpolation_Spectral_Peaks.html
    Accuracy also increases with signal length

    Cons: Doesn't find the right value if harmonics are stronger than
    fundamental, which is common.
    """
    signal = asarray(signal)

    N = len(signal)

    # Compute Fourier transform of windowed signal
    windowed = signal * kaiser(N, 100)
    f = rfft(windowed)

    # Find the peak and interpolate to get a more accurate peak
    i_peak = argmax(abs(f))  # Just use this value for less-accurate result
    i_interp = parabolic(log(abs(f)), i_peak)[0]

    # Convert to equivalent frequency
    return fs * i_interp / N  # Hz

def freq_from_autocorr(signal, fs):
    """
    Estimate frequency using autocorrelation

    Pros: Best method for finding the true fundamental of any repeating wave,
    even with strong harmonics or completely missing fundamental

    Cons: Not as accurate, doesn't find fundamental for inharmonic things like
    musical instruments, this implementation has trouble with finding the true
    peak
    """
    signal = asarray(signal) + 0.0

    # Calculate autocorrelation, and throw away the negative lags
    signal -= mean(signal)  # Remove DC offset
    corr = correlate(signal, signal, mode='full')
    corr = corr[len(corr)//2:]

    # Find the first valley in the autocorrelation
    d = diff(corr)
    start = find(d > 0)[0]

    # Find the next peak after the low point (other than 0 lag).  This bit is
    # not reliable for long signals, due to the desired peak occurring between
    # samples, and other peaks appearing higher.
    i_peak = argmax(corr[start:]) + start
    i_interp = parabolic(corr, i_peak)[0]

    return fs / i_interp


def freq_from_hps(signal, fs):
    """
    Estimate frequency using harmonic product spectrum

    Low frequency noise piles up and overwhelms the desired peaks

    Doesn't work well if signal doesn't have harmonics
    """
    signal = asarray(signal) + 0.0

    N = len(signal)
    signal -= mean(signal)  # Remove DC offset

    # Compute Fourier transform of windowed signal
    windowed = signal * kaiser(N, 100)

    # Get spectrum
    X = log(abs(rfft(windowed)))

    # Remove mean of spectrum (so sum is not increasingly offset
    # only in overlap region)
    X -= mean(X)

    # Downsample sum logs of spectra instead of multiplying
    hps = copy(X)
    for h in range(2, 9):  # TODO: choose a smarter upper limit
        dec = decimate(X, h, zero_phase=True)
        hps[:len(dec)] += dec

    # Find the peak and interpolate to get a more accurate peak
    i_peak = argmax(hps[:len(dec)])
    i_interp = parabolic(hps, i_peak)[0]

    # Convert to equivalent frequency
    return fs * i_interp / N  # Hz

def find_freq(wave, fs):
    i = 0
    max1 = 0
    max2 = 0
    max1_index = 0
    max2_index = 0
    min1 = 0
    min2 = 0
    min1_index = 0
    min2_index = 0
    foundperiod = 0
    samples = 0

    while i < wave.size and foundperiod == 0:
        current_element = wave[i]
        nextel = wave[i+1]
        if current_element < nextel and min1 == 0:
            min1 = 1
            min1_index = i
        if current_element > nextel and max1 == 0:
            max1 = 1
            max1_index = i
        if current_element < nextel and min1 == 1:
            min2 = 1
            min2_index = i
        if current_element > nextel and max1 == 1:
            max1 = 1
            max1_index = i
        i+=1
        if (max1 == 1 and max2 == 1 and min1 == 1) or (min1 == 1 and max1 == 1 and min2 == 1):
            foundperiod = 1
    if max1 == 1 and max2 == 1 and min1 == 1:
        samples = max2_index - max1_index
    elif min1 == 1 and max1 == 1 and min2 == 1:
        samples = min2_index - min1_index

    print(samples)
    #Irrelevant part
    cycles = 3
    frequency = 1 / (samples*(fs/cycles))
    return frequency

wave = genfromtxt("acf.txt")
#print("find_freq:", find_freq(wave, 44100))
print("cross:", freq_from_crossings(wave, 44100))

wave = genfromtxt("wave.txt")
print("fft:", freq_from_fft(wave, 44100))
print("acf:", freq_from_autocorr(wave, 44100))
print("hps:", freq_from_hps(wave, 44100))
