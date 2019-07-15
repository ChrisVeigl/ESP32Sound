//
// ODROID-GO  Flappy Bird
//
// By Ponticelli Domenico.
// https://github.com/pcelli85/M5Stack_FlappyBird_game
//
// Added to by: Victor G. Brusca
// https://github.com/vbrusca/FlappyBirdCloneOdroidGo
//
// More extensions (sound, highscore & bird anim) by CV
// https://github.com/ChrisVeigl/ESP32Sound
// 
// For background music, copy file 'data/swan.wav' to root folder of SD-Card !
// Yes! - It's flappy Swan Lake !! ;-)
// 
// Use menu and volume button to change fx/music volume.
// Enter the hall-of-fame using navigation cross!
// Clear highscore by pressing Select and Start!
//

#include <odroid_go.h>
#include "ESP32Sound.h"
#include "sounds.h"
#include <EEPROM.h>

#define SOUNDFILE "/swan.wav"

//screen dimensions
#define TFT_X_S           0
#define TFT_Y_S           20
#define TFTW              320         //screen width
#define TFTH              240         //screen height 240 - 20 for score
#define TFTW2             160         //half screen width
#define TFTH2             110         //half screen height

//game constants
#define SPEED             1
#define GRAVITY           5.25
#define JUMP_FORCE        2.50

//bird size
#define BIRDW             16
#define BIRDH             10
#define BIRDW2            8
#define BIRDH2            5

//pip size
#define PIPEW             24
int GAPHEIGHT = 85; //65 - 85

//floor size
#define FLOORH            30 //floor height from bottom of the screen

//grass size
#define GRASSH            4 //grass height, starts at floor y

int maxScore = 0;
char maxName[4]="   ";
uint8_t letterPos=0;
uint8_t letterIndex=0;
uint8_t actFxVolume=20;
uint8_t actSoundVolume=30;
uint8_t soundAvailable=0;


//background
const unsigned int BCKGRDCOL = GO.lcd.color565(138, 235, 244);

//bird
const unsigned int BIRDCOL = GO.lcd.color565(255, 254, 174);

//pipe
const unsigned int PIPECOL = GO.lcd.color565(99, 255, 78);
//pipe highlight
const unsigned int PIPEHIGHCOL = GO.lcd.color565(250, 255, 250);
//pipe seam
const unsigned int PIPESEAMCOL = GO.lcd.color565(0, 0, 0);
//floor
const unsigned int FLOORCOL = GO.lcd.color565(246, 240, 163);
//grass (GRASSCOL2 is the stripe color)
const unsigned int GRASSCOL = GO.lcd.color565(141, 225, 87);
const unsigned int GRASSCOL2 = GO.lcd.color565(156, 239, 88);

//bird sprite colors (Cx name for values to keep array readable)
#define C0 BCKGRDCOL
#define C1 GO.lcd.color565(195, 165, 75)
#define C2 BIRDCOL
#define C3 TFT_WHITE
#define C4 TFT_RED
#define C5 GO.lcd.color565(251, 216, 114)
#define C6 TFT_BLACK

static unsigned int birdcol[] = {
  C0, C0, C0, C1, C1, C1, C1, C1, C1, C1, C1, C1, C0, C0, C0, C0,
  C0, C0, C1, C1, C2, C2, C2, C2, C2, C2, C3, C3, C1, C0, C0, C0,
  C0, C1, C1, C2, C2, C1, C2, C2, C2, C2, C3, C3, C1, C1, C0, C0,
  C1, C1, C2, C2, C2, C2, C2, C2, C2, C2, C3, C3, C3, C1, C0, C0,
  C1, C1, C2, C6, C6, C6, C6, C2, C2, C2, C2, C4, C4, C4, C4, C1,
  C0, C5, C5, C2, C2, C2, C2, C2, C2, C2, C4, C1, C1, C1, C1, C1,
  C0, C5, C5, C2, C6, C6, C2, C2, C2, C2, C2, C4, C4, C4, C1, C1,
  C0, C5, C5, C2, C2, C2, C2, C2, C2, C2, C2, C2, C2, C2, C1, C0,
  C0, C0, C5, C2, C2, C2, C4, C4, C2, C2, C2, C2, C2, C2, C1, C0,
  C0, C0, C5, C5, C5, C5, C5, C4, C5, C5, C5, C5, C5, C1, C0, C0
};

static unsigned int birdcol2[] = {
  C0, C0, C0, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C0, C0, C0,
  C0, C0, C1, C1, C2, C2, C2, C2, C2, C2, C3, C3, C1, C1, C0, C4,
  C0, C1, C1, C2, C2, C1, C2, C2, C2, C2, C3, C3, C4, C1, C4, C0,
  C1, C1, C2, C2, C2, C6, C6, C2, C2, C2, C3, C3, C3, C4, C1, C0,
  C1, C1, C2, C6, C6, C2, C6, C2, C2, C2, C2, C4, C4, C1, C1, C1,
  C0, C5, C5, C2, C2, C2, C2, C2, C2, C2, C4, C1, C1, C1, C0, C0,
  C0, C5, C5, C2, C6, C6, C6, C2, C2, C2, C2, C4, C4, C1, C0, C0,
  C0, C5, C5, C2, C2, C2, C2, C2, C2, C2, C2, C2, C2, C4, C1, C1,
  C0, C0, C5, C2, C4, C4, C4, C2, C2, C2, C2, C2, C2, C2, C4, C1,
  C0, C0, C5, C5, C4, C5, C5, C5, C5, C5, C5, C5, C5, C1, C1, C4
};


static struct BIRD {
  long x, y, old_y;
  long col;
  float vel_y;
} bird;

static struct PIPES {
  long x, gap_y;
  long col;  
  long old_x;
} pipes;

int score;
static short tmpx, tmpy;
unsigned char GAMEH = TFTH - FLOORH - TFT_Y_S;
long grassx;

double delta;
double old_time;
double next_game_tick;
double current_time;
int loops;

bool passed_pipe;
unsigned char px;
bool gameBegin = true;
bool gameEnd = false;

bool gameCd = false;
bool btnPressed = false;
int targetFps = 100;
float actualFps;

int targetFpsMs = 1000 / targetFps;
int lastFrameMs;
int extraMs;
double lstart;

double lstop;
double frameMs;
bool dirty = false;
bool dirtyScore = false;

int countDown = 3;
long gameCdStart = 0;
bool onCollision = true;
bool onJump = true;
bool onGravity = true;

bool showActualFps = false;
bool gamePause = false;
float lastVelY = 0.0;
long gamePauseStart;
const int chipSelect = 7;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if(!SD.begin())
      Serial.println("Card failed, or not present");
  else 
      Serial.println("card initialized.");

  if (!EEPROM.begin(5))  
      Serial.println("failed to initialize EEPROM");
  
  randomSeed(analogRead(0));   //generate new random seed for the pipe gap
  // gotcha: do this before GO.begin() - else Volume button won't work!

  GO.begin();
  ESP32Sound.begin(16000);
  ESP32Sound.setSoundVolume(actSoundVolume);
  ESP32Sound.setFxVolume(actFxVolume);

  if (SD.exists(SOUNDFILE)) soundAvailable = 1; 
  gameInit();  
}

void loop() { 
  GO.update();
  if(gameBegin) {
    gameStart();
  }else if(!gameBegin && !gameEnd) {
    if(gameCd) {
        drawBird();
        drawGetReady();
    }else {
      lstart = millis();      

      if(!gamePause) {
          gameLoop();
      }else {
        if((lstart - gamePauseStart) >= 2000) {
          gameEnd = true;
          gamePause = false;
          dirty = true;
          return;
        }
      }

      
      lstop = millis();
      lastFrameMs = (lstop - lstart);
      actualFps = 1000.0 / lastFrameMs;
      extraMs = targetFpsMs-lastFrameMs;
      if(extraMs > 0) {
        delay(extraMs);
      }
    }
  }else {
    gameOver();
  }
  loops++;
}

void drawBird() {
  tmpx = BIRDW-1;
  do {
      px = bird.x + tmpx + BIRDW;
      tmpy = BIRDH - 1;
      do {
        GO.lcd.drawPixel(px, bird.old_y + tmpy, BCKGRDCOL);
      } while (tmpy--);

      // draw bird sprite at new position
      tmpy = BIRDH - 1;
      do {
        if (GO.BtnB.isPressed())
           GO.lcd.drawPixel(px, bird.y + tmpy, birdcol2[tmpx + (tmpy * BIRDW)]);
        else
           GO.lcd.drawPixel(px, bird.y + tmpy, birdcol[tmpx + (tmpy * BIRDW)]);

      } while (tmpy--);
  } while (tmpx--);
  
  // save position to erase bird on next draw
  bird.old_y = bird.y;  
}

void drawPipe() {

  GO.lcd.drawFastVLine(0, TFT_Y_S, GAMEH - TFT_Y_S, BCKGRDCOL); 
  GO.lcd.drawFastVLine(1, TFT_Y_S, GAMEH - TFT_Y_S, BCKGRDCOL);
  GO.lcd.drawFastVLine(2, TFT_Y_S, GAMEH - TFT_Y_S, BCKGRDCOL);  
  
  if((pipes.x + PIPEW + 1) >= 0) {    
    //pipe color    
    if(pipes.x + 3 >= 0  && pipes.x + 3 <= TFTW) {
      GO.lcd.drawFastVLine(pipes.x + 3, TFT_Y_S, pipes.gap_y, PIPECOL);
      GO.lcd.drawFastVLine(pipes.x + 3, (pipes.gap_y + GAPHEIGHT + 1), (GAMEH - (pipes.gap_y + GAPHEIGHT + 1)), PIPECOL);
    }
    
    //highlight
    if(pipes.x >= 0  && pipes.x <= TFTW) {
      GO.lcd.drawFastVLine(pipes.x, TFT_Y_S, pipes.gap_y, PIPEHIGHCOL);
      GO.lcd.drawFastVLine(pipes.x, (pipes.gap_y + GAPHEIGHT + 1), (GAMEH - (pipes.gap_y + GAPHEIGHT + 1)), PIPEHIGHCOL);
    }
    
    //outline left
    if(pipes.x - 1 >= 0  && pipes.x - 1 <= TFTW) {
      GO.lcd.drawFastVLine(pipes.x - 1, TFT_Y_S, pipes.gap_y, TFT_BLACK);
      GO.lcd.drawFastVLine(pipes.x - 1, (pipes.gap_y + GAPHEIGHT + 1), (GAMEH - (pipes.gap_y + GAPHEIGHT + 1)), TFT_BLACK);
    }
    
    //outline right
    if((pipes.x + PIPEW + 1) >= 0  && (pipes.x + PIPEW + 1) <= TFTW) {
      GO.lcd.drawFastVLine(pipes.x + PIPEW + 1, TFT_Y_S, pipes.gap_y, TFT_BLACK);
      GO.lcd.drawFastVLine(pipes.x + PIPEW + 1, (pipes.gap_y + GAPHEIGHT + 1), (GAMEH - (pipes.gap_y + GAPHEIGHT + 1)), TFT_BLACK);    
    }
    
    //bottom and top border of pipe
    GO.lcd.drawFastHLine(pipes.x + 1, pipes.gap_y + 1 + TFT_Y_S, PIPEW - 2, TFT_BLACK);
    GO.lcd.drawFastHLine(pipes.x + 1, pipes.gap_y + GAPHEIGHT, PIPEW - 2, TFT_BLACK); 

    //pipe seam
    GO.lcd.drawFastHLine(pipes.x, pipes.gap_y + 1 + TFT_Y_S - 6, PIPEW, TFT_BLACK);
    GO.lcd.drawFastHLine(pipes.x, pipes.gap_y + GAPHEIGHT + 6, PIPEW, TFT_BLACK);

    if((pipes.x + PIPEW + 2) <= TFTW && (pipes.x + PIPEW + 2) >= 0) {
      GO.lcd.fillRect(pipes.x + PIPEW + 2, TFT_Y_S, 4, GAMEH - TFT_Y_S, BCKGRDCOL); 
    }       
  }
}

void drawLevel() {
  GO.lcd.fillRect(0, TFT_Y_S, TFTW, GAMEH, BCKGRDCOL);

  //draw the floor once, we will not overwrite on this area in-game
  //black line
  GO.lcd.drawFastHLine(TFT_X_S, GAMEH, TFTW, TFT_BLACK);  

  //grass and stripe
  GO.lcd.fillRect(TFT_X_S, (GAMEH + 1), TFTW2, GRASSH, GRASSCOL);
  GO.lcd.fillRect(TFTW2, (GAMEH + 1), TFTW2, GRASSH, GRASSCOL2);  

  //black line
  GO.lcd.drawFastHLine(TFT_X_S, (GAMEH + GRASSH), TFTW, TFT_BLACK);

  //mud
  GO.lcd.fillRect(TFT_X_S, (GAMEH + GRASSH + 1), TFTW, (FLOORH - GRASSH), FLOORCOL);

  //grass x position (for stripe animation)
  grassx = TFTW;
  
  //grass stripes
  grassx -= SPEED;
  if(grassx <= 0) {
    grassx = TFTW;
  }
    
  GO.lcd.drawFastVLine((grassx % TFTW), (GAMEH + 1), (GRASSH - 1), GRASSCOL);
  GO.lcd.drawFastVLine(((grassx + 64) % TFTW), (GAMEH + 1), (GRASSH - 1), GRASSCOL2);
}

void drawScore() {
  //update score
  if(showActualFps) {
    GO.lcd.fillRect(0, 0, TFTW, 20, TFT_BLACK);
  }
  
  GO.lcd.setTextColor(TFT_WHITE);
  GO.lcd.setCursor(4, 4);
  GO.lcd.print("Score:");
  GO.lcd.setCursor(80, 4);
  
  if(dirtyScore) {
    dirtyScore = false;
    GO.lcd.fillRect(80, 0, 60, 20, TFT_BLACK);
    GO.lcd.print(score);
  }

  if(showActualFps) {  
    GO.lcd.print(score);
    GO.lcd.setCursor(180, 4);
    GO.lcd.print(actualFps);
  }  
}

void drawGetReady() {    
  //update score
  if(dirtyScore) {
    GO.lcd.fillRect(0, 0, TFTW, 20, TFT_BLACK);
    dirtyScore = false;
  }
    
  GO.lcd.setTextColor(TFT_WHITE);
  GO.lcd.setCursor(4, 4);
  GO.lcd.print("Get Ready ");
  GO.lcd.setCursor(130, 4);    
  GO.lcd.print(countDown);
  
  if(countDown <= 0) {
    loops = 0;
    gameCd = false;
    GO.lcd.fillRect(0, 0, TFTW, 20, TFT_BLACK);
    dirtyScore = true;
    return;  
  }    

  long now = millis();
  if(now - gameCdStart >= 1000) {
    gameCdStart = now;
    countDown--;
    dirtyScore = true;
  }
}

void update() {
  if(onJump) {
    if (!btnPressed && GO.BtnB.wasPressed()) {
      btnPressed = true;
    }else if(btnPressed) { // && GO.BtnB.wasReleased()) {
      btnPressed = false;
      //if the bird is not too close to the top of the screen apply jump force
      if (bird.y > BIRDH2 * 0.5) {
        lastVelY = -(JUMP_FORCE / 5);
        ESP32Sound.playFx(fx1);
      }else {
        lastVelY = 0;
      } 
      bird.vel_y = lastVelY;     
    }
  }
  
  old_time = current_time;
  current_time = lstart;
  delta = (current_time - old_time) / 1000;
        
  //pipe
  pipes.old_x = pipes.x;
  pipes.x -= SPEED;
  if(pipes.x + PIPEW + 2 <= 0) {
    pipes.x = TFTW;
    GO.lcd.fillRect(0, TFT_Y_S, 10, GAMEH - TFT_Y_S, BCKGRDCOL);
    pipes.gap_y = random(10, (GAMEH - (10 + GAPHEIGHT)));
    int minHeight= 65-score/2; if (minHeight<50) minHeight=50;
    GAPHEIGHT = random(minHeight, 85-score);  // well - it gets harder ;-)
    if((pipes.gap_y + GAPHEIGHT) >= (TFTH - FLOORH - TFT_Y_S)) {
      pipes.gap_y -= 40;
    }
  }  

  //bird
  if(onGravity && loops >= 3) {
    bird.vel_y += (GRAVITY * delta);
    if ((bird.y>20) || (bird.vel_y>0))
      bird.y += bird.vel_y;
  }

  if(onCollision) {
    //if the bird hit the ground game over
    if(bird.y + BIRDH >= GAMEH) {
      gamePause = true;
      gamePauseStart = millis();
    }
  
    //checking for bird collision with pipe
    if((bird.x + BIRDW) >= (pipes.x - BIRDW2) && bird.x <= (pipes.x + PIPEW - BIRDW)) {
      if(bird.y < (pipes.gap_y + TFT_Y_S) || (bird.y + BIRDH) > (pipes.gap_y + GAPHEIGHT)) {
        gamePause = true;
        gamePauseStart = millis();
      }else {
        passed_pipe = true;
      }
    }else if(bird.x > (pipes.x + PIPEW - BIRDW) && passed_pipe) {
      passed_pipe = false;
      dirtyScore = true;
      score++;
    }
    if (gamePause) {
        ESP32Sound.stopSound();
        ESP32Sound.playFx(fx2);
    }

  }
}

void gameLoop() {
  update();
  drawPipe();
  drawBird();
  drawScore();
  if (soundAvailable && !ESP32Sound.isPlaying() && !gamePause) {
    ESP32Sound.playSound(SD, SOUNDFILE);
  }
}



void gameStart() {
  if(dirty) {
    dirty = false;  
    //instead of calculating the distance of the floor from the screen height each time store it in a variable  
    GO.lcd.fillScreen(TFT_BLACK);
    GO.lcd.fillRect(10, TFTH2 - 32, TFTW - 20, 1, TFT_WHITE);
    GO.lcd.fillRect(10, TFTH2 + 34, TFTW - 20, 1, TFT_WHITE);  
    GO.lcd.setTextColor(TFT_WHITE);
  
      //half width - num char * char width in pixels
    GO.lcd.setTextSize(3);
    GO.lcd.setCursor(TFTW2 - (6 * 9), TFTH2 - 26);
    GO.lcd.println("FLAPPY");
  
    GO.lcd.setTextSize(3);
    GO.lcd.setCursor(TFTW2 - (6 * 9), TFTH2 + 8);
    GO.lcd.println("-BIRD-");
  
    GO.lcd.setTextSize(2);
    GO.lcd.setCursor(10, TFTH2 - 52);
    GO.lcd.print("ODROID-GO");


    GO.lcd.setTextColor(TFT_WHITE);
    GO.lcd.setCursor(TFTW2 - (17 * 6) + 5, TFTH2 + 46);
    GO.lcd.println("Press A to Start");

    GO.lcd.setCursor(TFTW2 - (17 * 6) + 12, TFTH2 + 65);
    GO.lcd.println("Press B to Flap");    

    GO.lcd.setTextSize(1);
    GO.lcd.setCursor(TFTW2 - (17 * 3) + 5, TFTH - 35);
    GO.lcd.println("Middlemind Games");

    GO.lcd.setCursor(10, 225);
    GO.lcd.printf("FX:%d",actFxVolume);
    GO.lcd.setCursor(270, 225);
    GO.lcd.printf("Vol:%d",actSoundVolume);

    printMaxScore();
  }
  
  //wait for push button
  if ((GO.BtnSelect.isPressed()) && (GO.BtnStart.isPressed()))  { 
      resetMaxScore();
      dirty=true;
  }
  if (GO.BtnMenu.wasPressed()) { 
      actFxVolume+=10; 
      if (actFxVolume>100) actFxVolume=0; 
      ESP32Sound.setFxVolume(actFxVolume);
      dirty=true;
  }
  if (GO.BtnVolume.wasPressed()) { 
      actSoundVolume+=10; 
      if (actSoundVolume>100) actSoundVolume=0; 
      ESP32Sound.setSoundVolume(actSoundVolume);
      dirty=true;
  } 
  if(GO.BtnA.wasPressed()) {
    gameBegin = false;
    gameEnd = false;
    gameCd = true;
    gameCdStart = millis();
    dirtyScore = true;
    drawLevel();
  }
}

void printMaxScore() {
  GO.lcd.setTextColor(TFT_RED,TFT_BLACK);
  GO.lcd.setTextSize(1);
  GO.lcd.setCursor(190,60);
  if (strcmp(maxName,"   ")) {
    GO.lcd.print("Highscore: ");
    GO.lcd.print(maxScore);
    GO.lcd.print(" by ");
    GO.lcd.print(maxName);
  }
  else {
    GO.lcd.print("Highscore cleared ! ");
  }
  GO.lcd.setTextSize(2);
}

void gameInit() {
  if(EEPROM.read(4) == 42) { 
    maxScore=(int)EEPROM.read(0);
    maxName[0]=EEPROM.read(1);
    maxName[1]=EEPROM.read(2);
    maxName[2]=EEPROM.read(3);
  }
  //clear screen
  GO.lcd.fillScreen(BCKGRDCOL);

  //reset score
  score = 0;
  countDown = 3;
  
  //init bird
  bird.x = 144;
  bird.y = bird.old_y = (TFTH2 - BIRDH);
  bird.vel_y = 0;//-JUMP_FORCE;
  tmpx = tmpy = 0;
 
  //init pipe
  pipes.x = TFTW;
  pipes.gap_y = random(10, (GAMEH - (10 + GAPHEIGHT)));
  if((pipes.gap_y + GAPHEIGHT) >= (TFTH - FLOORH - TFT_Y_S)) {
    pipes.gap_y -= 40;
  }

  dirty = true;
}


void gameOver() {
  if(dirty) {
    dirty = false;  
    GO.lcd.fillScreen(TFT_BLACK);
  
    GO.lcd.setTextColor(TFT_WHITE);
    GO.lcd.setTextSize(3);
  
    //half width - num char * char width in pixels
    GO.lcd.setCursor(TFTW2 - (9 * 9), TFTH2 - 6);
    GO.lcd.println("GAME OVER");
    GO.lcd.setTextSize(2);
    GO.lcd.setCursor(10, 10);
    GO.lcd.print("Score: ");
    GO.lcd.print(score);

    if(score > maxScore) {
      GO.lcd.setTextColor(TFT_RED,TFT_BLACK);
      GO.lcd.setTextSize(2);
      GO.lcd.setCursor(TFTW2 - (13*6), TFTH2 - 36);
      GO.lcd.println("NEW HIGHSCORE");
      GO.lcd.setCursor(TFTW2 - (12 * 10) + 20, TFTH2 + 28);
      GO.lcd.print("Enter Name: ");
      GO.lcd.setCursor(TFTW2 + 50, TFTH2 + 28);
      strcpy(maxName,"AAA");
      GO.lcd.print(maxName);
      letterPos=0;
      letterIndex=0;
    }
    else {
      GO.lcd.setCursor(TFTW2 - (12 * 10) + 3, TFTH2 + 28);
      GO.lcd.println("Press A to Continue");
      GO.lcd.setCursor(10, 28);
      GO.lcd.print("Max Score: ");
      GO.lcd.print(maxScore);
    }
  }

  if(score > maxScore) {
      //Serial.println(GO.JOY_Y.readAxis()); 
      delay(100);
      GO.lcd.setCursor(TFTW2 + 50, TFTH2 + 28);
      if (GO.JOY_Y.isAxisPressed() == 2)  {
        if (maxName[letterIndex]<'Z')
          maxName[letterIndex]++;
        else maxName[letterIndex]='A';
        GO.lcd.print(maxName);
        delay(100);
      }  
      else if (GO.JOY_Y.isAxisPressed() == 1)  {
        if (maxName[letterIndex]>'A')
          maxName[letterIndex]--;
        else maxName[letterIndex]='Z';
        GO.lcd.print(maxName);
        delay(100);
      } 
      else if (GO.JOY_X.isAxisPressed() == 2)  {
        if (letterIndex>0)
          letterIndex--;
        else letterIndex=2;
        delay(200);
      }
      else if (GO.JOY_X.isAxisPressed() == 1)  {
        if (letterIndex<2)
          letterIndex++;
        else letterIndex=0;
        delay(200);
      }
      else if(GO.BtnA.wasPressed()) {
        setMaxScore((byte)score);
        dirty = true;  
      }
  }
  else {
    //wait for push button
    if(GO.BtnA.wasPressed()) {
      gameEnd = false;
      gameBegin = true;
      dirtyScore = true;
      lstart = millis();
      gameInit();
    }
  }
}

void setMaxScore(byte score) {
  EEPROM.write(0,score);
  EEPROM.write(1,maxName[0]);
  EEPROM.write(2,maxName[1]);
  EEPROM.write(3,maxName[2]);
  EEPROM.write(4,42); // the answer to everything!
  EEPROM.commit();
  maxScore = score;
}

void resetMaxScore() {
  strcpy(maxName,"   ");
  setMaxScore(0);
}