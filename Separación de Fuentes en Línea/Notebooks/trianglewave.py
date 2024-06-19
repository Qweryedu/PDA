import numpy as np

def trianglewave(freq, samplerate):
    x = np.arange(1, samplerate + 1)
    wavelength = samplerate / freq
    saw = 2 * (x % wavelength) / wavelength - 1  # Start with sawtooth wave
    triwave = 2 * np.abs(saw) - 1
    return triwave