#include <stdbool.h>


// clang-format off
#define ANSI_BOLD        "\e[1m"
#define ANSI_DIM         "\e[2m"
#define ANSI_UNDERLINED  "\e[4m"
#define ANSI_BLINK       "\e[5m"
#define ANSI_REVERSE     "\e[7m"
#define ANSI_HIDDEN      "\e[8m"

#define ANSI_RESET_ALL         "\e[0m"
#define ANSI_RESET_BOLD        "\e[21m"
#define ANSI_RESET_DIM         "\e[22m"
#define ANSI_RESET_UNDERLINED  "\e[24m"
#define ANSI_RESET_BLINK       "\e[25m"
#define ANSI_RESET_REVERSE     "\e[27m"
#define ANSI_RESET_HIDDEN      "\e[28m"

#define ANSI_FG_DEFAULT  "\e[39m"
#define ANSI_FG_BLACK    "\e[30m"
#define ANSI_FG_RED      "\e[31m"
#define ANSI_FG_GREEN    "\e[32m"
#define ANSI_FG_YELLOW   "\e[33m"
#define ANSI_FG_BLUE     "\e[34m"
#define ANSI_FG_MAGENTA  "\e[35m"
#define ANSI_FG_CYAN     "\e[36m"
#define ANSI_FG_LGRAY    "\e[37m"
#define ANSI_FG_DGRAY    "\e[90m"
#define ANSI_FG_LRED     "\e[91m"
#define ANSI_FG_LGREEN   "\e[92m"
#define ANSI_FG_LYELLOW  "\e[93m"
#define ANSI_FG_LBLUE    "\e[94m"
#define ANSI_FG_LMAGENTA "\e[95m"
#define ANSI_FG_LCYAN    "\e[96m"
#define ANSI_FG_WHITE    "\e[97m"

#define ANSI_BG_DEFAULT  "\e[49m"
#define ANSI_BG_BLACK    "\e[40m"
#define ANSI_BG_RED      "\e[41m"
#define ANSI_BG_GREEN    "\e[42m"
#define ANSI_BG_YELLOW   "\e[43m"
#define ANSI_BG_BLUE     "\e[44m"
#define ANSI_BG_MAGENTA  "\e[45m"
#define ANSI_BG_CYAN     "\e[46m"
#define ANSI_BG_LGRAY    "\e[47m"
#define ANSI_BG_DGRAY    "\e[100m"
#define ANSI_BG_LRED     "\e[101m"
#define ANSI_BG_LGREEN   "\e[102m"
#define ANSI_BG_LYELLOW  "\e[103m"
#define ANSI_BG_LBLUE    "\e[104m"
#define ANSI_BG_LMAGENTA "\e[105m"
#define ANSI_BG_LCYAN    "\e[106m"
#define ANSI_BG_WHITE    "\e[107m"
// clang-format on

void returnToStartLine(bool clearText);
void gotoStatLine(void);
void tidyStats(void);
void clearScreen(void);
void setScrollArea(unsigned numLines, bool newline);
