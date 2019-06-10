import sys
import os

arguments=sys.argv
outFile ='sounds.h'
with open(outFile,'w') as out:
    out.write('#include <pgmspace.h>\n\n');
    for i in range(1, len(arguments)):
        fileName=arguments[i] 
        len = os.stat(fileName).st_size
        if (len < 65536):
            print ('Now processing file '+fileName+' with length '+str(len))
            raw = open(fileName, 'rb').read()
            ascii = [ord(c) for c in raw]
            arrayName = fileName.partition(".")[0]
            cnt=0

            out.write('const uint8_t ')
            out.write(arrayName)
            out.write('[] PROGMEM={');
            lowbyte= len & 0xff
            highbyte = (len & 0xff00) >> 8
            out.write(str(highbyte)+' , '+str(lowbyte)+' , ');

            for row in ascii:
                out.write('{0},'.format(row))
                cnt=cnt+1
                if (cnt==50):
                    out.write('\n')
                    cnt=0
            out.write('127};')
            out.write('\n\n')
    out.close()