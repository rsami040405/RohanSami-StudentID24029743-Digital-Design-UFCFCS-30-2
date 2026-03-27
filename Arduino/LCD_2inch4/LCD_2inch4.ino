//Libraries
#include <SPI.h>
#include "LCD_Driver.h" 
#include "GUI_Paint.h" 

//Arduino Pin Configuration
#define BUZZER 6
#define BUTTON_PIN 4    
#define POT_PIN A1
#define JOY_Y_PIN A2   

//Game states
enum State { MENU, GAME_SAFE_CRACKER, GAME_CRAPPY, END_MENU };
State gameState = MENU; //Game starts on the main menu

//2 options in main menu
#define NUM_MENU_ITEMS 2
int currentMenuSelection = 0; 

//2 options in end menu
#define NUM_END_MENU_ITEMS 2
int currentEndMenuSelection = 0; 

//Traks game session 0 is safe cracker and 1 is crappy bird
int lastGamePlayed = 0; 
int lastRawPot = -1;
bool safeWon = false; 

//Safe Cracker game logic
#define COMBO_LENGTH 3  
#define MAX_NUMBER 20   
int combination[COMBO_LENGTH]; 
int guess = -1;
int currentStep = 0;           
int currentDialValue = -1; 

//Crappy Bird game logic
#define MAX_PILLARS 1 
#define MAX_PLAYERS 1 

//Pillar LOgic
typedef struct Pillar {  
    int x; 
    int last_x; 
    int height; 
    int gap;    
    bool scored[MAX_PLAYERS]; 
} Pillar;

//PLayer logic
typedef struct Player { 
    int x; 
    int y; 
    int velocity; 
    bool alive; 
    int score; 
    int last_y; 
} Player;

//Screen Scaling
int window_width; 
int window_height; 

//Pixel size 
int ball_size = 4; 
int pillar_width = 2; 

//Initial amount of space between pillars
int base_gap_height = 110; 

//Player and Pillar arrays
Player players[MAX_PLAYERS];
Pillar pillars[MAX_PILLARS];

int crappyScore = 0; 

//Audio Feedback
void playSound(int frequency, int duration) {
  tone(BUZZER, frequency, duration);
}

void setup() {
  Serial.begin(115200); //Serial communication used for debugging
  
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); 

//LCD display driver initialisation
  Config_Init();
  LCD_Init();
  SPI.setClockDivider(SPI_CLOCK_DIV2); 
  LCD_Clear(WHITE);

//Draw main menu after clearing the screen
  Paint_NewImage(LCD_WIDTH, LCD_HEIGHT, 0, WHITE);
  Paint_Clear(WHITE);
  
  drawMenuBase();
}

//Loop for gamestates
void loop() {
  if (gameState == MENU) {
    runMenu();
  } 
  else if (gameState == GAME_SAFE_CRACKER) {
    runSafeCracker();
  }
  else if (gameState == GAME_CRAPPY) {
    runCrappyBird();
  }
  else if (gameState == END_MENU) {
    runEndMenu();
  }
}

//Main menu design
void drawMenuBase() {
  Paint_Clear(WHITE);
  
  Paint_DrawRectangle(5, 5, 235, 45, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawString_EN(45, 15, "MAIN MENU", &Font20, BLUE, WHITE);
  Paint_DrawString_EN(15, 60, "Select a game:", &Font16, WHITE, BLACK);
  
  Paint_DrawString_EN(40, 110, "1. Safe Cracker", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(40, 160, "2. Crappy Bird", &Font16, WHITE, BLACK);
  
  currentMenuSelection = 0; 
  lastRawPot = analogRead(POT_PIN); //prevents menus jumping around due to initialising signal reading
  updateMenuDisplay(currentMenuSelection);
}
//Draws Arrow and plays a sound when different option is moved to
void updateMenuDisplay(int selection) {
  Paint_DrawRectangle(10, 100, 30, 200, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  if (selection == 0) Paint_DrawString_EN(15, 110, ">", &Font16, WHITE, GREEN);
  else if (selection == 1) Paint_DrawString_EN(15, 160, ">", &Font16, WHITE, RED);
  playSound(50, 10); 
}

void runMenu() {
  int rawPot = analogRead(POT_PIN);
  if (abs(rawPot - lastRawPot) > 15) {  //input is only processed if the movment of pot is greater than 15
    lastRawPot = rawPot; 
    int mappedSelection = map(rawPot, 0, 1024, 0, NUM_MENU_ITEMS); //Converts 10bit analog range 0-1023 to menu index 0-1
    if (mappedSelection >= NUM_MENU_ITEMS) mappedSelection = NUM_MENU_ITEMS - 1; //makes it so the selection boundary does not go over 1

//Only redraw screen if the selection has changed
    if (mappedSelection != currentMenuSelection) {
      currentMenuSelection = mappedSelection;
      updateMenuDisplay(currentMenuSelection);
    }
  }

  if (digitalRead(BUTTON_PIN) == LOW) { //check button press
    delay(50); //Software debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      randomSeed(millis());//Asks arduino for a random safe cracking game numbers based on exact millisecond the user presses the button
      
      if (currentMenuSelection == 0) {
        playSound(1000, 100); 
        lastGamePlayed = 0;//Remember game as safe cracker for play again screen
        gameState = GAME_SAFE_CRACKER;
        newSafeCrackerGame(); 
      } else if (currentMenuSelection == 1) {
        playSound(1000, 100); 
        lastGamePlayed = 1;//Remember game as crappy bird for play again screen
        gameState = GAME_CRAPPY;
        newCrappyBirdGame(); 
      }
      while(digitalRead(BUTTON_PIN) == LOW); //Waits for user to let go of button so a double click is not registered
    }
  }
}

// End menu logic
void drawEndMenuBase() {
  Paint_Clear(WHITE);
  if (lastGamePlayed == 0) {
    if (safeWon) { //Safe cracker win drawing
      Paint_DrawRectangle(5, 5, 235, 45, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);
      Paint_DrawString_EN(35, 15, "SAFE CRACKED!", &Font20, GREEN, WHITE);
    } else {//Safe cracker loss drawing
      Paint_DrawRectangle(5, 5, 235, 45, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
      Paint_DrawString_EN(55, 15, "GAME OVER", &Font20, RED, WHITE);
    }
  } else { //Crappy bird game over drawing
    Paint_DrawRectangle(5, 5, 235, 45, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(55, 15, "GAME OVER", &Font20, RED, WHITE);
    
    char buf[20];//Crappy bird score added to the end menu
    sprintf(buf, "Score: %d", crappyScore);
    Paint_DrawString_EN(70, 60, buf, &Font16, WHITE, BLACK);
  }
  //End game options
  Paint_DrawString_EN(15, 85, "What next?", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(40, 125, "1. Play Again", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(40, 175, "2. Main Menu", &Font16, WHITE, BLACK);

  //Reads potentiometer for where the cursor should be
  currentEndMenuSelection = 0; 
  lastRawPot = analogRead(POT_PIN);
  updateEndMenuDisplay(currentEndMenuSelection);
}
//Draws arrow position and colour based on selection in end menu
void updateEndMenuDisplay(int selection) {
  Paint_DrawRectangle(10, 115, 30, 205, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  if (selection == 0) Paint_DrawString_EN(15, 125, ">", &Font16, WHITE, GREEN);
  else if (selection == 1) Paint_DrawString_EN(15, 175, ">", &Font16, WHITE, BLUE);
  playSound(50, 10); 
}

//monitors pot and button in end menu
void runEndMenu() {
  int rawPot = analogRead(POT_PIN);
  if (abs(rawPot - lastRawPot) > 15) {  //only make an input if dial moved more than 15
    lastRawPot = rawPot; 
    int mappedSelection = map(rawPot, 0, 1024, 0, NUM_END_MENU_ITEMS); //Translates 10 bit analog signal of potentiometer to onlt have 2 menu options 0 or 1
    if (mappedSelection >= NUM_END_MENU_ITEMS) mappedSelection = NUM_END_MENU_ITEMS - 1; //does not let menu items go past 1

//Only draw if the cursor for menu options have changed
    if (mappedSelection != currentEndMenuSelection) {
      currentEndMenuSelection = mappedSelection;
      updateEndMenuDisplay(currentEndMenuSelection);
    }
  }

//If button is pressed plays a sound and shuffles randon number generator for a new number
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); 
    if (digitalRead(BUTTON_PIN) == LOW) {
      randomSeed(millis());
      playSound(1000, 100); 

//Relaunches last game that was played      
      if (currentEndMenuSelection == 0) {
        if (lastGamePlayed == 0) {
          gameState = GAME_SAFE_CRACKER;
          newSafeCrackerGame(); 
        } else {
          gameState = GAME_CRAPPY;
          newCrappyBirdGame();
        }
      } else if (currentEndMenuSelection == 1) { //Launches the menu
        gameState = MENU;
        drawMenuBase();
      }
      while(digitalRead(BUTTON_PIN) == LOW); 
    }
  }
}

//Crappy Bird logic
void newCrappyBirdGame() {
  window_width = LCD_WIDTH;
  window_height = LCD_HEIGHT;

  players[0].x = window_width / 4; //Players position on screen
  players[0].y = window_height / 2;
  players[0].last_y = window_height / 2;
  players[0].alive = true;
  players[0].score = 0;
  crappyScore = 0;

  for (int i = 0; i < MAX_PILLARS; i++) { //i is an index for each pillar and it increments each time to get up to the value of max pillars and then stops
      pillars[i].x = window_width + 150; //Puts pillar 150 pixels to the right
      pillars[i].last_x = pillars[i].x;//Code knows where the last pillar was to spawn new ones
      pillars[i].gap = base_gap_height; //Sets the gap in the middle of pillar
      pillars[i].height = random(20, window_height - pillars[i].gap - 20);//Generates random height for top of pillar
      for (int p = 0; p < MAX_PLAYERS; p++) { //Scoring logic to make sure if pillar is passed score increases
          pillars[i].scored[p] = false; //If pillar has not been passed it will not count a point
      }
  }
  Paint_Clear(WHITE);
}

//If player is not alive game goes to end menu
void runCrappyBird() {
  int p = 0; 
  if (!players[p].alive) {
    playSound(200, 500); 
    delay(1000);
    crappyScore = players[p].score; 
    gameState = END_MENU;
    drawEndMenuBase();
    return;
  }

  players[p].last_y = players[p].y; //Stores previous Y position of player for screen clearing
  
  int joyY = analogRead(JOY_Y_PIN); //Read joysticks Y position
  
  if (joyY < 400) { //If joystick is pushed up past 400 character moves if not stays neutral
    players[p].y -= map(joyY, 400, 0, 8, 30); //Further you move the stick up the faster the character moves up
  } else if (joyY > 620) {//If the joystick is pushed down past 620 the character moves down if not it stays neutral
    players[p].y += map(joyY, 620, 1023, 8, 30); //Further you move the stick down the faster the character moves down
  }
  
//Prevents the bird from flying off the screen by clamping y position
  if (players[p].y < ball_size + 2) players[p].y = ball_size + 2;
  if (players[p].y > window_height - ball_size - 2) players[p].y = window_height - ball_size - 2;

//If player scores 5 points the pillars will speed up by 5 pixels. Starting speed is 25
  int current_speed = 25 + ((players[p].score / 5) * 5); 
  if (current_speed > 50) current_speed = 50; //Speed stops at 50 max
  int current_gap = base_gap_height - ((players[p].score / 5) * 8);//Every 5 points the pillar gap decreases by 8 pixels and does not reach below 45 pixel gap total
  if (current_gap < 45) current_gap = 45;

  for (int i = 0; i < MAX_PILLARS; i++) { 
      pillars[i].last_x = pillars[i].x;
      pillars[i].x -= current_speed; 

//If pillars go off screen they will be painted over in white clearing them
      if (pillars[i].x < -pillar_width) { 
          Paint_DrawRectangle(pillars[i].last_x, 0, pillars[i].last_x + pillar_width, pillars[i].height, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL); 
          Paint_DrawRectangle(pillars[i].last_x, pillars[i].height + pillars[i].gap, pillars[i].last_x + pillar_width, window_height, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL); 

//Pillar moved back on the right side of the screen if they have been wiped out          
          pillars[i].x = window_width; 
          pillars[i].last_x = window_width; 
          pillars[i].gap = current_gap; //New gap size and random height for new pillar on the right
          pillars[i].height = random(20, window_height - pillars[i].gap - 20); 
          for (int p_index = 0; p_index < MAX_PLAYERS; p_index++) { //Scoring flag for pillar reset so new pillar can give a point if passed
              pillars[i].scored[p_index] = false; 
          }
      }

      if (!pillars[i].scored[p] && pillars[i].x < players[p].x - ball_size) { //CHecks if the player has passed the pillar and if they havent recieved a point for it
          players[p].score++; //Gives player point if they have not recieved one
          pillars[i].scored[p] = true; //Scored switch = true so no more extra points are added
          playSound(4000, 50); //plays sound when scoring
      }

//IF player and pillar overlap on x axis it will be registered as a hit using the players size and pillar width
      bool hitPillarX = (pillars[i].last_x + pillar_width >= players[p].x - ball_size) && 
                        (pillars[i].x <= players[p].x + ball_size);

//Checks if ball hits top or bottom pillar     
      bool hitTopPillar = (players[p].y - ball_size <= pillars[i].height);
      bool hitBottomPillar = (players[p].y + ball_size >= pillars[i].height + pillars[i].gap);
//If player hits pillar then their alive state is set to false
      if (hitPillarX && (hitTopPillar || hitBottomPillar)) { 
          players[p].alive = false; 
      }
  }
//Checks if bird has moved vertically since the last frame
  if (abs(players[p].y - players[p].last_y) > 0) { //checks if birds moved vertically
    Paint_DrawRectangle(players[p].x - ball_size, players[p].last_y - ball_size, players[p].x + ball_size, players[p].last_y + ball_size, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);//Erase bird by redrawing in white if it has moved
  }
  Paint_DrawRectangle(players[p].x - ball_size, players[p].y - ball_size, players[p].x + ball_size, players[p].y + ball_size, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);//Redraw bird in blue

  for (int i = 0; i < MAX_PILLARS; i++) {
    if (pillars[i].x != pillars[i].last_x) { //only update screen if pillar has moved
        Paint_DrawRectangle(pillars[i].last_x, 0, pillars[i].last_x + pillar_width, pillars[i].height, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL); //Erase top pipe by colouring in white
        Paint_DrawRectangle(pillars[i].last_x, pillars[i].height + pillars[i].gap, pillars[i].last_x + pillar_width, window_height, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL); //Erase bottom pipe by colouring in white
        
        Paint_DrawRectangle(pillars[i].x, 0, pillars[i].x + pillar_width, pillars[i].height, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL); //Redraw Top pipe by colouring in red
        Paint_DrawRectangle(pillars[i].x, pillars[i].height + pillars[i].gap, pillars[i].x + pillar_width, window_height, RED, DOT_PIXEL_1X1, DRAW_FILL_FULL); //Redraw bottom pipe by colouring in red
    }
  }
//Formats and displays integer score as a string for display driver
  char buf[10];
  sprintf(buf, "%d", players[p].score);
  Paint_DrawString_EN(5, 5, buf, &Font20, BLACK, WHITE); 
}

//Safe Cracker logic
void runSafeCracker() {
  int rawPot = analogRead(POT_PIN);//Read current position of potentiometer
  
  if (abs(rawPot - lastRawPot) > 20) { //Threshold of 20 to not change number unless it has moved beyond 20 units
    lastRawPot = rawPot; 
    int mappedDial = map(rawPot, 0, 1024, 0, MAX_NUMBER + 1); //Rescales the 10-bit analog input (0-1023) to the game's number range
    if (mappedDial > MAX_NUMBER) mappedDial = MAX_NUMBER; //Value does not reach above the max for the game which is 20
    if (mappedDial != currentDialValue) { //Only update screen if the potentiometer turns enough to change the number
      currentDialValue = mappedDial;
      updateLiveDialDisplay(currentDialValue); //clears old number and adds new number that is the same as the dial value
    }
  }
  if (digitalRead(BUTTON_PIN) == LOW) {//Checks if button is pressed
    delay(50); //Software debounce for button
    if (digitalRead(BUTTON_PIN) == LOW) {//Rechecks
      guess = currentDialValue; //Guess is based off dials current position
      handleGuess(); 
      while(digitalRead(BUTTON_PIN) == LOW); 
    }
  }
}

void newSafeCrackerGame() {
  currentStep = 0; //1st digit of safe cracking code
  guess = -1; //Previous guess state cleared
  safeWon = false; //Win condition reset

  //Loop that chooses random numbers for safe cracking combination
  for (int i = 0; i < COMBO_LENGTH; i++) combination[i] = random(0, MAX_NUMBER + 1); 
  
  Paint_Clear(WHITE); //clear screen
  drawSafeCrackerUI(); //Draw the static safe cracker ui
  
  lastRawPot = analogRead(POT_PIN); //Reads position of potentiometer when games starts
  currentDialValue = map(lastRawPot, 0, 1024, 0, MAX_NUMBER + 1); //Maps potentiometer to safe combination max number
  if (currentDialValue > MAX_NUMBER) currentDialValue = MAX_NUMBER; //Makes it so dial cannot exceed the max value
  updateLiveDialDisplay(currentDialValue); 
  
  updateFeedbackUI(-1); 
}

//Draws the safecracker game ui
void drawSafeCrackerUI() {
  Paint_DrawRectangle(2, 2, 238, 318, BLACK, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
  Paint_DrawRectangle(5, 5, 235, 45, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawString_EN(35, 15, "SAFE CRACKER", &Font20, BLACK, WHITE);
  Paint_DrawString_EN(15, 110, "Your Guess:", &Font16, WHITE, BLACK);
}

//Updates screen and audio everytime the player changes the dial
void updateLiveDialDisplay(int dialVal) {
  char buffer[10];
  Paint_DrawRectangle(50, 135, 85, 165, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  sprintf(buffer, "%-2d", dialVal);
  Paint_DrawString_EN(50, 135, buffer, &Font24, WHITE, BLACK);
  
  int diff = abs(combination[currentStep] - dialVal); //Calculates the difference from the correct number
  int pitch = map(diff, MAX_NUMBER, 0, 200, 2000); //Maps the distance from correct number to higher pitched when closer and lowe when further away
  
  if (pitch < 200) pitch = 200;
  if (pitch > 2000) pitch = 2000;

  if (diff == 0) {
    playSound(2500, 50); //High sound if you are on the correct number
  } else {
    playSound(pitch, 15); 
  }
}
//process players guess
void handleGuess() {
  int diff = abs(combination[currentStep] - guess); //calculate difference how far number is from the target
  updateFeedbackUI(diff);
  if (diff == 0) {
    currentStep++;
    if (currentStep >= COMBO_LENGTH) {
      Paint_DrawRectangle(10, 240, 180, 280, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
      Paint_DrawString_EN(15, 250, "SAFE OPEN!", &Font20, WHITE, GREEN); //safe open message when have correct combination
      victorySound(); //plays sound
      delay(1500); 
      safeWon = true; 
      gameState = END_MENU; //reverts back to end menu
      drawEndMenuBase();
    } else {
      Paint_DrawRectangle(10, 240, 180, 280, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
      Paint_DrawString_EN(15, 250, "CORRECT!", &Font20, WHITE, GREEN); //Correct guess message displayed 
      
      playSound(1047, 100); delay(120); //correct guess sound
      playSound(1319, 100); delay(120);
      playSound(1568, 200); delay(1200); 

//Reset guess state when correct guess occurs
      Paint_DrawRectangle(10, 240, 180, 280, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
      guess = -1;
      updateFeedbackUI(-1);
    }
  } else { //displays red incorrect when hit incorrect number//noise is displayed too
    Paint_DrawRectangle(10, 240, 180, 280, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(15, 250, "INCORRECT!", &Font20, WHITE, RED);
    playSound(200, 500); 
    delay(1000); 
  
//Game goes into end menu if you have an incorrect guess
    safeWon = false; 
    gameState = END_MENU;
    drawEndMenuBase();
  }
}
//Displays the step in the safe cracking game the player is at
void updateFeedbackUI(int diff) {
  char buffer[30];
  Paint_DrawRectangle(10, 60, 230, 85, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  sprintf(buffer, "Cracking #%d of %d", currentStep + 1, COMBO_LENGTH);
  Paint_DrawString_EN(20, 65, buffer, &Font16, WHITE, BLUE);
}

//Victory Sound for cracked safe
void victorySound() {
  playSound(1047, 150); delay(150);
  playSound(1319, 150); delay(150);
  playSound(1568, 150); delay(150);
  playSound(2093, 400); delay(400);
}