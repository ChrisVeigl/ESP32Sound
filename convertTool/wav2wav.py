#
#  convert .wav files to 16KHz, mono, 8bit format
#  usage: python wav2wav.py file1.wav file2.wav ...
#  the converter will create file1_conv.wav, file2_conv.wav, ...
#
#  thanks to:
#  https://stackoverflow.com/questions/30619740/python-downsampling-wav-audio-file
#


import sys
import os
import wave
import audioop


arguments=sys.argv

for i in range(1, len(arguments)):
    fileName=arguments[i] 
    length = os.stat(fileName).st_size
    print ('Now processing file '+fileName+' with length '+str(length))
    try:
        s_read = wave.open(fileName, 'rb')
        newName = fileName.partition(".")[0]
        newName=newName+'_conv.wav'
        s_write = wave.open(newName, 'wb')
    except:
        print 'Failed to open file!'
        quit()

    n_frames = s_read.getnframes()
    data = s_read.readframes(n_frames)
    inchannels=s_read.getnchannels()
    bytes=s_read.getsampwidth()
    inrate=s_read.getframerate()
    print ('File has '+str(inchannels)+' channels,'+str(bytes)+' bytes per sample and rate '+str(inrate))

    try:
        print ('converting to 16KHz!')
        converted = audioop.ratecv(data, bytes, inchannels, inrate, 16000, None)
        if (inchannels == 2):
            print ('converting to mono!')
            converted = audioop.tomono(converted[0], bytes, 1, 0)
        if (bytes > 1):
            print ('converting to 8-bit representation!')
            converted = audioop.lin2lin(converted, bytes, 1)
            converted = audioop.bias(converted, 1, 128)
    except:
        print 'Failed to downsample wav'

    try:
        s_write.setparams((1, 1, 16000, 0, 'NONE', 'Uncompressed'))
        s_write.writeframes(converted)
    except:
        print 'Failed to write wav'
        quit()

    try:
        s_read.close()
        s_write.close()
    except:
        print 'Failed to close wav file'
