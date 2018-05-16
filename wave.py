#!/usr/bin/env python

import wave
import pylab as pl
import numpy as np

#open the wav file
f = wave.open('card.wav', 'rb')

#read the file
#(nchannels, sampwidth, framerate, nframes = params[:4])
params = f.getparams()
nchannels, sampwidth, framerate, nframes = params[:4]

#read the wave
str_data = f.readframes(nframes)
f.close()

#transform the wave into list
wave_data = np.fromstring(str_data, dtype = np.short)
wave_data.shape = -1, 2
wave_data = wave_data.T
time = np.arange(0, nframes) * (1.0 / framerate)

#plot
pl.subplot(211)
pl.plot(time, wave_data[0])
pl.subplot(212)
pl.plot(time, wave_data[1], c = 'g')
pl.xlabel('time(secondes)')
pl.show()