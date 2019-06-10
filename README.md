# ESP32Sound
Arduino library for audio playback on ESP32 / ODROID-GO

This library provides sound capabilites for the ODROID-GO. 
It supports playback of large sound files from SD card (background music)
and concurrent playback of short sound effects (FX) from flash memory.  
The volume of background music and FX can be set separately.
The sound playback works "in background" so that the main loop can be
used for other things (LCD, buttons, WiFi etc.) 

###Preparation/placement of sound files  
The sound files must be provided in 8-bit, mono, raw format. The sampling rate
can be chosen on demand, 16000 is recommended (more means more CPU-load). 
A nice audio tool which can be used to create .raw format from .mp3 or .wav is Audacity. 
For instructions see [here](https://www.hellomico.com/getting-started/convert-audio-to-raw/)

The background music files can be placed on the SD card, the file path is given to the
playSound function as an argument (with a leading slash), eg. *playSound(SD, "/myfile.raw")*

The FX files are stored in flash memory in a C-array. Only small files should be used.
A python tool to convert .raw files into C-arrays is provided (raw2array.py, folder convertTool). 
This tool creates a header file *sounds.h* from one or more .raw files, e.g.:  
***python raw2array.py sound1.raw sound2.raw sound3.raw***  
Please note that the maximum size for FX raw files is 65535 bytes.
After including *sounds.h* into your sketch, you can use *playFX(sound1)*, *playFX(sound2)*, ...

### Implementation infos  
The current version of the library uses a timer ISR (called with sampling rate) 
for feeding the sound-samples into the DAC of the ODROID-GO. A dedicated task refills the play buffer
with sound data from the SD card. The play buffer is spilt in two parts, so that the timer ISR reads from
the part which is not currently refilled. This enables a continuous playback without breaks / pops. 
Future improvements might use DMA/I2S to increase performance.

This code is released under GPLv3 license.
see: https://www.gnu.org/licenses/gpl-3.0.en.html and https://www.fsf.org/
