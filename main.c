/*** Includes ***/
// #include <ctype.h>
#include "main.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** Data and Input ***/

enum Mode { // editor modes will be stored in an enum
  Normal = 0,
  Insert, // Infers that 'insert' is 1
};

typedef struct erow {
  int size;
  char *chars;
} erow;

struct editorConfig { // editor settings will be stored in the struct
  enum Mode mode;
  struct termios orig_termios; // original term settings
  int cx, cy;                  // cursor position
  int screenrows, screencols;  // max screen size
  int numrows;
  erow *rows;
};

struct editorConfig state;

// When an error encountered, 'die' is called; 's' is the errno value
void die(const char *s) {
  tc_exit_alt_screen();
  perror(s);
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.orig_termios) == -1)
    die("tcsettattr");

  exit(1);
}

// Enum for storing special editor keys such that they are easier to deal with
enum editorKey {
  PAGE_UP = 1000,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DELETE_KEY,
  ESCAPE_KEY,
};

// Handles reading the keys inputted by standard input
int editorReadKey() {
  int nread; // number of bytes read
  char c;    // character that is read

  // while, when we try to read a byte, and a byte is what is read
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read"); // and there is an error (nread == -1), then die
  }

  if (c == '\x1b') { // if we read an escape character
    char seq[3];     // create a buffer that will hold the following bytes

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return ESCAPE_KEY; // if there is no character after the escape...
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      // if there is no character after the character after the escape
      return ESCAPE_KEY;

    if (seq[0] == '[') {

      if (seq[1] >= '0' && seq[1] <= '9') {

        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return ESCAPE_KEY; // invalid escape sequence so just return escape

        // Escape sequences: '\x1b[#~' where # is the number specified in seq[1]
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DELETE_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }

      } else { // '\x1b[_'
        switch (seq[1]) {
        case 'A':
          return 'k';
        case 'B':
          return 'j';
        case 'C':
          return 'l';
        case 'D':
          return 'h';
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }

    } else if (seq[0] == 'O') {
      // some terminals use '\x1bOH' and '\x1bOF' as their home and end bindings
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }

    return ESCAPE_KEY;

  } else
    return c; // return the character if we have not read an escape character
}

/*** Terminal ***/

// Return the terminal back to its original settings - IO, cursor, etc.
void disableRawMode() {
  // set terminal setting to old settings
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.orig_termios) == -1)
    die("tcsettattr");
}

// Turn on/off default terminal functionality
void enableRawMode() {
  struct termios raw; // struct is created that will hold terminal attributes

  // get attributes and store in variable
  if (tcgetattr(STDIN_FILENO, &state.orig_termios) == -1)
    die("tcgetattr");

  raw = state.orig_termios; // structs do NOT behave like objects in Java <-
                            // this copies values in the struct
  raw.c_iflag &= ~(
      IXON | ICRNL | BRKINT | INPCK |
      ISTRIP); // IXON: ctrl-s/q - used for pausing + resuming data transmission
  // ICRNL refers to carriage return and newline - terminal makes them the same
  // ASCII code (no good) -> ctrl-m != ctrl-j
  raw.c_oflag &= ~(OPOST); // turn off all post processing of our input
  raw.c_lflag &=
      ~(ECHO | ICANON | ISIG | IEXTEN); // bitwise operation to turn off ECHO
                                        // feature - prints text you type
  // ISIG turns off ctrl-c and ctrl-z functionality in terminal
  // IEXTEN fixes ctrl-v in some terminals where it waits for another character
  // and then sends it, also fixes ctrl-o in Macs Turn off remaining
  // miscellanous flags (including BRKINT, INPCK, ISTRIP):
  raw.c_cflag |= (CS8);

  // update terminal attributes
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

// Function updates the parameters passed to it to reflect the cursor position
// and returns an integer representing success/error
int getCursorPosition(int *rows, int *cols) {
  char buf[32]; // create 32 byte char buffer
  unsigned int i = 0;

  // request cursor position (reports as ESC[#;#R) <- written to STDOUT
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  // iterate through len of buffer writing character by character
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break; // if there is nothing left to read from STDIN, break
    if (buf[i] == 'R')
      break; // Here char array is indexed into like normal; R is the last
             // expected letter from our response (cursor)
    i++;
  }

  buf[i] = '\0'; // strings are to be terminated with the 0 byte
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1; // Our expected res should look like this: ESC[#;#R
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1; // & basically makes char arrays behave like a string ->
               // substring(2:)
  // We are basically saying <- return the items in memory from the third index
  // onward

  return 0;
}

// Function updates the parameters passed to it to reflect the size of the
// terminal and returns an integer representing success/error
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  // if there is an error (column size should not be 0)
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1; // attempt to move to very bottom right of screen
    // and then return cursor position to get the size of the terminal
    return getCursorPosition(rows, cols);
  } else {
    // Return the number of columns and rows gotten for ioctl() and 'ws'
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/

// Instead of writing to the screen one item at a time, we write to the screen
// in bulk using a buffer (avoid flickering)
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0};

// Write to 'abuf' struct 'ab', the char pointer s, and specify the len by which
// to expand the struct's memory
void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len); // set aside new place in memory

  if (new == NULL)
    return; // if memory is completely full and NULL is returned, just return
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

// Zeroes out the buffer
void abFree(struct abuf *ab) { free(ab->b); }

// Upon startup, handles what ought to be done by the editor
void initEditor() {
  state.cx = 0;
  state.cy = 0;
  state.numrows = 0;
  state.mode = Normal;
  state.rows = malloc(sizeof(erow *));
  if (getWindowSize(&state.screenrows, &state.screencols) == -1)
    die("getWindowSize");
}

// Handles drawing the editor - it's called drawRows but using the buffer, it
// draws it all at once
void editorDrawRows(struct abuf *ab) {
  for (int i = 0; i < state.screenrows; i++) {
    // since cursor starts at (0, 0), write ~ to there then do the same unless
    // we are at the last row
    abAppend(ab, "~ ", 2);
    if (i >= state.numrows) {
      // NOTE: Do something
    } else {
      int len = state.rows[i].size;
      if (len > state.screencols)
        len = state.screencols;
      abAppend(ab, state.rows[i].chars, len);
    }

    // "K" erases parts of line; 2 - whole line, 1 - line left of
    // cursor, 0 erases line right of cursor
    // 0 is the default argument if the number is ommitted
    abAppend(ab, "\x1b[K", 3);

    if (i != state.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

// Handles refreshing the screen - updating cursor position visually etc etc.
void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // hide cursor (prevent flickering)
  // move cursor to 0, 0 - but don't change the state cursor values
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  // cy and cx + 1 because terminal rows and cols start at 1
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", state.cy + 1, state.cx + 1);
  // write cursor position to buffer
  abAppend(&ab, buf, strlen(buf)); // add cursor position to buffer to write

  abAppend(&ab, "\x1b[?25h", 6); // show cursor

  write(STDOUT_FILENO, ab.b, ab.len);
  free(ab.b);
}

// Updates the struct containing the configuration of the editor - its state
void editorMoveCursor(char key) {
  switch (key) {
  case 'h':
    if (state.cx != 0)
      state.cx--;
    break;
  case 'j':
    if (state.cy != state.screenrows - 1)
      state.cy++;
    break;
  case 'k':
    if (state.cy != 0)
      state.cy--;
    break;
  case 'l':
    if (state.cx != state.screencols - 1)
      state.cx++;
    break;
  }
}

// Handles what actions should be taken when a keypress is read
void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
  case 'q':
    disableRawMode();
    tc_exit_alt_screen();
    exit(0);
    break;
  case DELETE_KEY:
    // NOTE: Should have the same functionality as the 'x' key
    break;
  case HOME_KEY:
    state.cx = 0;
    break;
  case END_KEY:
    state.cx = state.screencols - 1;
    break;
  case PAGE_UP:
    state.cy = 0;
    break;
  case PAGE_DOWN:
    state.cy = state.screenrows - 1;
    break;
  case 'h':
  case 'j':
  case 'k':
  case 'l':
    editorMoveCursor(c);
    break;
  }
}

/* file I/O */

int arrAppend(char *line) {
  erow *newRows = malloc(sizeof(erow) * (state.numrows + 1));
  if (newRows == NULL) {
    return -1;
  }

  memcpy(newRows, state.rows, state.numrows * (sizeof(erow)));

  erow row;
  row.size = strlen(line);
  row.chars = line;

  newRows[state.numrows] = row;
  state.rows = newRows;

  state.numrows += 1;

  return 0;
}

int readFile(char *filename, char **fileContents) {
  FILE *fptr = fopen(filename, "r");

  if (fptr == NULL)
    die("Error reading file");

  fseek(fptr, 0, SEEK_END);
  long fileSize = ftell(fptr);
  fseek(fptr, 0, SEEK_SET);

  *fileContents = malloc(fileSize + 1);
  if (*fileContents == NULL)
    die("Memory allocation failed");

  fread(*fileContents, 1, fileSize, fptr);
  (*fileContents)[fileSize] = '\0'; // null-terminate for safety

  fclose(fptr);
  return fileSize;
}

void splitIntoRows(char *fileContents, int fileSize) {
  int curr = 0, lineStart = 0;
  while (curr < fileSize) {
    if (fileContents[curr] == '\n') {
      char *buf = malloc(curr - lineStart + 1);
      memcpy(buf, &fileContents[lineStart], curr - lineStart);
      buf[curr - lineStart] = '\0';
      if (arrAppend(buf) == -1) {
        die("row splitting");
      }
      lineStart = curr + 1;
    } else {
    }
    curr++;
  }
}

void editorOpen(char *fileName) {
  char *fileContents;
  int fileSize = readFile(fileName, &fileContents);
  if (fileSize == -1) {
    die("error reading file");
  }
  splitIntoRows(fileContents, fileSize);
}

int main(int argc, char *argv[]) {
  tc_enter_alt_screen();
  editorRefreshScreen();
  enableRawMode();
  initEditor();

  if (argc == 2) {
    editorOpen(argv[1]);
  }

  char c;
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
