import numpy as np
import matplotlib.pyplot as plt

def add_reverb(data, reflection_coef):
    # if data.shape[0] > data.shape[1]:
    #     data = np.transpose(data)

    fs = 1000  # sample rate
    room_dim = np.array([4, 4, 2.5])  # dimensions of room (meters)
    mic_pos = np.array([1, 1, 1.6])  # location of mic in room
    src_pos = np.array([2, 2, 1])  # location of sound source in room
    virtual_sources_num = 12  # number of virtual sources: (2*virtual_sources_num+1)^3

    # creating room response
    room_response = rir(fs, mic_pos, virtual_sources_num, reflection_coef, room_dim, src_pos)
    plt.figure(0)
    plt.plot(room_response)
    plt.title('Room response')
    # plt.show()

    # Adding reverb to data
    reverb_data = fconv(data, room_response, 1)  # adding reverb
    reverb_data = reverb_data[:data.shape[0]]

    return reverb_data


def rir(fs, mic, n, r, rm, src):
    nn = np.arange(-n, n+1)
    rms = nn + 0.5 - 0.5 * (-1.0) ** nn

    srcs = (-1.0) ** nn
    xi = srcs * src[0] + rms * rm[0] - mic[0]
    yj = srcs * src[1] + rms * rm[1] - mic[1]
    zk = srcs * src[2] + rms * rm[2] - mic[2]

    i, j, k = np.meshgrid(xi, yj, zk)
    d = np.sqrt(i ** 2 + j ** 2 + k ** 2)
    time = np.round(fs * d / 343) + 1

    e, f, g = np.meshgrid(nn, nn, nn)
    c = r ** (np.abs(e) + np.abs(f) + np.abs(g))
    e = c / d

    h = np.zeros((int(time.max()),))
    for idx, val in enumerate(time.flatten()):
        h[int(val)-1] += e.flatten()[idx]

    return h


def fconv(x, h, normalize=True):
    Ly = len(x) + len(h) - 1
    Ly2 = 2 ** (np.ceil(np.log2(Ly))).astype(int)
    X = np.fft.fft(x, Ly2)
    H = np.fft.fft(h, Ly2)
    Y = X * H
    y = np.fft.ifft(Y, Ly2).real
    y = y[:Ly]

    if normalize:
        y /= np.max(np.abs(y))

    return y
