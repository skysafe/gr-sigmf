import math


def sig_source_c(samp_rate, freq, amp, N):
    t = map(lambda x: float(x) / samp_rate, xrange(N))
    y = map(lambda x: amp * math.cos(2. * math.pi * freq * x) +
            1j * amp * math.sin(2. * math.pi * freq * x), t)
    return y
