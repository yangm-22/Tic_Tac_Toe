#include "mbed.h"
#include "LCD_DISCO_F429ZI.h"
#include "TS_DISCO_F429ZI.h"
#include "pitches.h"
#include <stdio.h> 
#define LCD_WIDTH 240
#define LCD_HEIGHT 320
#define music 0x01
#define COMMON_ANODE

using namespace std;

LCD_DISCO_F429ZI LCD;
TS_DISCO_F429ZI TS;

PwmOut piezo(PA_6);
InterruptIn user_button(BUTTON1);
PwmOut led_red(PE_8);
PwmOut led_green(PE_12);
PwmOut led_blue(PE_14);

Ticker piezoTicker;

Thread t1(osPriorityBelowNormal); // for playing music
Thread t2(osPriorityBelowNormal); // for flashing RGB LED
EventFlags evt;

// function prototypes for states
void state1(void); // home screen (play button)
void state2(void); // player 1 moves
void state3(void); // player 2 moves
void state4(void); // player 1 wins
void state5(void); // player 2 wins
void state6(void); // draw
void initializeSM(void);
void update_lcd_main(); // home screen
void update_lcd_game(); // game screen
bool isBoxEmpty(int i); // check if box empty
int win(void); // check if a player won
void update_lcd_p1Win(void);
void update_lcd_p2Win(void); 
void update_lcd_draw(void);
void StopBuzzer(void); // stop music
void BuzzHz(int freq);
void playSong(void); // to play songs
void UserButtonISR(void); // to turn on or off song that is playing 
void thread1(void); // for playing song
void thread2(void); // for RGB LED
void setColour(void); // set colour of RGB LED

// enumerate states
typedef enum {STATE_1=0, STATE_2, STATE_3, STATE_4, STATE_5, STATE_6} State_Type;

// define a table of pointers to functions for each state
static void (*state_table[]) (void) = {state1, state2, state3, state4, state5, state6};
static State_Type curr_state;

// global variables
TS_StateTypeDef TS_State;
uint16_t tsX, tsY;
char board[] = { ' ', ' ', ' ',
                ' ', ' ', ' ', 
                ' ', ' ', ' '};
int turns = 0;
bool isSongPlaying = true;

// notes in the melody for Harry Potter Theme Song
int melody[] = {
    REST, NOTE_D4, NOTE_G4, NOTE_AS4, NOTE_A4, NOTE_G4, NOTE_D5, NOTE_C5, NOTE_A4, NOTE_G4, NOTE_AS4, NOTE_A4, NOTE_F4, NOTE_GS4, NOTE_D4, NOTE_D4, NOTE_G4, NOTE_AS4, NOTE_A4, NOTE_G4, NOTE_D5, NOTE_F5, NOTE_E5, NOTE_DS5, NOTE_B4, NOTE_DS5, NOTE_D5, NOTE_CS5, NOTE_CS4, NOTE_B4, NOTE_G4, NOTE_AS4, NOTE_D5, NOTE_AS4, NOTE_D5, NOTE_AS4,
    NOTE_DS5, NOTE_D5, NOTE_CS5, NOTE_A4, NOTE_AS4, NOTE_D5, NOTE_CS5, NOTE_CS4, NOTE_D4, NOTE_D5, REST, NOTE_AS4, NOTE_D5, NOTE_AS4, NOTE_D5, NOTE_AS4, NOTE_F5, NOTE_E5, NOTE_DS5, NOTE_B4, NOTE_DS5, NOTE_D5, NOTE_CS5, NOTE_CS4, NOTE_AS4, NOTE_G4
};

// note durations: 4 = quarter note, 8 = eighth note, etc.
int durations[] = {
    2, 4, 4, 8, 4, 2, 4, 2, 2, 4, 8, 4, 2, 4, 1, 4, 4, 8, 4, 2, 4, 2, 4, 2, 4, 4, 8, 4, 2, 4, 1, 4, 2, 4, 2, 4, 2, 4, 2, 4, 4, 8, 4, 2, 4, 1, 4, 4, 2, 4, 2, 4, 2, 4, 2, 4, 4, 8, 4, 2, 4, 1, 0
};


// main() runs in its own thread in the OS
int main() {
    piezo = 0;
    user_button.fall(&UserButtonISR);
    initializeSM();
    LCD.SetFont(&Font24);
    LCD.SetTextColor(LCD_COLOR_DARKBLUE);
    TS.Init(LCD_WIDTH, LCD_HEIGHT);

    t1.start(thread1);
    t2.start(thread2);

    led_red.period_us(255);
    led_green.period_us(255);
    led_blue.period_us(255);

    while (true) {
        state_table[curr_state]();
        ThisThread::sleep_for(100ms);
    }
}


void update_lcd_main() {
    LCD.Clear(LCD_COLOR_WHITE);
    uint8_t text[30]; 
    sprintf((char *)text, "Welcome to"); 
    LCD.DisplayStringAt(0, 40, (uint8_t *)&text, CENTER_MODE);
    sprintf((char *)text, "Tic-Tac-Toe!"); 
    LCD.DisplayStringAt(0, 80, (uint8_t *)&text, CENTER_MODE);

    LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75); // making PLAY button
    sprintf((char *)text, "PLAY"); 
    LCD.SetTextColor(LCD_COLOR_WHITE);
    LCD.SetBackColor(LCD_COLOR_DARKBLUE);
    LCD.DisplayStringAt(0.04*LCD_WIDTH/2, 1.75*LCD_HEIGHT/3, (uint8_t *)&text, CENTER_MODE);
    LCD.SetTextColor(LCD_COLOR_DARKBLUE);
    LCD.SetBackColor(LCD_COLOR_WHITE);
}

void update_lcd_game() {
    LCD.Clear(LCD_COLOR_WHITE);
    
    // making boxes for grid (row 1)
    LCD.DrawRect(0, 40, LCD_WIDTH/3, LCD_WIDTH/3); // index: 0
    LCD.DrawRect(LCD_WIDTH/3, 40, LCD_WIDTH/3, LCD_WIDTH/3); // index: 1
    LCD.DrawRect(2*LCD_WIDTH/3, 40, LCD_WIDTH/3, LCD_WIDTH/3); // index: 2
    // making boxes for grid (row 2)
    LCD.DrawRect(0, 40+LCD_WIDTH/3, LCD_WIDTH/3, LCD_WIDTH/3); // index: 3
    LCD.DrawRect(LCD_WIDTH/3, 40+LCD_WIDTH/3, LCD_WIDTH/3, LCD_WIDTH/3); // index: 4
    LCD.DrawRect(2*LCD_WIDTH/3, 40+LCD_WIDTH/3, LCD_WIDTH/3, LCD_WIDTH/3); // index: 5
    // making boxes for grid (row 3)
    LCD.DrawRect(0, 40+2*LCD_WIDTH/3, LCD_WIDTH/3, LCD_WIDTH/3); // index: 6
    LCD.DrawRect(LCD_WIDTH/3, 40+2*LCD_WIDTH/3, LCD_WIDTH/3, LCD_WIDTH/3); // index: 7
    LCD.DrawRect(2*LCD_WIDTH/3, 40+2*LCD_WIDTH/3, LCD_WIDTH/3, LCD_WIDTH/3); // index: 8

    // making grid
    LCD.FillRect(LCD_WIDTH/3-2, 40, 5, LCD_WIDTH); 
    LCD.FillRect(2*LCD_WIDTH/3-2, 40, 5, LCD_WIDTH); 
    LCD.FillRect(0, 40+LCD_WIDTH/3-2, LCD_WIDTH, 5); 
    LCD.FillRect(0, 40+2*LCD_WIDTH/3-2, LCD_WIDTH, 5); 

    for (int i=0 ; i<3 ; i++) {
        for (int j=0 ; j<3 ; j++) {
            if (board[i + 3*j] == 'X') {
                int touchX = i*LCD_WIDTH/3;
                int touchY = j*LCD_WIDTH/3 + 40;

                LCD.DrawLine(20+touchX, 60+touchY-LCD_WIDTH/6, LCD_WIDTH/3-20+touchX, LCD_WIDTH/3+20+touchY-LCD_WIDTH/6); // drawing \ of X
                
                LCD.DrawLine(20+touchX, LCD_WIDTH/3+20+touchY-LCD_WIDTH/6, LCD_WIDTH/3-20+touchX, 60+touchY-LCD_WIDTH/6); // drawing / of X
            }

            if (board[i + 3*j] == 'O') {
                int touchX = i*LCD_WIDTH/3;
                int touchY = j*LCD_WIDTH/3 + 40;
        
                LCD.DrawCircle(LCD_WIDTH/6+touchX, 2*40+touchY-LCD_WIDTH/8-10, LCD_WIDTH/8);
            }
        }
    }

    // check winning conditions 
    if (win() == 1) {
        curr_state = STATE_4;
    }
    if (win() == 2) {
        curr_state = STATE_5;
    }
    if (win() == 3) {
        curr_state = STATE_6;
    }
}

void update_lcd_p1Win() {
    LCD.Clear(LCD_COLOR_WHITE);
    uint8_t text[30]; 
    sprintf((char *)text, "PLAYER 1"); 
    LCD.DisplayStringAt(0, 40, (uint8_t *)&text, CENTER_MODE);
    sprintf((char *)text, "IS THE WINNER!"); 
    LCD.DisplayStringAt(0, 80, (uint8_t *)&text, CENTER_MODE);

    LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75); // making HOME button
    sprintf((char *)text, "HOME"); 
    LCD.SetTextColor(LCD_COLOR_WHITE);
    LCD.SetBackColor(LCD_COLOR_DARKBLUE);
    LCD.DisplayStringAt(0.04*LCD_WIDTH/2, 1.75*LCD_HEIGHT/3, (uint8_t *)&text, CENTER_MODE);
    LCD.SetTextColor(LCD_COLOR_DARKBLUE);
    LCD.SetBackColor(LCD_COLOR_WHITE);
}

void update_lcd_p2Win() {
    LCD.Clear(LCD_COLOR_WHITE);
    uint8_t text[30]; 
    sprintf((char *)text, "PLAYER 2"); 
    LCD.DisplayStringAt(0, 40, (uint8_t *)&text, CENTER_MODE);
    sprintf((char *)text, "IS THE WINNER!"); 
    LCD.DisplayStringAt(0, 80, (uint8_t *)&text, CENTER_MODE);

    LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75); // making HOME button
    sprintf((char *)text, "HOME"); 
    LCD.SetTextColor(LCD_COLOR_WHITE);
    LCD.SetBackColor(LCD_COLOR_DARKBLUE);
    LCD.DisplayStringAt(0.04*LCD_WIDTH/2, 1.75*LCD_HEIGHT/3, (uint8_t *)&text, CENTER_MODE);
    LCD.SetTextColor(LCD_COLOR_DARKBLUE);
    LCD.SetBackColor(LCD_COLOR_WHITE);
}

void update_lcd_draw() {
    LCD.Clear(LCD_COLOR_WHITE);
    uint8_t text[30]; 
    sprintf((char *)text, "DRAW!"); 
    LCD.DisplayStringAt(0, 40, (uint8_t *)&text, CENTER_MODE);

    LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75); // making HOME button
    sprintf((char *)text, "HOME"); 
    LCD.SetTextColor(LCD_COLOR_WHITE);
    LCD.SetBackColor(LCD_COLOR_DARKBLUE);
    LCD.DisplayStringAt(0.04*LCD_WIDTH/2, 1.75*LCD_HEIGHT/3, (uint8_t *)&text, CENTER_MODE);
    LCD.SetTextColor(LCD_COLOR_DARKBLUE);
    LCD.SetBackColor(LCD_COLOR_WHITE);
}


// check if box is empty (return true if empty + false if filled)
bool isBoxEmpty(int i) {
    if (board[i] == ' ') {
        return true;
    }
    else {
        return false;
    }
}

// check for player win conditions
int win(void){
    // check for winning conditions or draw conditions
    char winCombos[8][3] = {{board[0],board[1],board[2]}, 
                    {board[3],board[4],board[5]}, 
                    {board[6],board[7],board[8]},
                    {board[0],board[3],board[6]},
                    {board[1],board[4],board[7]},
                    {board[2],board[5],board[8]},
                    {board[0],board[4],board[8]},
                    {board[2],board[4],board[6]}};

    int winner = 0;

    for (int i=0 ; i<9 ; i++) {
        if ((winCombos[i][0] == 'X') && (winCombos[i][1] == 'X') && (winCombos[i][2] == 'X')) {
            winner = 1;
        }
        if ((winCombos[i][0] == 'O') && (winCombos[i][1] == 'O') && (winCombos[i][2] == 'O'))  {
            winner = 2;
        }
    }

    if ((turns == 9) && (winner == 0)) {
        winner = 3;
    }


    return winner;
}

// function to stop the buzzer
void StopBuzzer() {
    piezo = 0;
}

// function to produce diff frequencies
void BuzzHz(int freq) {
    if (freq) {
        piezo.period(1.0/freq);
        piezo = 0.5; // to set the duty cycle
    } else {
        piezo = 0;
    }
}

// function to play the song
void playSong() {
    int size = sizeof(durations) / sizeof(int);

    for (int note = 0 ; note < size-1 ; note++) {
        // to calculate note duration => one second divided by note type
        // ex. quarter note = 1000 / 4, eighth note = 1000/8, etc.
        int noteDuration = 1000 / (durations[note]);
        BuzzHz(2*melody[note]);  // multiplied by 2 for octave
        ThisThread::sleep_for(noteDuration);

        StopBuzzer();
        ThisThread::sleep_for(60/132*1000ms);

        if (isSongPlaying == false) {
            StopBuzzer();
            break;
        }
    }
} 

// to pause or play the song in state 1
void UserButtonISR() {
    isSongPlaying = !isSongPlaying;
    if (isSongPlaying == false) {
        StopBuzzer();
        evt.clear(music);
    }
}

// to set the colour of LED
void setColour(int red, int green, int blue) {
#ifdef COMMON_ANODE
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
#endif
    led_red.pulsewidth_us(red);
    led_green.pulsewidth_us(green);
    led_blue.pulsewidth_us(blue);
}


// threads
// thread for playing music
void thread1() {
    while (true) {
        evt.wait_any(music, osWaitForever);
        if (isSongPlaying == true) {
            playSong();
        }
        evt.clear(music); // clear event flag
    }
}

// thread for RGB LED
void thread2() {
    while (true) {
        if (curr_state == STATE_1) {
            setColour(255, 0, 0); // red
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
            setColour(255, 165, 0); // orange r: 255, g: 165, b: 0
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
            setColour(255, 225, 0); // yellow r: 255, g: 255, b: 0
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
            setColour(0, 255, 0); // green
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 255); // blue
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
            setColour(127, 0, 255); // violet r: 127, g: 0, b: 255)
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);

        }
        
        // flash pink for player 1 turn or player 1 win 
        else if ((curr_state == STATE_2) || (curr_state == STATE_4)) {
            setColour(255, 0, 255);
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
        }

        // flash blue for player 2 or player 2 win
        else if ((curr_state == STATE_3) || (curr_state == STATE_5)) {
            setColour(0, 0, 255);
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
        }
        
        // flash red for draw
        else if (curr_state == STATE_6) {
            setColour(255, 0, 0);
            ThisThread::sleep_for(250ms);
            setColour(0, 0, 0);
            ThisThread::sleep_for(250ms);
        }
    }

}

// STATES
void initializeSM(void) {
    curr_state = STATE_1;
    isSongPlaying = true;
}

// state 1 -- home screen
void state1(void) {
    update_lcd_main();
    TS.GetState(&TS_State);
    evt.set(music);

    // clear game board
    for (int i=0 ; i<9 ; i++) {
        board[i] = ' ';
    }

    turns = 0;

    if (TS_State.TouchDetected) {
        tsX = TS_State.X;
        tsY = 320 - TS_State.Y;

        // detect PLAY button touch
        if ((tsX >= 0.5*LCD_WIDTH/2) && (tsX <= 0.5*LCD_WIDTH/2 + LCD_WIDTH/2) && (tsY >= 1.5*LCD_HEIGHT/3) && (tsY <= 1.5*LCD_HEIGHT/3 + 75)) {
            curr_state = STATE_2;
            LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75);
        }
    }
}

// state 2 -- player 1 turn 
void state2(void) { 
    update_lcd_game();
    isSongPlaying = false;
    evt.clear(music);

    uint8_t text[28]; 
    sprintf((char *)text, "PLAYER 1 TURN"); 
    LCD.DisplayStringAt(0, 10, (uint8_t *)&text, CENTER_MODE);

    TS.GetState(&TS_State);
    if (TS_State.TouchDetected) {

        tsX = TS_State.X;
        tsY = 320 - TS_State.Y;

        // detect which box is clicked
        for (int i=0 ; i<3 ; i++) {
            for (int j=0 ; j<3 ; j++) {
                int touchX = i*LCD_WIDTH/3;
                int touchY = j*LCD_WIDTH/3 + 40;
        
                if ((tsX >= touchX) && (tsX <= touchX + LCD_WIDTH/3) && (tsY >= j*LCD_WIDTH/3 + 40) && (tsY <= j*LCD_WIDTH/3 + 40 + LCD_WIDTH/3)) {
                    // check if box is empty
                    if (isBoxEmpty(i + 3*j) == true) {
                        // drawing X
                        LCD.DrawLine(20+touchX, 60+touchY-LCD_WIDTH/6, LCD_WIDTH/3-20+touchX, LCD_WIDTH/3+20+touchY-LCD_WIDTH/6); // drawing \ of X
                        LCD.DrawLine(20+touchX, LCD_WIDTH/3+20+touchY-LCD_WIDTH/6, LCD_WIDTH/3-20+touchX, 60+touchY-LCD_WIDTH/6); // drawing / of X

                        board[i + 3*j] = 'X'; // update array with X
                        turns++;
                        curr_state = STATE_3; // player 2 turn 
                    }
                }
            }
        }
    }
}

// state 3 -- player 2 turn
void state3(void) { 
    update_lcd_game();

    uint8_t text[28]; 
    sprintf((char *)text, "PLAYER 2 TURN"); 
    LCD.DisplayStringAt(0, 10, (uint8_t *)&text, CENTER_MODE);

    TS.GetState(&TS_State);
    if (TS_State.TouchDetected) {

        tsX = TS_State.X;
        tsY = 320 - TS_State.Y;

        // detect which box is clicked
        for (int i=0 ; i<3 ; i++) {
            for (int j=0 ; j<3 ; j++) {
                int touchX = i*LCD_WIDTH/3;
                int touchY = j*LCD_WIDTH/3 + 40;
        
                if ((tsX >= touchX) && (tsX <= touchX + LCD_WIDTH/3) && (tsY >= j*LCD_WIDTH/3 + 40) && (tsY <= j*LCD_WIDTH/3 + 40 + LCD_WIDTH/3)) {
                    // check if box is empty
                    if (isBoxEmpty(i + 3*j) == true) {
                        // drawing O
                        LCD.DrawCircle(LCD_WIDTH/6+touchX, 2*40+touchY-LCD_WIDTH/8-10, LCD_WIDTH/8);

                        board[i + 3*j] = 'O'; // update array with O
                        turns++;
                        curr_state = STATE_2; // player 1 turn 
                    
                    }
                }
            }
        }
    }
}

// state 4 -- player 1 wins
void state4(void) { 
    update_lcd_p1Win();
    TS.GetState(&TS_State);
    if (TS_State.TouchDetected) {
        tsX = TS_State.X;
        tsY = 320 - TS_State.Y;

        // detect HOME button touch
        if ((tsX >= 0.5*LCD_WIDTH/2) && (tsX <= 0.5*LCD_WIDTH/2 + LCD_WIDTH/2) && (tsY >= 1.5*LCD_HEIGHT/3) && (tsY <= 1.5*LCD_HEIGHT/3 + 75)) {
            curr_state = STATE_1;
            LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75);
            isSongPlaying = true;
        }
    }
}

// state 5 -- player 2 wins
void state5(void) {
    update_lcd_p2Win();
    TS.GetState(&TS_State);
    if (TS_State.TouchDetected) {
        tsX = TS_State.X;
        tsY = 320 - TS_State.Y;

        // detect HOME button touch
        if ((tsX >= 0.5*LCD_WIDTH/2) && (tsX <= 0.5*LCD_WIDTH/2 + LCD_WIDTH/2) && (tsY >= 1.5*LCD_HEIGHT/3) && (tsY <= 1.5*LCD_HEIGHT/3 + 75)) {
            curr_state = STATE_1;
            LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75);
            isSongPlaying = true;
        }
    }
}

// state 6 -- draw
void state6(void) { 
    update_lcd_draw();
    TS.GetState(&TS_State);
    if (TS_State.TouchDetected) {
        tsX = TS_State.X;
        tsY = 320 - TS_State.Y;

        // detect HOME button touch
        if ((tsX >= 0.5*LCD_WIDTH/2) && (tsX <= 0.5*LCD_WIDTH/2 + LCD_WIDTH/2) && (tsY >= 1.5*LCD_HEIGHT/3) && (tsY <= 1.5*LCD_HEIGHT/3 + 75)) {
            curr_state = STATE_1;
            LCD.FillRect(0.5*LCD_WIDTH/2, 1.5*LCD_HEIGHT/3, LCD_WIDTH/2, 75);
            isSongPlaying = true;
        }
    }
}
