#include <errno.h>
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

enum Mode mode;

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

int cx = 0, cy = 0; // cursor position

char editorReadKey(){
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  return c;
}


void editorProcessKeypress(){
  char c = editorReadKey();
  switch (c) {
    case 'q':
      disableRawMode();
      tc_exit_alt_screen();
      exit(0);
      break;
  }
}

void editorRefreshScreen(){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

int main(){

  tc_enter_alt_screen();
  editorRefreshScreen();
  enableRawMode();
  mode = Normal;

  char c;
  while(1){ 
    editorProcessKeypress();
  }

  return 0;
}

