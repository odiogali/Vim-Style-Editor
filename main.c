/*** Includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "tc.h"

/*** Data and Input ***/
enum Mode {
  Normal = 0,
  Insert = 1, 
};

struct editorConfig {
  enum Mode mode;
  struct termios orig_termios; // will hold original terminal settings to revert back to it
  int cx, cy; // cursor position
  int screenrows, screencols;
};

struct editorConfig state;

void die(const char *s){
  tc_exit_alt_screen();
  perror(s);
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.orig_termios) == -1) // set terminal setting to old settings
    die("tcsettattr"); 
  
  exit(1);
}

char editorReadKey(){
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  return c;
}

/*** Terminal ***/
void disableRawMode(){
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.orig_termios) == -1) // set terminal setting to old settings
    die("tcsettattr"); 
}

void enableRawMode(){
  struct termios raw; // struct is created that will hold terminal attributes

  if (tcgetattr(STDIN_FILENO, &state.orig_termios) == -1) // get attributes and store in variable
    die("tcgetattr"); 

  raw = state.orig_termios; // structs do NOT behave like objects in Java
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

int getCursorPosition(int *rows, int *cols){
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  printf("\r\n");
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (iscntrl(c)){
      printf("%d\r\n", c);
    } else {
      printf("(%c) %d\r\n", c, c);
    }
  }

  editorReadKey();

  return -1;
}

int getWindowSize(int *rows, int *cols){
  struct winsize ws;

  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B" , 12) != 12) return -1;
    editorReadKey();
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void initEditor(){
  state.cx = 0;
  state.cy = 0;
  state.mode = Normal;
  if (getWindowSize(&state.screenrows, &state.screencols) == -1)
    die("getWindowSize");
}


void editorDrawRows(){
  for (int i = 0; i < state.screenrows; i++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void editorRefreshScreen(){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
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

int main(){

  tc_enter_alt_screen();
  editorRefreshScreen();
  enableRawMode();
  initEditor();

  char c;
  while(1){ 
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

