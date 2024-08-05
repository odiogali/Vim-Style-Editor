#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <termios.h>
#include "tc.h"

enum Mode {
  Normal = 0,
  Insert = 1, 
};

struct termios orig_termios; // will hold original terminal settings to revert back to it

void die(const char *s){
  perror(s);
  exit(1);
}

void disableRawMode(){
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) // set terminal setting to old settings
    die("tcsettattr"); 
}

void enableRawMode(){
  struct termios raw; // struct is created that will hold terminal attributes

  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) // get attributes and store in variable
    die("tcgetattr"); 

  raw = orig_termios; // structs do NOT behave like objects in Java
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); // IXON: ctrl-s/q - used for pausing + resuming data transmission
  // ICRNL refers to carriage return and newline - terminal makes them the same ASCII code (no good) -> ctrl-m != ctrl-j
  raw.c_oflag &= ~(OPOST); // turn off all post processing of our input
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // bitwise operation to turn off ECHO feature - prints text you type
  // ISIG turns off ctrl-c and ctrl-z functionality in terminal
  // IEXTEN fixes ctrl-v in some terminals where it waits for another character and then sends it, also fixes ctrl-o in Macs
  // Turn off remaining miscellanous flags (including BRKINT, INPCK, ISTRIP):
  raw.c_cflag |= (CS8);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) // update terminal attributes
    die("tcsetattr");
}

int handleEvent(char event, enum Mode mode){
  if (mode == 0) {
    switch (event){

    }
  } else {

  }
  return 0;
}

int main(){

  tc_enter_alt_screen();
  tc_clear_screen();
  int cx = 0, cy = 0; // cursor position
  tc_move_cursor(cx, cy);
  enableRawMode();
  enum Mode mode = Normal;

  char c;
  while(read(STDIN_FILENO, &c, 1) == 1){ // may need to do error handling for read??
    if (iscntrl(c)){
      printf("%d\r\n", c);
    } else {
      printf("%d (%c)\r\n", c, c);
    }

    if (c == 'q'){
      disableRawMode();
      tc_exit_alt_screen();
      break;
    }
  }

  return 0;
}

