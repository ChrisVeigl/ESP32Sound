//
//  ESP32Sound library for ODROID-GO
//  Version 1.0, 2019-06-10
//

#ifndef _ESP32Sound_H_
#define _ESP32Sound_H_

#if defined(ESP32)

#include <FS.h>

#define AMP_PIN 25                   // see ODROID-GO schematics
#define DAC_PIN 26                   // internal DAC2 (pin 26) is used, see ODROID-GO schematics
#define DEFAULT_SAMPLINGRATE 16000
#define DEFAULT_SOUNDBUF_SIZE 4096   // default queue size 
#define DEFAULT_CHUNK_SIZE 512       // samples to read from SD at once 
#define DEFAULT_SOUND_VOLUME 30
#define DEFAULT_FX_VOLUME 50
#define PEAKDECAY_INTERVAL 50    // samples to wait for peak auto-decrease
#define WAIT_FOR_QUEUESPACE 10   // ticks to wait if queue has not enough space 

class ESP32Sound_Class {

 private: 
    static hw_timer_t * timer;
    static QueueHandle_t xQueue;
    static portMUX_TYPE mux;
    static TaskHandle_t xHandle;
    static void soundTimer();   // the timer ISR
    static uint8_t getWavHeader();
    static void setPlaying(uint8_t p);

    static File soundFile;
    static uint8_t * buf;
    static uint16_t bufsize;
    static uint16_t chunksize;
    static volatile uint32_t sampleCounter;
    static volatile uint32_t lastSample;
    static const uint8_t *   playFXLoc;
    static volatile uint32_t playFXLen;
    static volatile uint8_t playStream;
    static volatile uint8_t fxVolume;
    static volatile uint8_t soundVolume;
    static uint8_t  peak;
    static uint8_t  verbosity;
    static uint16_t channels;
    static uint16_t bits;
    static uint32_t samplingRate;
    static uint32_t dataStart;
    static uint32_t dataSize;
 
  public: 
    // initialize system, set playback rate and buffer size
    static void begin(uint32_t samplingrate=DEFAULT_SAMPLINGRATE, uint16_t soundbufSize=DEFAULT_SOUNDBUF_SIZE);
    static void playSound(fs::FS &fs, const char * path);  // start music playback from file
    static boolean isPlaying();                  // true if music is playing, false otherwise 
    static void stopSound();                     // stops playback
    static void playFx(const uint8_t * fxBuf);   // plays small effects from flash memory
    static void setPlaybackRate(uint32_t pr);    // sets playback rate in samples/sec
    static void setFxVolume(uint8_t vol);        // sets effects volume (in %, 0-255, 100 is original)
    static void setSoundVolume(uint8_t vol);     // sets music volume (in %, 0-255, 100 is original)
    static void setVerbosity(uint8_t verbosity); // 0: quite, 1:chatty
    static uint8_t getPeak();                    // gets the current peak volume (0-127)

    static void soundStreamTask( void * parameter );
};

extern ESP32Sound_Class ESP32Sound;

#else
#error "This library only supports boards with ESP32 processor."
#endif

#endif
