//
// Demo for the ESP32Sound library 
// https://github.com/ChrisVeigl/ESP32Sound
//
// This demo plays a short sound effect from flash memory.
// See documentation on github how to convert sound files to c-arrays (in sound.h)
//


#include <odroid_go.h>
#include "ESP32Sound.h"
#include "sounds.h"

#define PLAYBACK_RATE  16000

void setup(){
    GO.begin();
    Serial.begin(115200);
    Serial.println("Now initialising sound system!");
    ESP32Sound.begin(PLAYBACK_RATE);
}

void loop(){
    for (int i=0; i<4; i++) {
      ESP32Sound.playFx(coo);
      delay(2000);
    }
    delay(10000);
}

