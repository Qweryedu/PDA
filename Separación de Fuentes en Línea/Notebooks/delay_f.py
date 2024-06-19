import numpy as np

def delay_f(x, time, fs):
    x_f = np.fft.fft(x)
    N = len(x_f)

    w = np.concatenate(([0], np.arange(1, N//2+1), np.arange(-N//2+1, 0))) / N * fs

    y_f = np.zeros_like(x_f, dtype=complex)
    for f in range(N):
        e = np.exp(-1j * 2 * np.pi * w[f] * time)
        y_f[f] = x_f[f] * e

    y = np.real(np.fft.ifft(y_f))

    return y
