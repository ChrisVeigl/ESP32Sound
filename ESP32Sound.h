//
//  ESP32Sound library for ODROID-GO
//  Version 1.0, 2019-06-10
//

#ifndef _ESP32Sound_H_
#define _ESP32Sound_H_

#if defined(ESP32)

#define SPEAKER_PIN 26
#define TONE_PIN_CHANNEL 0
#define SOUNDBUF_SIZE 1024
#define SOUNDBUF_HALF (SOUNDBUF_SIZE/2)
#define DEFAULT_SOUND_VOLUME 30
#define DEFAULT_FX_VOLUME 50


class ESP32Sound_Class {

 private: 
    static hw_timer_t * timer;
    static SemaphoreHandle_t xSemaphore;
    static portMUX_TYPE mux;
    static TaskHandle_t xHandle;
    static void soundTimer();   // the timer ISR

    static File soundFile;
    static uint8_t buf[SOUNDBUF_SIZE];
    static volatile uint16_t sampleCounter;
    static volatile uint16_t lastSample;
    static volatile int16_t loadNext;
    static volatile uint16_t playFXLen;
    static volatile uint8_t playStream;
    static const uint8_t * playFXLoc;
    static volatile uint8_t fxVolume;
    static volatile uint8_t soundVolume;
 
  public: 
    static void begin(uint16_t samplingrate);              // initialize system, set playback rate
    static void playSound(fs::FS &fs, const char * path);  // start music playback from file
    static boolean isPlaying();                  // true if music is playing, false otherwise 
    static void stopSound();                     // stops playback
    static void playFx(const uint8_t * fxBuf);   // plays small effects from flash memory
    static void setPlaybackRate(uint16_t pr);    // sets playback rate in samples/sec
    static void setFxVolume(uint8_t vol);        // sets effects volume (in %, 0-255, 100 is original)
    static void setSoundVolume(uint8_t vol);     // sets music volume (in %, 0-255, 100 is original)
    static void soundStreamTask( void * parameter );
};

extern ESP32Sound_Class ESP32Sound;

#else
#error "This library only supports boards with ESP32 processor."
#endif

#endif
