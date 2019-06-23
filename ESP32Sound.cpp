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
//
//  Preparation/placement of sound files:
//  The sound files must be provided in 8-bit, mono, raw format. The sampling rate
//  can be chosen on demand, 16000 is recommended (more means more CPU-load). 
//  A nice audio tool which can be used to create .raw format from .mp3 or .wav is Audacity. 
//  For instructions see: https://www.hellomico.com/getting-started/convert-audio-to-raw/
//  The background music files can be placed on the SD card, the file path is given to the
//  playSound function as an argument (with a leading slash), eg. playSound(SD, "/myfile.raw")
//  The FX files are stored in flash memory in a C-array. Only small files should be used.
//  A python tool to convert .raw files into C-arrays is provided (raw2array.py). This tool
//  creates a header file "sounds.h" from one or more .raw files, eg:
//  python raw2array.py sound1.raw sound2.raw sound3.raw
//  After including "sounds.h" into your sketch, you can use playFX(sound1), playFX(sound2)...
//
//  The current version of the library uses a timer ISR (called with sampling rate) 
//  for feeding the sound-samples into the DAC of the ODROID-GO. 
//  Future improvements might use DMA/I2S to increase performance.
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
volatile uint16_t ESP32Sound_Class::playFXLen = 0;
volatile uint8_t  ESP32Sound_Class::playStream = 0;
uint8_t           ESP32Sound_Class::peak=0;
uint8_t           ESP32Sound_Class::verbosity=0;


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

void ESP32Sound_Class::begin(uint16_t samplingrate, uint16_t size)  {

    pinMode(AMP_PIN, OUTPUT);
    digitalWrite(AMP_PIN, HIGH);
    dacWrite(DAC_PIN, 127);
    xQueue = xQueueCreate( size, 1 );
    if (verbosity) Serial.printf("Init sound: SR=%d, size=%d\n",samplingrate,size);
    bufsize=size;
    timer = timerBegin(0, 80, true);   // prescaler 80 : 1MHz
    timerAlarmDisable(timer);
    timerAttachInterrupt(timer, &soundTimer, true);
    timerAlarmWrite(timer, 1000000/samplingrate, true);  // periodic timer call with ~samplingrate
}

void ESP32Sound_Class::playSound(fs::FS &fs, const char * path){
    if (xHandle!=NULL) {
      if (verbosity) Serial.printf("sound already playing!\n");
      return;
    }

    soundFile = fs.open(path);   
    if(soundFile){
        if (verbosity) Serial.printf("open file %s successful!\n", path);
        xTaskCreate(  soundStreamTask,      /* Task function. */
                      "sst1",           /* String with name of task. */
                      4000,             /* Stack size in bytes. */
                      NULL,             /* Parameter passed as input of the task */
                      1,                /* Priority of the task. */
                      &xHandle);            /* Task handle. */                    
    } 
    // else if (verbosity) Serial.println("Failed to open file for reading");
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
    portENTER_CRITICAL(&mux);             
    playStream=0;
    portEXIT_CRITICAL(&mux);             
    soundFile.close();
    vTaskDelete( xHandle );
    xHandle=NULL;
    if (verbosity) Serial.println("SoundstreamTask closed.");
  }
}

void ESP32Sound_Class::playFx(const uint8_t * fxBuf){
    portENTER_CRITICAL(&mux);             
    playFXLoc=fxBuf+2;
    // the length is stored in the first two bytes (high byte first)
    playFXLen= (((uint16_t) fxBuf[0])<<8) + ((uint16_t) fxBuf[1]);
    portEXIT_CRITICAL(&mux);               
    timerAlarmEnable(timer); // in case timer is currently not running  
}

void ESP32Sound_Class::setPlaybackRate(uint16_t pr){
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

void ESP32Sound_Class::setVerbosity(uint8_t v){
    portENTER_CRITICAL(&mux);             
    verbosity=v;
    portEXIT_CRITICAL(&mux);             
}

void ESP32Sound_Class::soundStreamTask( void * parameter )
{ 
    uint8_t first=1;
    uint8_t chunk[chunksize];
    int32_t ret=0;
    int32_t len = 0;
    int32_t toRead = 0;

    if (verbosity) Serial.println("SoundStreamTask created");    
    len = soundFile.size();
    sampleCounter=0;
    lastSample=len;
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
        if (verbosity) Serial.printf("space in queue: %d/%d .. waiting!\n",sp,toRead);
        vTaskDelay(WAIT_FOR_QUEUESPACE);
      }
      for (int i=0;i<toRead;i++) {
        xQueueSend(xQueue,( void * ) &(chunk[i]), ( TickType_t ) portMAX_DELAY);
      }
      
      if (first) {
          portENTER_CRITICAL(&mux);             
          playStream=1;
          portEXIT_CRITICAL(&mux);             
          timerAlarmEnable(timer);  // in case timer is currently not running  
          if (verbosity) Serial.println("Timer enabled!\n");
          first=0;
      } 
    } 
                   
    if (verbosity) Serial.printf("Finished soundfile after  %u bytes.\n", len);
    soundFile.close();
    xHandle=NULL;
    vTaskDelete( NULL );
} 

ESP32Sound_Class ESP32Sound;
