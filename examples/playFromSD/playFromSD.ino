//
// Demo for the ESP32Sound library 
// https://github.com/ChrisVeigl/ESP32Sound
//
// This demo plays a music file from SD-Card.
// Put the file "sound1.raw" from the data folder into the root folder of the SD-card.
//


#include <odroid_go.h>
#include "ESP32Sound.h"

#define PLAYBACK_RATE  16000

void setup(){
    Serial.begin(115200);
    Serial.println("Now initialising SD card!");
    if(!SD.begin()){
        Serial.println("Card Mount Failed");
        return;
    }
    if(SD.cardType() == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }

    GO.begin();
    Serial.println("Now initialising sound system!");
    ESP32Sound.begin(PLAYBACK_RATE);
}

void loop(){
    ESP32Sound.playSound(SD, "/sound1.raw");
    while (ESP32Sound.isPlaying());
    delay(1000);
}


