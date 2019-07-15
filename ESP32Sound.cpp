//
//  ESP32Sound library for ODROID-GO
//  Version 1.0, 2019-06-10
//
//  This library provides sound capabilites for the ODROID-GO. 
//  It supports playback of large sound files from SD card (background music)
//  and concurrent playback of short sound effects (FX) from flash memory.  
//  The volume of background music and FX can be set separately.
//  The sound playback works "in background" so that the main loop can be
//  used for other things (LCD, buttons, WiFi etc.) 
//  Please note that the sampling rate of background music and FX files must be the same.
//  Although playSound() can handle different .wav files (eg. 44,1Khz, 16bit, stereo), 
//  the format 16Khz, mono, 8 bit is recommended (low-bandwidth, low CPU-load).
//  The provided python scripts can be used to convert arbitrary .wav files to that format.
//
//  Preparation/placement of sound files:
//  The sound files must be provided in .wav format.  
//  The background music files can be placed on the SD card, the file path is given to the
//  playSound() function as an argument with a leading slash, eg. playSound(SD, "/myfile.wav");
//  The FX files are stored in flash memory as a C-array. Only small files should be used.
//  The python script wav2array.py converts .wav files into C-arrays, it creates the header file 
//  "sounds.h" from one or more .wav files, eg: python raw2array.py sound1.wav sound2.wav sound3.wav
//  After including "sounds.h" into your sketch, you can use playFX(sound1), playFX(sound2)...
//  The python script wav2wav.py converts .wav files to 16Khz, mono, 8 bit format.
//
//  The current version of the library uses a timer ISR (called with sampling rate) 
//  for feeding the sound-samples into the DAC of the ODROID-GO. 
//  Future improvements might use DMA/I2S to increase performance and sound quality.
//
//  This code is released under GPLv3 license.
//  see: https://www.gnu.org/licenses/gpl-3.0.en.html and https://www.fsf.org/

#include <Arduino.h>
#include "ESP32Sound.h"

// definition/initialisation of the static class members 
// (the class is a static/singleton!)
hw_timer_t *      ESP32Sound_Class::timer = NULL;
QueueHandle_t     ESP32Sound_Class::xQueue = NULL;
portMUX_TYPE      ESP32Sound_Class::mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t      ESP32Sound_Class::xHandle = NULL;
File              ESP32Sound_Class::soundFile;
volatile uint8_t  ESP32Sound_Class::soundVolume = DEFAULT_SOUND_VOLUME;
volatile uint8_t  ESP32Sound_Class::fxVolume = DEFAULT_FX_VOLUME;
uint16_t          ESP32Sound_Class::bufsize;
uint16_t          ESP32Sound_Class::chunksize=DEFAULT_CHUNK_SIZE;
volatile uint32_t ESP32Sound_Class::sampleCounter;
volatile uint32_t ESP32Sound_Class::lastSample;
const uint8_t *   ESP32Sound_Class::playFXLoc = NULL;
volatile uint32_t ESP32Sound_Class::playFXLen = 0;
volatile uint8_t  ESP32Sound_Class::playStream = 0;
uint8_t           ESP32Sound_Class::peak=0;
uint8_t           ESP32Sound_Class::verbosity=1;
uint16_t          ESP32Sound_Class::channels=1;
uint16_t          ESP32Sound_Class::bits=8;
uint32_t          ESP32Sound_Class::samplingRate=16000;
uint32_t          ESP32Sound_Class::dataStart=0;
uint32_t          ESP32Sound_Class::dataSize=0;


void IRAM_ATTR ESP32Sound_Class::soundTimer(){
  uint8_t dacValue=127;
  uint8_t currentAmplitude;
  static uint8_t peakDecayCount=0;

  // mix FX samples
  if (playFXLen) {
    dacValue+=(*playFXLoc-127)*fxVolume/100;
    playFXLoc++;
    playFXLen--;
  } 

  // mix streaming samples
  if (playStream) {
    uint8_t cRxedChar;
    xQueueReceiveFromISR( xQueue,( void * ) &cRxedChar, NULL);
    dacValue+=(cRxedChar-127)*soundVolume/100;
    portENTER_CRITICAL_ISR(&mux);
    sampleCounter++;
    if (sampleCounter >= lastSample) {
      playStream=0;
    }
    portEXIT_CRITICAL_ISR(&mux);
  }
  dacWrite(DAC_PIN, dacValue);

  // update peak value
  currentAmplitude= dacValue>127 ? dacValue-127 : 127-dacValue;
  if (peak < currentAmplitude) peak=currentAmplitude; 
  peakDecayCount++;
  if (peakDecayCount==PEAKDECAY_INTERVAL) {
    peakDecayCount=0;
    if (peak>0) peak--;
  }

  // check if we finished playing
  if ((!playFXLen) && (!playStream) && (!peak)) {
    dacWrite(DAC_PIN, 127);
    timerAlarmDisable(timer); 
  }
}

void ESP32Sound_Class::begin(uint32_t samplingrate, uint16_t soundbufSize)  {

    pinMode(AMP_PIN, OUTPUT);
    digitalWrite(AMP_PIN, HIGH);
    dacWrite(DAC_PIN, 127);
    xQueue = xQueueCreate( soundbufSize, 1 );
    if (verbosity) Serial.printf("Init sound: samplingrate=%d, soundBufsize=%d\n",samplingrate,soundbufSize);
    bufsize=soundbufSize;
    timer = timerBegin(0, 80, true);   // prescaler 80 : 1MHz
    timerAlarmDisable(timer);
    timerAttachInterrupt(timer, &soundTimer, true);
    setPlaybackRate(samplingrate);
}

void ESP32Sound_Class::playSound(fs::FS &fs, const char * path){
    if (isPlaying()) {
      if (verbosity) Serial.printf("sound already playing!\n");
      return;
    }
 
    setPlaying(1);
    soundFile = fs.open(path);   
    if(soundFile){
        if (verbosity) Serial.printf("open file %s successful!\n", path);
        if (getWavHeader()) {
            if (verbosity) Serial.println("Wav file opened.");
            setPlaybackRate(samplingRate);
        } else {
            if (verbosity) Serial.println("Wav format not recognized, assuming raw 8-bit 16Khz format.");
            channels=1;
            bits=8;
            dataStart=0;
            dataSize=soundFile.size();
        }
        xTaskCreate(  soundStreamTask,  /* Task function. */
                      "sst1",           /* String with name of task. */
                      4000,             /* Stack size in bytes. */
                      NULL,             /* Parameter passed as input of the task */
                      1,                /* Priority of the task. */
                      &xHandle);        /* Task handle. */                    
    } 
    else if (verbosity) {
        Serial.println("Failed to open file for reading");
        setPlaying(0);             
    }
}

boolean ESP32Sound_Class::isPlaying(){
    uint8_t tmp;
    portENTER_CRITICAL(&mux);             
    tmp=playStream;
    portEXIT_CRITICAL(&mux);
    if (tmp) return(true);
    return(false); 
}

uint8_t ESP32Sound_Class::getPeak(){
  uint8_t actPeak;
  portENTER_CRITICAL(&mux);             
  actPeak=peak;
  portEXIT_CRITICAL(&mux);
  return(actPeak); 
}

void ESP32Sound_Class::stopSound(){
  if (verbosity) Serial.println("Stop sound.");
  if (xHandle!=NULL) {
    setPlaying(0);             
    soundFile.close();
    vTaskDelete( xHandle );
    xHandle=NULL;
    if (verbosity) Serial.println("SoundstreamTask closed.");
  }
}

void ESP32Sound_Class::playFx(const uint8_t * fxBuf){
    portENTER_CRITICAL(&mux);             
    playFXLoc=fxBuf+4;
    // the length is stored in the first 4 bytes (low byte first)
    playFXLen= ( ((uint32_t) fxBuf[0]) + ((uint32_t) fxBuf[1]<<8) + ((uint32_t) fxBuf[1]<<16) +((uint32_t) fxBuf[1]<<24));
    portEXIT_CRITICAL(&mux);               
    timerAlarmEnable(timer); // in case timer is currently not running  
}

void ESP32Sound_Class::setPlaybackRate(uint32_t pr){
    timerAlarmWrite(timer, 1000000/pr, true);
}

void ESP32Sound_Class::setFxVolume(uint8_t vol){
    portENTER_CRITICAL(&mux);             
    fxVolume=vol;
    portEXIT_CRITICAL(&mux);             
}

void ESP32Sound_Class::setSoundVolume(uint8_t vol){
    portENTER_CRITICAL(&mux);             
    soundVolume=vol;
    portEXIT_CRITICAL(&mux);             
}

void ESP32Sound_Class::setPlaying(uint8_t p) {
    portENTER_CRITICAL(&mux);             
    playStream=p;
    portEXIT_CRITICAL(&mux);             
}

void ESP32Sound_Class::setVerbosity(uint8_t v){
    portENTER_CRITICAL(&mux);             
    verbosity=v;
    portEXIT_CRITICAL(&mux);             
}



#define RIFF_ID "RIFF"
#define WAVE_ID "WAVE"

#define DATA_CHUNK_ID "data"
#define FMT_CHUNK_ID "fmt "

#define GET_LE_LONGWORD(bfr, ofs) (bfr[ofs+3] << 24 | bfr[ofs+2] << 16 |bfr[ofs+1] << 8 |bfr[ofs+0])
#define GET_LE_SHORTWORD(bfr, ofs) (bfr[ofs+1] << 8 | bfr[ofs+0])

#define WAV_HEADER_BUF 0x30


uint8_t ESP32Sound_Class::getWavHeader(){
    uint8_t headerData[WAV_HEADER_BUF];
    uint32_t offset,size, chunkSize;
    
    if (soundFile.read(headerData,0x0c) != 0x0c) {
       if (verbosity)  Serial.printf("SD read error: wav header not read\n");
       return(0);
    }
    if (memcmp(headerData,RIFF_ID,4) || memcmp(headerData,RIFF_ID,4))  
       return(0);
    
    size = GET_LE_LONGWORD(headerData, 4);
    offset=0x0c;
    
    while (offset<size) {

        if (soundFile.read(headerData,8) != 8) {
           Serial.printf("SD read error: wav header not read\n");
           return(0);
        }
        
        if (!memcmp(headerData, DATA_CHUNK_ID,4)) {
            dataSize = GET_LE_LONGWORD(headerData, 0x04);
            dataStart = offset+8;
            if (verbosity) Serial.printf("Wav file detected, Samplerate=%d, channels=%d, bits=%d, size=%d\n", samplingRate,channels,bits,dataSize);
            return(1);
        }

        else {
            chunkSize=GET_LE_LONGWORD(headerData,0x04);
            if (chunkSize>=WAV_HEADER_BUF-8) {
                Serial.printf("SD read error: header chunk size too big!\n");
                return(0);
            }
            if (soundFile.read(headerData+8,chunkSize) != chunkSize) {
               Serial.printf("SD read error: wav header not read\n");
               return(0);
            }
            if (!memcmp(headerData, FMT_CHUNK_ID,4)) {
                channels = GET_LE_SHORTWORD(headerData,0x0a);
                samplingRate = GET_LE_LONGWORD(headerData, 0x0c);
                bits = GET_LE_SHORTWORD(headerData,0x16);
            }
            offset+=chunkSize+8;
        }        
    }
    return(0);   
}



void ESP32Sound_Class::soundStreamTask( void * parameter )
{ 
    uint8_t first=1;
    uint8_t chunk[chunksize];
    int32_t ret=0;
    int32_t len = 0;
    int32_t toRead = 0;

    if (verbosity) Serial.println("SoundStreamTask created");    
    len = dataSize;
    sampleCounter=0;
    lastSample=len/(bits>>3)/channels;
    xQueueReset( xQueue );

    while (len>0) {
      if (len > chunksize) toRead=chunksize; else toRead=len;
      if ((ret=soundFile.read(chunk,toRead)) != toRead) {
            if (verbosity) Serial.printf("SD read error: %d of %d bytes read\n",ret,toRead);
            toRead=ret; 
      } 
      len-=toRead;
      int sp; 
      while ((sp=uxQueueSpacesAvailable (xQueue))<toRead) {
        // if (verbosity) Serial.printf("space in queue: %d/%d .. waiting!\n",sp,toRead);
        vTaskDelay(WAIT_FOR_QUEUESPACE);
      }
      for (int i=0;i<toRead;i++) {
        if (bits==8) {
           xQueueSend(xQueue,( void * ) &(chunk[i]), ( TickType_t ) portMAX_DELAY);
           if (channels==2) i++;
        }
        else if (bits==16) {
           int16_t val = chunk[i] | ((uint16_t)(chunk[i+1]) << 8);
           uint8_t act= val/256+128;
           
           xQueueSend(xQueue,( void * ) &(act), ( TickType_t ) portMAX_DELAY);
           i++;
           if (channels==2) i+=2;           
        }
      }
      
      if (first) {
          timerAlarmEnable(timer);  // in case timer is currently not running  
          if (verbosity) Serial.println("Timer enabled!\n");
          first=0;
      } 
    } 

    if (verbosity) Serial.printf("Finished soundfile after  %u bytes.\n", dataSize);
    soundFile.close();
    xHandle=NULL;
    vTaskDelete( NULL );
} 

ESP32Sound_Class ESP32Sound;
