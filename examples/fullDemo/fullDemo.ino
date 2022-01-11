//
// Demo for the ESP32Sound library 
// https://github.com/ChrisVeigl/ESP32Sound
//
// This demo shows concurrent playback of music and effects + LCD user interface.
// Put the .wav music file from folder '/data' into the root folder of the SD-card,
// and/or add other .wav sound files there. 
// See github readme for format conversion infos! 
//


#include <odroid_go.h>
#include "ESP32Sound.h"
#include "sounds.h"

#define PLAYBACK_RATE  16000
#define BUFFER_SIZE  1024

#define MAX_SOUNDFILES 10
#define MAX_SOUNDFILENAME_LEN 16

char **soundfileNames; 
uint8_t soundfileCount=0;
uint8_t actSoundfile=0;
uint8_t actFxVolume = 50;
uint8_t actSoundVolume = 30;
uint16_t actPlaybackRate = PLAYBACK_RATE;


uint16_t getSoundfiles(fs::FS &fs, const char * dirname, char** fileNames);
void displayGUI();


void setup(){
    GO.begin();
    // Serial.begin(115200);
    delay(500);
    Serial.println("Now initialising SD card!");
    if(!SD.begin()){
        Serial.println("Card Mount Failed");
        return;
    }
    if(SD.cardType() == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }
    soundfileNames= (char**) calloc(MAX_SOUNDFILES,sizeof(char*));
    soundfileCount=getSoundfiles(SD,"/", soundfileNames);
    Serial.printf("We have %d soundfiles!\n",soundfileCount);
    
    Serial.println("Now initializing LCD etc.");
    displayGUI();

    Serial.println("Now initialising sound system!");
    ESP32Sound.begin(PLAYBACK_RATE,BUFFER_SIZE);
}

void loop(){
    static int actButtonState=0, oldButtonState=1234;
    static unsigned long previousMillis=0;
    static uint16_t fps=0;

    GO.update();
    actButtonState= GO.JOY_Y.isAxisPressed() + (GO.JOY_X.isAxisPressed()<<2) 
                 + (GO.BtnB.isPressed() << 4) + (GO.BtnA.isPressed() << 5)
                 + (GO.BtnMenu.isPressed()<<6) +(GO.BtnVolume.isPressed()<<7)
                 + (GO.BtnSelect.isPressed()<<8)+(GO.BtnStart.isPressed()<<9); 

    if (actButtonState != oldButtonState) {
      if (GO.JOY_Y.isAxisPressed() == 2)   ESP32Sound.playFx(fx1);
      if (GO.JOY_Y.isAxisPressed() == 1)   ESP32Sound.playFx(fx2);
      if (GO.JOY_X.isAxisPressed() == 2)   ESP32Sound.playFx(fx3);
      if (GO.JOY_X.isAxisPressed() == 1)   ESP32Sound.playFx(fx4);
      if (GO.BtnB.isPressed())   ESP32Sound.stopSound();
      if (GO.BtnA.isPressed() && soundfileCount)  
          ESP32Sound.playSound(SD, soundfileNames[actSoundfile]);
      if (GO.BtnMenu.isPressed()) { 
          actFxVolume+=10; 
          if (actFxVolume>150) actFxVolume=0; 
          ESP32Sound.setFxVolume(actFxVolume);
          displayGUI();
      }
      if (GO.BtnVolume.isPressed()) { 
          actSoundVolume+=10; 
          if (actSoundVolume>150) actSoundVolume=0; 
          ESP32Sound.setSoundVolume(actSoundVolume);
          displayGUI();
      } 
      if (GO.BtnSelect.isPressed()) { 
          actSoundfile++; 
          if (actSoundfile>=soundfileCount) actSoundfile=0; 
          displayGUI();
      }
      if (GO.BtnStart.isPressed())  { 
          actPlaybackRate+=1000; 
          if (actPlaybackRate>24000) actPlaybackRate=8000; 
          ESP32Sound.setPlaybackRate(actPlaybackRate);
          displayGUI();
      }
      oldButtonState=actButtonState;
      // displayGUI();  // uncomment if you want to see all button updates (slower!)
    }

    int p=ESP32Sound.getPeak()/6;
    for (int i=0; i<18;i++) {
        if (i<p) {
            if (i>14)
               GO.lcd.fillRect(290, 220-i*12, 20, 10, RED);
            else if (i>8)
               GO.lcd.fillRect(290, 220-i*12, 20, 10, YELLOW);
            else 
               GO.lcd.fillRect(290, 220-i*12, 20, 10, GREEN);
        } 
        else GO.lcd.fillRect(290, 220-i*12, 20, 10, BLACK);
    }

    fps++;
    if (millis() - previousMillis >= 1000) {
        previousMillis=millis();
        GO.lcd.setCursor(270,5);
        GO.lcd.setTextSize(1);
        GO.lcd.printf("FPS:%d ",fps);
        fps=0;
    }
    delay (10);  // keep some SPI bandwith for sound (in case of heavy LCD traffic) !
}

void displayGUI() {
    GO.lcd.setCursor(0,0);
    GO.lcd.setTextSize(2);
    GO.lcd.setTextColor(TFT_RED, TFT_BLACK);
    GO.lcd.println("ODROID-GO Sound Demo!\n");
    GO.lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    GO.lcd.println("Cross: play FX!");
    GO.lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    if (GO.JOY_Y.isAxisPressed() == 2) 
        GO.lcd.println ("  Up: play FX1");
    else if (GO.JOY_Y.isAxisPressed() == 1) 
        GO.lcd.println ("  Down: play FX2");
    else if (GO.JOY_X.isAxisPressed() == 2) 
        GO.lcd.println ("  Left: play FX3");
    else if (GO.JOY_X.isAxisPressed() == 1) 
        GO.lcd.println ("  Right: play FX4");
    else GO.lcd.println("                  ");

    GO.lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    GO.lcd.println("\nButtons play/stop sound!");
    GO.lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    if (GO.BtnB.isPressed() == 1) 
        GO.lcd.println("  B: Stop Sound");
    else if (GO.BtnA.isPressed() == 1) 
        GO.lcd.println("  A: Start Sound");
    else GO.lcd.println("                   ");

    GO.lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    GO.lcd.println("\nChange settings:");
    GO.lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    GO.lcd.printf("      Rate (Sta): %d \n", actPlaybackRate);
    GO.lcd.printf("    FX-Vol (Men): %d   \n", actFxVolume);
    GO.lcd.printf(" Sound-Vol (Vol): %d   \n", actSoundVolume);
    if (soundfileCount) 
        GO.lcd.printf(" Soundfile (Sel):\n %s                               ", soundfileNames[actSoundfile]);
    else
        GO.lcd.printf(" no soundfiles found on SD\n");
}



uint16_t getSoundfiles(fs::FS &fs, const char * dirname, char** fileNames){
    uint16_t count=0;
    Serial.printf("getting soundfiles from: %s\n", dirname);

    File root = fs.open(dirname);
    if((!root) || (!root.isDirectory())) 
        return (0);

    File file = root.openNextFile();
    while(file){
        if(!file.isDirectory()){
            if (strstr(file.name(),".wav")) {
                Serial.print("  FILE: ");
                Serial.print(file.name());
                Serial.print("  SIZE: ");
                Serial.println(file.size());
                if ((count<MAX_SOUNDFILES) && (strlen(file.name()) < MAX_SOUNDFILENAME_LEN)) {
                    fileNames[count]= (char*) calloc(strlen(file.name()),sizeof(char));
                    strcpy (fileNames[count],file.name());
                    count++;
                }
            }
        }
        file = root.openNextFile();
    }
    return (count);
}
