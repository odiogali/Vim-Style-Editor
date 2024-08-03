#include <unistd.h>
#include <stdbool.h>
#include <termios.h>
#include "tc.h"

struct termios orig_termios;

void disableRawMode(){
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode(){
  struct termios raw; // struct is created that will hold terminal attributes

  tcgetattr(STDIN_FILENO, &orig_termios); // get attributes and store in variable
  raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON); // bitwise operation to turn off ECHO feature that prints what you're typing

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // update terminal attributes
}

int main(){

  tc_enter_alt_screen();
  enableRawMode();

  char c;
  while(read(STDIN_FILENO, &c, 1) == 1){
    if (c == 'q'){
      disableRawMode();
      tc_exit_alt_screen();
    }
  }

  return 0;
}

