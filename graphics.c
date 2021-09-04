#include <stdbool.h>      // for bool, false, true
#include <stdio.h>        // for fputs, stdout, fflush, fprintf, printf, FILE
#include <sys/ioctl.h>    // for winsize
#include "util.h"         // for proc_runtime


extern FILE* debugFile;
extern unsigned numCharacters;
extern volatile struct winsize termSize;
extern bool alternateBuffer;

void returnToStartLine(bool clearText)
{
    unsigned numLines = numCharacters / (termSize.ws_col + 1);
    //fprintf(debugFile, "height: %d, width: %d, numChar: %d, numLines = %d\n",
    //            termSize.ws_row, termSize.ws_col, numCharacters, numLines);

    if (clearText)
    {
        for (unsigned i = 0; i < numLines; i++)
        {
            fputs("\e[2K\e[1A", stdout);
        }
        fputs("\e[2K\e[1G", stdout);
    }
    else
    {
        printf("\e[%uA\e[1G", numLines);
    }
}


void setScrollArea(unsigned numLines, bool newline)
{
    if (newline)
        fputs("\n", stdout);

    fputs("\e[s", stdout);
    printf("\e[0;%ur", numLines - 1);
    fputs("\e[u", stdout);

    if (newline)
        fputs("\e[1A", stdout);
}


void gotoStatLine(void)
{
    // Clear screen below cursor, move to bottom of screen
    printf("\e[0J\e[%u;1H", termSize.ws_row + 1U);
}


void tidyStats(void)
{
    fputs("\e[s", stdout);
    gotoStatLine();
    fputs("\e[u", stdout);
}

void clearScreen(void)
{
    fprintf(debugFile, "%.03f: height: %d, width: %d, numChar: %u\n", proc_runtime(),
            termSize.ws_row, termSize.ws_col, numCharacters);

    if (alternateBuffer)
    {
        // Erase screen + saved lines
        fputs("\e[2J\e[3J\e[1;1H", stdout);
    }
    else
    {
        returnToStartLine(false);
        // Clears screen from cursor to end, switches to Alternate Screen Buffer
        // Erases saved lines, sets cursor to top left
        fputs("\e[0J\e[?1049h\e[3J\e[1;1H", stdout);
        alternateBuffer = true;
    }
}
