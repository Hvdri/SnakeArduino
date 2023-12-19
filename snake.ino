#include <LedControl.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

const byte dinPin = 12;
const byte clockPin = 11;
const byte loadPin = 10;
const byte matrixSize = 8;
LedControl lc = LedControl(dinPin, clockPin, loadPin, 1);


const int pinX = A0;
const int pinY = A1;
const int pinSW = 2;
const int pinBuzzer = 13;
const int pinLcdBrightness = 6;

int initialSnakeSize = 1;
int snakeSize = 1;
int snakeX[64];
int snakeY[64];

enum Direction {
  UP,
  DOWN,
  LEFT,
  RIGHT
};
Direction snakeDirection = DOWN;

enum GameState {
  START,
  PLAYING,
  PAUSED,
  GAME_OVER,
  MENU
};
GameState gameState;

int foodX;
int foodY;

unsigned long previousMillis = 0;

const long flickerInterval = 200;
bool foodFlickerState = false;

long interval = 150;
const int minThreshHold = 200;
const int maxThreshHold = 600;

const byte rs = 9;
const byte en = 8;
const byte d4 = 7;
const byte d5 = 3;
const byte d6 = 5;
const byte d7 = 4;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

unsigned long lastButtonPressTime = 0;
unsigned long lastJoystickMoveTime = 0;
const unsigned long debounceDelay = 100;

int selectedMenu = 1;
int selectedOption = 1;
int selectedMenuItem = 1;
int selectedSettingItem = 1;
int totalMainMenuItems = 1;
int totalSettingsMenuItems = 3;
int totalAboutMenuItems = 1;
int totalMenus = 3;
int totalOptions[] = { 1, 3, 1 };

int score = 0;
int lastSecond = 0;
int timeElapsed = 0;
const int second = 1000;

bool canBuzz = true;
byte matrixBrightness = 0;
byte lcdBrightness = 0;

int normalSound = 440;
int foodSound = 880;
int deadSound = 220;

void buzz(int freq) {
  if (canBuzz)
    tone(pinBuzzer, freq, 100);
}

void displayImageInt64(LedControl lc, uint64_t image) {
  for (int i = 0; i < 8; i++) {
    byte row = (image >> i * 8) & 0xFF;
    for (int j = 0; j < 8; j++) {
      lc.setLed(0, i, j, bitRead(row, j));
    }
  }
}

const uint64_t face = 0x0038240000a54200;

void setup() {
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(pinX, INPUT);
  pinMode(pinY, INPUT);
  pinMode(pinLcdBrightness, OUTPUT);
  
  lc.shutdown(0, false);
  lc.clearDisplay(0);
  gameState = START;

  lcd.begin(16, 2);


  EEPROM.get(0, lcdBrightness);
  EEPROM.get(10, matrixBrightness);
  // EEPROM.put(20, true);
  EEPROM.get(20, canBuzz);

  lc.setIntensity(0, matrixBrightness);
  analogWrite(pinLcdBrightness, lcdBrightness);


  selectedSettingItem = 1;

  lastSecond = millis();
  timeElapsed = 0;
  score = 0;

  snakeX[0] = matrixSize / 2;
  snakeY[0] = matrixSize / 2;

  spawnFood();

  Serial.begin(9600);
}

bool displayMenuFlag = true;



void displayMenu() {
  lcd.clear();
  displayImageInt64(lc, face);

  switch (selectedMenu) {
    case 1:
      displayMainMenu();
      break;

    case 2:
      displaySettingsMenu();
      break;

    case 3:
      displayAboutMenu();
      break;
  }

  displayMenuFlag = false;
}

void displayMainMenu() {
  Serial.println("main");

  lcd.clear();
  lcd.print(F("<   Main Menu  >"));
  lcd.setCursor(0, 1);
  lcd.print("1. Start Game");
}

void displaySettingsMenu() {
  Serial.println("settings");
  lcd.clear();
  lcd.print(F("<   Settings   >"));

  switch (selectedSettingItem) {
    case 1:
      lcd.setCursor(0, 1);
      lcd.print("1. LCD Brightness");
      break;

    case 2:

      lcd.setCursor(0, 1);
      lcd.print("2. Matrix Brightness");
      break;

    case 3:

      lcd.setCursor(0, 1);
      lcd.print("3. Sound On/Off");
      break;
  }
}

void displayAboutMenu() {
  Serial.println("about");

  lcd.clear();
  lcd.print(F("<    About     >"));
  lcd.setCursor(0, 1);
  lcd.print(" Creator: Horia");
}

void loop() {
  readJoystick();

  switch (gameState) {
    case MENU:
      if (displayMenuFlag) {
        displayMenu();
        Serial.println("menu");
        displayMenuFlag = false;
      }
      break;

    case START:
      Serial.println("start");
      lcd.print(F("   Snake Game   "));
      lcd.setCursor(0, 1);
      lcd.print(F(" Press Joystick "));
      break;

    case PLAYING:
      Serial.println("playing");
      lcd.setCursor(0, 0);
      lcd.print((String) "Score:" + score);
      lcd.setCursor(0, 1);
      lcd.print((String) "Time:" + timeElapsed);

      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        updateSnake();

        if (checkCollision()) {
          endOfGame();
          snakeSize = initialSnakeSize;
          return;
        }

        if (snakeX[0] == foodX && snakeY[0] == foodY) {
          buzz(foodSound);
          snakeSize++;
          if (interval - 5 > 0)
            interval -= 5;
          score += 10;
          spawnFood();
        }

        displaySnake();
        displayFood();

        if (millis() - lastSecond > second) {
          timeElapsed++;
          lastSecond = millis();
        }
      }
      break;

    case PAUSED:
      lcd.setCursor(0, 0);
      lcd.print((String) "Score:" + score + "    Game");
      lcd.setCursor(0, 1);
      lcd.print((String) "Time:" + timeElapsed + "    Paused");
      break;
  }
}



void readJoystick() {
  static bool buttonPressed = false;
  static unsigned long lastMenuChange = 0;

  int swState = digitalRead(pinSW);
  int xValue = analogRead(pinX);
  int yValue = analogRead(pinY);

  // Check if the button is pressed
  if (swState == LOW && !buttonPressed) {
    buttonPressed = true;
    buzz(normalSound);

    if (gameState == START) {
      gameState = MENU;
      lcd.clear();
      selectedMenuItem = 1;
      selectedOption = 1;
      displayMenuFlag = true;

    } else if (gameState == PLAYING) {
      gameState = PAUSED;
      lcd.setCursor(0, 0);
      lcd.print((String) "Score:" + score);
      lcd.setCursor(0, 1);
      lcd.print((String) "Time:" + timeElapsed);

      lcd.setCursor(9, 0);
      lcd.print((String) "| Game");
      lcd.setCursor(9, 1);
      lcd.print((String) "|Paused");

    } else if (gameState == PAUSED) {
      gameState = PLAYING;
      lcd.clear();

    } else if (gameState == GAME_OVER) {
      gameState = MENU;
      lcd.clear();

    } else if (gameState == MENU) {
      if (millis() - lastMenuChange > debounceDelay) {
        // Debounce to avoid rapid menu changes
        lastMenuChange = millis();

        switch (selectedMenu) {
          case 1:
            // Main Menu options
            if (selectedOption == 1) {
              gameState = PLAYING;
              lcd.clear();
            }
            break;

          case 2:
            lcd.clear();
            if (selectedSettingItem == 1) {
              
              lcdBrightness = (lcdBrightness + 85) % 255;
              EEPROM.put(0, lcdBrightness);
              analogWrite(pinLcdBrightness, lcdBrightness);
              Serial.println(lcdBrightness);
              displayMenuFlag = true;

            } else if (selectedSettingItem == 2) {
              matrixBrightness = (matrixBrightness + 5) % 15;
              EEPROM.put(10, matrixBrightness);
              lc.setIntensity(0, matrixBrightness);

              displayMenuFlag = true;
              break;

            } else if (selectedSettingItem == 3) {

              canBuzz = !canBuzz;
              EEPROM.put(20, canBuzz);
              Serial.println(canBuzz);
              displayMenuFlag = true;
            }
            break;

          case 3:
            // About Menu
            break;
        }

        displayMenuFlag = true;
      }
    }
  }

  if (swState == HIGH) {
    buttonPressed = false;
  }

  // left and right
  if (gameState == MENU) {
    if (yValue < minThreshHold) {
      if (millis() - lastMenuChange > debounceDelay) {

        lastMenuChange = millis();

        if (selectedMenu == 1)
          selectedMenu = totalMenus;
        else
          selectedMenu--;

        displayMenuFlag = true;
      }
    } else if (yValue > maxThreshHold) {
      if (millis() - lastMenuChange > debounceDelay) {
        // Debounce to avoid rapid menu changes
        lastMenuChange = millis();

        if (selectedMenu == totalMenus)
          selectedMenu = 1;
        else
          selectedMenu++;

        displayMenuFlag = true;
      }
    }
  }

  // up and down
  if (gameState == MENU) {
    if (xValue < minThreshHold) {
      if (selectedMenu == 2) {


        if (selectedSettingItem == 1)
          selectedSettingItem = 3;
        else
          selectedSettingItem--;

        displayMenuFlag = true;
      }

    } else if (xValue > maxThreshHold) {
      if (selectedMenu == 2) {

        if (selectedSettingItem == 3)
          selectedSettingItem = 1;
        else
          selectedSettingItem++;

        displayMenuFlag = true;
      }
    }
  }

  if (gameState == PLAYING) {
    if (xValue < minThreshHold && snakeDirection != RIGHT) {
      snakeDirection = LEFT;
    } else if (xValue > maxThreshHold && snakeDirection != LEFT) {
      snakeDirection = RIGHT;
    }

    if (yValue < minThreshHold && snakeDirection != DOWN) {
      snakeDirection = UP;
    } else if (yValue > maxThreshHold && snakeDirection != UP) {
      snakeDirection = DOWN;
    }
  }
}

void updateSnake() {
  for (int i = snakeSize - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  switch (snakeDirection) {
    case RIGHT:
      snakeY[0]--;
      break;
    case LEFT:
      snakeY[0]++;
      break;
    case UP:
      snakeX[0]++;
      break;
    case DOWN:
      snakeX[0]--;
      break;
  }

  snakeX[0] = (snakeX[0] + matrixSize) % matrixSize;
  snakeY[0] = (snakeY[0] + matrixSize) % matrixSize;
}

void displaySnake() {
  lc.clearDisplay(0);

  for (int i = 0; i < snakeSize; i++) {
    lc.setLed(0, snakeX[i], snakeY[i], true);
  }
}

void displayFood() {
  flickerFood();

  if (!foodFlickerState) {
    lc.setLed(0, foodX, foodY, true);
  }
}

void flickerFood() {
  unsigned long currentMillis = millis();
  static unsigned long foodFlickerPreviousMillis = 0;

  if (currentMillis - foodFlickerPreviousMillis >= flickerInterval) {
    foodFlickerPreviousMillis = currentMillis;
    foodFlickerState = !foodFlickerState;
  }
}

void spawnFood() {
  foodX = random(matrixSize);
  foodY = random(matrixSize);

  for (int i = 0; i < snakeSize; i++) {
    if (foodX == snakeX[i] && foodY == snakeY[i]) {
      spawnFood();
      return;
    }
  }
}

bool checkCollision() {
  for (int i = 1; i < snakeSize; i++) {
    if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
      return true;
    }
  }
  return false;
}

void endOfGame() {
  gameState = GAME_OVER;
  buzz(deadSound);
  static bool waitForInput = true;

  if (digitalRead(pinSW) == LOW && waitForInput) {

    gameState = MENU;
    lcd.clear();
    selectedMenuItem = 1;
    selectedOption = 1;
    displayMenuFlag = true;
    setup();
    waitForInput = false;

  } else {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print((String) "Score:" + score + "    Game");
    lcd.setCursor(0, 1);
    lcd.print((String) "Time:" + timeElapsed + "    Paused");

    lcd.setCursor(9, 0);
    lcd.print("| Game");
    lcd.setCursor(9, 1);
    lcd.print("| Over!");

    selectedSettingItem = 1;

    lastSecond = millis();
    timeElapsed = 0;
    score = 0;

    snakeX[0] = matrixSize / 2;
    snakeY[0] = matrixSize / 2;
  }
}
