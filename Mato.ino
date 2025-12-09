#include <EEPROM.h>
#include <IoAbstraction.h>
#include <LinkedListLib.h>
#include <Adafruit_SSD1306.h>
#include <TM1637Display.h>

//for tm1637
#define CLK 5
#define DIO 4
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     4
#define DIR_RIGHT 1
#define DIR_LEFT 2
#define DIR_UP 3
#define DIR_DOWN 4
#define INITIAL_MAINLOOP_DELAY 70
#define MIN_MAINLOOP_DELAY 20
#define WORM_MAX_LEN 30
#define RIGHT_BUTTON_PIN 12
#define LEFT_BUTTON_PIN 2
#define BUZZER_PIN 3

struct XY
{
  char x;
  char y;
};

struct EEPROMDATA
{
  unsigned int hscore;
  unsigned int hlevel;
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
TM1637Display tm1637display(CLK, DIO);
bool rightButtonDown = false;
bool leftButtonDown = false;
byte dir = DIR_RIGHT;
char posx = 0;
char posy = 0;
char appleposx = 0;
char appleposy = 0;
byte wormLen = 1;
bool startFromBeginning = true;
unsigned int score = 0;
byte currentScore = 10;
byte level = 1;
byte mainLoopDelay = INITIAL_MAINLOOP_DELAY;
unsigned int mainLoopId = 0;
byte resetCount = 0;
byte livesLeft = 3;
EEPROMDATA edata{0,1};
XY xy{0,0};
LinkedList<XY> linkedlist;

void setup() 
{   
  pinMode(RIGHT_BUTTON_PIN, INPUT);
  pinMode(LEFT_BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  tm1637display.clear();
  tm1637display.setBrightness(0x08);
  
  EEPROM.get(0, edata); 

  if (edata.hscore > 10000) {
    edata.hscore = 0;
    EEPROM.put(0, edata);
  }

  if (edata.hlevel > 10000) {
    edata.hlevel = 1;
    EEPROM.put(0, edata);
  }
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {   
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();  
  display.display();

  tone(BUZZER_PIN,450,200);
  delay(400);
  tone(BUZZER_PIN,450,200);
  delay(200);
  tone(BUZZER_PIN,900,500);
    
  mainLoopId = taskManager.scheduleFixedRate(INITIAL_MAINLOOP_DELAY, mainLoop);
  taskManager.scheduleFixedRate(10, rightSwitchLoop);
  taskManager.scheduleFixedRate(10, leftSwitchLoop);  
}

void loop() 
{  
  taskManager.runLoop();
}

void setMainLoopRunning()
{
  if (startFromBeginning) {
    mainLoopId = taskManager.scheduleFixedRate(INITIAL_MAINLOOP_DELAY, mainLoop);
  }
  else {  
    mainLoopId = taskManager.scheduleFixedRate(mainLoopDelay, mainLoop);
  }
}

void mainLoop()
{ 
  start();  
  setPosition();
  handleCollision();  
  drawWorm();
}

void rightSwitchLoop()
{
  if (startFromBeginning)
    return;
    
  if (digitalRead(RIGHT_BUTTON_PIN)) {
    if (!rightButtonDown) {
      if (dir == DIR_RIGHT)
        dir = DIR_DOWN;
      else if (dir == DIR_DOWN)
        dir = DIR_LEFT;
      else if (dir == DIR_LEFT)
        dir = DIR_UP;
      else
        dir = DIR_RIGHT;  

      if (currentScore > 1)
        currentScore--;

      tone(BUZZER_PIN,300,30);  
    }    
    rightButtonDown = true;    
  }
  else
    rightButtonDown = false;  
}

void leftSwitchLoop()
{
  if (startFromBeginning)
    return;
    
  if (digitalRead(LEFT_BUTTON_PIN)) {
    if (!leftButtonDown) {
      if (dir == DIR_RIGHT)
        dir = DIR_UP;
      else if (dir == DIR_DOWN)
        dir = DIR_RIGHT;
      else if (dir == DIR_LEFT)
        dir = DIR_DOWN;
      else
        dir = DIR_LEFT;  

      if (currentScore > 1)
        currentScore--;

      tone(BUZZER_PIN,300,30);
      resetCount = 0; // reset high sore done only with right button
    }    
    leftButtonDown = true;    
  }
  else
    leftButtonDown = false;  
}

void start()
{
  if (startFromBeginning) {
    tm1637display.clear();
    display.clearDisplay();    
    display.setTextSize(3); 
    display.setTextColor(WHITE);
    display.setCursor(30, 0);
    display.println(F("Mato"));
    display.setTextSize(1);
    display.setCursor(107, 13);
    display.println(F("by"));
    display.setCursor(10, 27);
    display.println(F("Tero Hiltunen 2020"));
    display.setCursor(10, 43);
    display.println(F("Highest score:"));
    display.setCursor(97, 43);
    display.println(edata.hscore);
    display.setCursor(10, 55);
    display.println(F("Highest level:"));
    display.setCursor(97, 55);
    display.println(edata.hlevel);   
    display.display();  
    
    while (!digitalRead(RIGHT_BUTTON_PIN) && !digitalRead(LEFT_BUTTON_PIN)) {      
    }    

    delay(500);
    
    display.clearDisplay();
    display.setTextSize(1); 
    display.setTextColor(WHITE);
    display.setCursor(25, 33);
    display.println(F("left -> fast"));
    display.setCursor(25, 45);
    display.println(F("right -> slow"));
    display.display();

    bool bLeftButton = false;
    bool bRightButton = false;
    
    do {
      bLeftButton = digitalRead(LEFT_BUTTON_PIN);
      bRightButton = digitalRead(RIGHT_BUTTON_PIN);       
    }
    while (!bRightButton && !bLeftButton);
    
    delay(500);
    
    display.clearDisplay();
    tm1637display.showNumberDec(0, false);
    tm1637display.showNumberDec(0, false);
    drawApple();  
    display.display();

    startFromBeginning = false;

    if (bLeftButton) { // speed up worm if left button is pressed 
      mainLoopDelay -= 20;
      taskManager.cancelTask(mainLoopId);
      taskManager.scheduleOnce(500, setMainLoopRunning);        
    }
  }
}

void drawBigPixel(const byte &x, const byte &y, const byte &color)
{
  display.drawPixel((x*4), (y*4), color);
  display.drawPixel((x*4)+1, (y*4), color);
  display.drawPixel((x*4), (y*4)+1, color);
  display.drawPixel((x*4)+1, (y*4)+1, color);

  display.drawPixel((x*4)+2, (y*4), color);
  display.drawPixel((x*4)+3, (y*4), color);
  display.drawPixel((x*4)+2, (y*4)+1, color);
  display.drawPixel((x*4)+3, (y*4)+1, color);
  
  display.drawPixel((x*4)+2, (y*4)+2, color);
  display.drawPixel((x*4)+3, (y*4)+2, color);
  display.drawPixel((x*4)+2, (y*4)+3, color);
  display.drawPixel((x*4)+3, (y*4)+3, color);  

  display.drawPixel((x*4), (y*4)+2, color);
  display.drawPixel((x*4)+1, (y*4)+2, color);
  display.drawPixel((x*4), (y*4)+3, color);
  display.drawPixel((x*4)+1, (y*4)+3, color); 
}

bool getBigPixel(const byte &x, const byte &y)
{  
  return (display.getPixel((x*4)+3, (y*4)) || display.getPixel((x*4), (y*4)+3));  
}

void setPosition()
{
  if (dir == DIR_RIGHT) {
    posx++;
    if (posx > 31)
      posx = 0;
  }
  else if (dir == DIR_LEFT) {
    posx--;
    if (posx < 0)
      posx = 31;
  }
  else if (dir == DIR_UP) {
    posy--;
    if (posy < 0)
      posy = 15;
  }
  else {
    posy++;
    if (posy > 15)
      posy = 0;
  }  
}

// reset highscore if there are 10 one point scores in a row in leve 1 using only right button.
void handleHighScoreReset()
{
  if (level > 1) // only in level 1
    return;
    
  if (currentScore == 1) {    
    resetCount++;
    if (resetCount == 10) {
      edata.hscore = 0;
      edata.hlevel = 1;      
      EEPROM.put(0, edata);
      resetValues(true);
    }
  }
  else
    resetCount = 0;
}

void gameOver()
{    
  display.clearDisplay();
  display.setTextSize(2);             
  display.setTextColor(WHITE);        
  display.setCursor(7,5);             
  display.println(F("Game over!"));
  display.setCursor(7,30);    
  display.setTextSize(1);
    
  if (score > edata.hscore) {
    edata.hscore = score;
    EEPROM.put(0, edata);
    
    display.println(F("NEW HIGH SCORE!:"));
    display.setCursor(7,42);
    display.println(score);
    display.setCursor(7,56);
    display.println(F("Level:"));
    display.setCursor(45,56);
    display.println(level);
  }
  else {
    display.println(F("Score: "));
    display.setCursor(45,30);
    display.println(score);
    display.setCursor(7,47);
    display.println(F("Level:"));
    display.setCursor(45,47);
    display.println(level);    
  }

  display.display();

  if (level > edata.hlevel) {
    edata.hlevel = level;
    EEPROM.put(0, edata);    
  }
  
  for (int i = 1000;i > 0;i=(i-10)) {
    tone(BUZZER_PIN,i,20);
    delay(20);
  }
  
  while (!digitalRead(RIGHT_BUTTON_PIN) && !digitalRead(LEFT_BUTTON_PIN)) {
  }

  delay(500);
   
  resetValues(true);          
}

void lifeLoss()
{
  flashScreen();  
  livesLeft--;

  if (livesLeft == 0) {    
    gameOver();
    return;  
  }  
    
  display.clearDisplay();
  display.setTextSize(1);             
  display.setTextColor(WHITE);        
  display.setCursor(30,25);
  
  if (livesLeft == 2)             
    display.println(F("2 lives left"));
  else if (livesLeft == 1)
    display.println(F("1 life left"));
  
  display.display();
  
  delay(3000);

  display.clearDisplay();
  drawLevel();
  drawApple();  
  display.display();
   
  resetValues(false);          
}

void flashScreen()
{
  for (int i=0;i<15;i++) {
    display.invertDisplay(true);
    display.display();
    tone(BUZZER_PIN,200,50);
    delay(50);  
    display.invertDisplay(false);
    display.display();
    tone(BUZZER_PIN,100,50);
    delay(50);      
  }  
}

void handleCollision()
{
  if ((posx == appleposx) && (posy == appleposy)) {    
    tone(BUZZER_PIN,700,200);
    
    if (wormLen < WORM_MAX_LEN)
      wormLen++;

    handleHighScoreReset();

    //bonus points if only 2 or less turns
    if (currentScore >= 8) {
      tone(BUZZER_PIN,900,200);
      currentScore += 5;
    }
      
    score += currentScore;
    currentScore = 10;
        
    tm1637display.showNumberDec(score, false);

    if (wormLen == WORM_MAX_LEN) {
      nextLevel();
    }
    
    drawApple();
  }
  else if (getBigPixel(posx,posy)) {
    lifeLoss();  
  }
}

void drawApple()
{
  bool clearSpace = true;
    
  do {
    appleposx = random(0,31);
    appleposy = random(0, 15);

    if (getBigPixel(appleposx,appleposy)) {
      clearSpace = false;
    }
    else {
      clearSpace = true;
      drawBigPixel(appleposx, appleposy, WHITE);
    }
  }
  while (clearSpace == false);
}

void drawWorm()
{
  if (startFromBeginning)
    return;
  
  drawBigPixel(posx, posy, WHITE);
   
  xy.x = posx;
  xy.y = posy;

  linkedlist.InsertHead(xy);

  if (linkedlist.GetSize() > wormLen) {
    xy = linkedlist.GetTail();  
    drawBigPixel(xy.x, xy.y, BLACK);
    linkedlist.RemoveTail();           
  }
  
  display.display();
}

void resetValues(bool fullReset)
{  
  resetCount = 0;  
  wormLen = 1;
  linkedlist.Clear();
  dir = DIR_RIGHT;
  posx = 0;
  posy = 0;
  currentScore = 10;  

  if (fullReset) {
    startFromBeginning = true;
    score = 0;
    level = 1;
    livesLeft = 3;
    mainLoopDelay = INITIAL_MAINLOOP_DELAY;
    taskManager.cancelTask(mainLoopId);
    taskManager.scheduleOnce(500, setMainLoopRunning); 
  }
}

void nextLevel()
{
  level++;

  //bonus for finishing the level
  score += (level * 50);
  tm1637display.showNumberDec(score, false);

  resetValues(false);
  
  display.clearDisplay();
  display.setTextSize(2);             
  display.setTextColor(WHITE);        
  display.setCursor(40,5);             
  display.println(F("Level"));

  if (level < 10)
    display.setCursor(60,30);             
  else
    display.setCursor(57,30);
  
  display.println(level);

  if (level > edata.hlevel) {
    display.setTextSize(1);             
    display.setTextColor(WHITE);        
    display.setCursor(10,55);             
    display.println(F("NEW HIGHEST LEVEL!"));
  }
  
  display.display();

  for (int i = 200;i < 1000;i=(i+20)) {
    tone(BUZZER_PIN,i,20);
    delay(20);
  }
  tone(BUZZER_PIN,1000,700);
  
  delay(2000);
  display.clearDisplay();  
  display.display();

  drawLevel();
  
  if (mainLoopDelay > MIN_MAINLOOP_DELAY)
    mainLoopDelay = mainLoopDelay - 5;

  taskManager.cancelTask(mainLoopId);
  taskManager.scheduleOnce(500, setMainLoopRunning);  
}

void drawLevel()
{
  if (level > 1) {
    for (byte i=5;i<10;i++) {
      drawBigPixel(15, i, WHITE);
      drawBigPixel(16, i, WHITE);
    }

    if (level > 2) {
      for (byte i=23;i<28;i++) {
        drawBigPixel(i, 7, WHITE);
        drawBigPixel(i, 8, WHITE);
      }  

      if (level > 3) {
        for(byte i=4;i<9;i++) {
          drawBigPixel(i, 7, WHITE);
          drawBigPixel(i, 8, WHITE);
        }

        if (level > 4) {
          for (byte i=5;i<10;i++) {
            drawBigPixel(13, i, WHITE);
            drawBigPixel(14, i, WHITE);
            drawBigPixel(17, i, WHITE);
            drawBigPixel(18, i, WHITE);
          }
          if (level > 5) {
            for (byte i=23;i<28;i++) {
              drawBigPixel(i, 12, WHITE);
              drawBigPixel(i, 13, WHITE);
            }

            if (level > 6) {
              for (byte i=4;i<9;i++) {
                drawBigPixel(i, 3, WHITE);
                drawBigPixel(i, 2, WHITE);
              }  
            }
          }
        }
      }
    }
  }
}
