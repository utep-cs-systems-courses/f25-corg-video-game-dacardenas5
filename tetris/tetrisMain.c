#include <msp430.h>
#include <libTimer.h>
#include <stdlib.h>
#include "lcdutils.h"
#include "lcddraw.h"

#define SW1 1
#define SW2 2
#define SW3 4
#define SW4 8
#define SWITCHES 15

#define BLOCK_SIZE 8
#define BOARD_COLS 10  //standard display
#define BOARD_ROWS 20  //standrd display
#define BOARD_OFFSET_X 24 //hidden rows allows for correct updates for tetris

#define COLOR_I     COLOR_CYAN     
#define COLOR_O     COLOR_YELLOW  
#define COLOR_T     COLOR_PURPLE  
#define COLOR_S     COLOR_GREEN   
#define COLOR_Z     COLOR_RED     
#define COLOR_J     COLOR_BLUE    
#define COLOR_L     COLOR_ORANGE

char grid[BOARD_ROWS][BOARD_COLS] = {0}; //this is the grid that stores if the place is occupied

typedef struct {
  char shape [4][4][4]; //each peice has a shape wit curr  rotation, row and col
  char nrot; //rotaion logic based on piece, how many diff rotations
  unsigned short colo; //piece color
} Tetromino;

const Tetromino tetrominoes[7] = {//Seven unique pieces with amount of unique rotations + color

  // I
  { {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
     {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}}}, 2, COLOR_I },

  // O
  { {{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}}, 1, COLOR_O },

  // T
  { {{{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
     {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}}, 4, COLOR_T },

  // L
  { {{{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
     {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}}, 4, COLOR_L },

  // J
  { {{{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
     {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}}, 4, COLOR_J },

  // S
  { {{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
     {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
     {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}}}, 2, COLOR_S },

  // Z
  { {{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
     {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}}, 2, COLOR_Z }
};

Tetromino current; //current piece that user is contorlloing

int px = 3, py = -2; //starting location of the tetrimino (its within the hidden rows)
int rotation = 0; //starting rotation

char switches = 0;

short redrawScreen = 1;

static char switch_update_interrupt_sense() {
  char p2val = P2IN;
  P2IES |= (p2val & SWITCHES);   // if switch up, sense down
  P2IES &= (p2val | ~SWITCHES);  // if switch down, sense up
  return p2val;
}

void switch_init() {
  P2REN |= SWITCHES;    // enable resistors
  P2IE |= SWITCHES;     // enable interrupts
  P2OUT |= SWITCHES;    // pull-ups
  P2DIR &= ~SWITCHES;   // inputs
  switch_update_interrupt_sense();
}



void switch_interrupt_handler() {
  char p2val = switch_update_interrupt_sense();
  switches = ~p2val & SWITCHES;
}

void draw_block(int gx,int gy,unsigned short color) {
  int x=BOARD_OFFSET_X+gx*BLOCK_SIZE;
  int y=BOARD_OFFSET_Y+gy*BLOCK_SIZE;
  fillRectangle(x,y,BLOCK_SIZE,BLOCK_SIZE,color);
}

void draw_grid() {
  for(int r=0;r<BOARD_ROWS;r++){
    for(int c=0;c<BOARD_COLS;c++){
      draw_block(c,r,grid[r][c]?COLOR_BLOCK:COLOR_EMPTY);
    }
  }
}

void draw_current_piece(){
  for(int r=0;r<4;r++){
    for(int c=0;c<4;c++){
      if(current.shape[rotation][r][c]){
	int x = px +c;
	int y = py +r;
	if(y>=0){
	  draw_block(x,y,current.color);
	}
      }
    }
  }
}

int collides(int nx,int ny,int nrot) {
  for(int r=0;r<4;r++){
    for(int c=0;c<4;c++){
      if(current.shape[nrot][r][c]) {
	int x=nx+c, y=ny+r;
	if(x<0 || x>=BOARD_COLS || y>=BOARD_ROWS) return 1;
	if(y>=0 && grid[y][x]) return 1;
      }
    }
  }
  return 0;
}

void place_piece() {
  for(int r=0;r<4;r++){
    for(int c=0;c<4;c++){
      if(current.shape[rotation][r][c]) {
	int x=px+c, y=py+r;
	if(y>=0 && y<BOARD_ROWS){
	  grid[y][x]=1;
	}
      }
    }
  }
}

void remove_lines() {

  for(int r=0;r<BOARD_ROWS;r++) {
    int full=1;
    
    for(int c=0;c<BOARD_COLS;c++){
      if(!grid[r][c]){
	full=0;
       }
    }
  
    if(full) {
      
      for(int rr=r;rr>0;rr--){
	for(int c=0;c<BOARD_COLS;c++){
	  grid[rr][c]=grid[rr-1][c];
	}
      }
      
      for(int c=0;c<BOARD_COLS;c++){
	grid[0][c]=0;
      }  
    }
    
  }
}

void try_move_left(){
  if(!collides(px-1,py,rotation)){
    px--;
  }
}

void try_move_right(){
  if(!collides(px+1,py,rotation)){
    px++;
  }
}

void try_rotate(){
  int nr=(rotation+1)%current.nrot;
  if(!collides(px,py,nr)){
    rotation=nr;
  }
}

void soft_drop(){
  if(!collides(px,py+1,rotation)){
    py++;
  }else{
    place_piece();
    remove_lines();
    spawn_piece();
  }
}

void spawn_piece() {
  current=tetrominoes[rand()%7];
  px=(BOARD_COLS-4)/2;
  py=-2;
  rotation=0;
  if(collides(px,py,rotation)){

    for(int r=0;r<BOARD_ROWS;r++){
      for(int c=0;c<BOARD_COLS;c++){
	grid[r][c]=0;
      }
    }
    
  }
}

void update_shape() {
  clearScreen(COLOR_EMPTY);
  draw_grid();
  draw_current_piece();
}

void wdt_c_handler() {
  static int count=0;
  count++;

  if(count>=25) {
    soft_drop();
    redrawScreen=1;
    count=0;
  }
}

void main() {

  P1DIR|=BIT6; P1OUT|=BIT6;
  configureClocks();
  lcd_init();
  switch_init();
  enableWDTInterrupts();
  or_sr(0x8);
  spawn_piece();

  while(1) {
    if(redrawScreen){
      redrawScreen=0;
      update_shape();
    }

    P1OUT&=~BIT6;
    or_sr(0x10);
    P1OUT|=BIT6;

    if(switches & SW1){
      try_move_left();
    }

    if(switches & SW2){
      try_move_right();
    }
    
    if(switches & SW3){
      try_rotate();
    }
    
    if(switches & SW4){
      soft_drop();
    }
    
  }
}

void __interrupt_vec(PORT2_VECTOR) Port_2(){
  if(P2IFG & SWITCHES){
    P2IFG &= ~SWITCHES;
    switch_interrupt_handler();
  }
}
