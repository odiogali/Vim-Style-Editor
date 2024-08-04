#include <stdio.h>
#define tc_enter_alt_screen() puts("\033[?1049h")
#define tc_exit_alt_screen() puts("\033[?1049l")
#define tc_clear_screen() puts("\x1B[2J")
#define tc_move_cursor(X, Y) printf("\033[%d;%dH", Y, X)
#define tc_move_cursor_left()
#define tc_move_cursor_right()
#define tc_move_cursor_up()
#define tc_move_cursor_down()
