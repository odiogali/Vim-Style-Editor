#include <stdio.h>
#define tc_enter_alt_screen() puts("\033[?1049h")
#define tc_exit_alt_screen() puts("\033[?1049l")
