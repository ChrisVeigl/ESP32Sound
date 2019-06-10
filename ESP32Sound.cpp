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

#include <odroid_go.h>
#include "ESP32Sound.h"


// definition/initialisation of the static class members 
// (the class is a static/singleton!)
hw_timer_t *      ESP32Sound_Class::timer = NULL;
SemaphoreHandle_t ESP32Sound_Class::xSemaphore = NULL;
portMUX_TYPE      ESP32Sound_Class::mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t      ESP32Sound_Class::xHandle = NULL;
File              ESP32Sound_Class::soundFile;
volatile uint8_t  ESP32Sound_Class::soundVolume = DEFAULT_SOUND_VOLUME;
volatile uint8_t  ESP32Sound_Class::fxVolume = DEFAULT_FX_VOLUME;
uint8_t           ESP32Sound_Class::buf[SOUNDBUF_SIZE];
volatile uint16_t ESP32Sound_Class::sampleCounter = 0;
volatile uint16_t ESP32Sound_Class::lastSample = SOUNDBUF_HALF;
volatile int16_t  ESP32Sound_Class::loadNext = 0;
const uint8_t *   ESP32Sound_Class::playFXLoc = NULL;
volatile uint16_t ESP32Sound_Class::playFXLen = 0;
volatile uint8_t  ESP32Sound_Class::playStream = 0;


void IRAM_ATTR ESP32Sound_Class::soundTimer(){
  uint8_t dacValue=127;

  // mix FX samples
  if (playFXLen) {
    dacValue+=(*playFXLoc-127)*fxVolume/100;
    playFXLoc++;
    playFXLen--;
  } 

  // mix streaming samples
  if (playStream) {
    dacValue+=(buf[sampleCounter]-127)*soundVolume/100;
    portENTER_CRITICAL_ISR(&mux);
    sampleCounter++;
    if (sampleCounter == lastSample) {
      // switch to the other buffer 
      if (lastSample == SOUNDBUF_HALF) {
        lastSample=SOUNDBUF_SIZE;
        loadNext=0;
      } else {
        sampleCounter=0;
        lastSample=SOUNDBUF_HALF;              
        loadNext=SOUNDBUF_HALF;
      }
      // signal to soundStreamTask that buffer finished -> load next
      xSemaphoreGiveFromISR(xSemaphore, NULL);    
    }
    portEXIT_CRITICAL_ISR(&mux);
  }
  dacWrite(SPEAKER_PIN, dacValue); 

  // check if we finished playing
  if ((!playFXLen) && (!playStream)) {
    dacWrite(SPEAKER_PIN, 127);
    timerAlarmDisable(timer); 
  }
}

void ESP32Sound_Class::begin(uint16_t samplingrate)  {
    ledcSetup(TONE_PIN_CHANNEL, 0, 13);
    ledcAttachPin(SPEAKER_PIN, TONE_PIN_CHANNEL);

    xSemaphore = xSemaphoreCreateMutex();

    timer = timerBegin(0, 80, true);   // prescaler 80 : 1MHz
    timerAttachInterrupt(timer, &soundTimer, true);
    timerAlarmWrite(timer, 1000000/samplingrate, true);  // periodic timer call with ~samplingrate
    timerAlarmDisable(timer);
}

void ESP32Sound_Class::playSound(fs::FS &fs, const char * path){
    if (xHandle!=NULL) {
      Serial.printf("sound already playing!\n");
      return;
    }
    
    soundFile = fs.open(path);   
    if(soundFile){
        Serial.printf("open file %s successful!\n", path);
        xTaskCreate(  soundStreamTask,      /* Task function. */
                      "sst1",           /* String with name of task. */
                      4000,             /* Stack size in bytes. */
                      NULL,             /* Parameter passed as input of the task */
                      1,                /* Priority of the task. */
                      &xHandle);            /* Task handle. */                    
    } else Serial.println("Failed to open file for reading");
}

boolean ESP32Sound_Class::isPlaying(){
    uint8_t tmp;
    portENTER_CRITICAL(&mux);             
    tmp=playStream;
    portEXIT_CRITICAL(&mux);
    if (tmp) return(true);
    return(false); 
}

void ESP32Sound_Class::stopSound(){
    if (xHandle!=NULL) {
      portENTER_CRITICAL(&mux);             
      playStream=0;
      portEXIT_CRITICAL(&mux);             
      soundFile.close();
      vTaskDelete( xHandle );
      xHandle=NULL;
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

void ESP32Sound_Class::soundStreamTask( void * parameter )
{ 
    uint8_t first=1;
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;

    Serial.println("SoundStreamTask created");    
    len = soundFile.size();
    size_t flen = len;
    start = millis();
    portENTER_CRITICAL(&mux);             
    loadNext=0;
    sampleCounter=0;
    portEXIT_CRITICAL(&mux);             
    while(len>0 ) { 
        size_t toRead = len;
        if(toRead > SOUNDBUF_HALF) toRead = SOUNDBUF_HALF;
        // load half of the buffer (the half the timer ISR is currently not reading!)
        soundFile.read(buf+loadNext, toRead);
        // Serial.printf("*");
        if (first) {
            portENTER_CRITICAL(&mux);             
            playStream=1;
            portEXIT_CRITICAL(&mux);             
            timerAlarmEnable(timer);  // in case timer is currently not running  
            first=0; 
        }
        
        // wait until timer ISR signals that it read 50% of the buffer
        xSemaphoreTake( xSemaphore, ( TickType_t ) portMAX_DELAY );
        len -= toRead;
    }
    portENTER_CRITICAL(&mux);             
    playStream=0;
    portEXIT_CRITICAL(&mux);             

    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    soundFile.close();
    xHandle=NULL;
    vTaskDelete( NULL );
}

ESP32Sound_Class ESP32Sound;
