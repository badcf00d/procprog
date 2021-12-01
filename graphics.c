#include <ctype.h>        // for isalpha
#include <stdbool.h>      // for bool, true, false
#include <stdio.h>        // for fputs, printf, stdout, fprintf, putchar, sscanf
#include <string.h>       // for memset
#include <sys/ioctl.h>    // for winsize
#include "stats.h"        // for printStats
#include "util.h"         // for proc_runtime, printChar, processChar



extern FILE* debugFile;
extern unsigned numCharacters;
extern volatile struct winsize termSize;
extern bool alternateBuffer;

static char csiCommandBuf[16] = {0};
static char* pBuf = csiCommandBuf;
static unsigned char currentTextFormat[8] = {
    0};    // This should be more than enough simultaneous styles




void setTextFormat(void)
{
    for (unsigned i = 0; i < sizeof(currentTextFormat); i++)
    {
        if (currentTextFormat[i] != 0)
        {
            fprintf(debugFile, "csi: %.03f: set text format: %um\n", proc_runtime(),
                    currentTextFormat[i]);
            printf("\e[%um", currentTextFormat[i]);
        }
    }
}

void unsetTextFormat(void)
{
    fprintf(debugFile, "csi: %.03f: unset text format\n", proc_runtime());
    fputs("\e[0m", stdout);
}


void returnToStartLine(bool clearText)
{
    unsigned numLines = (numCharacters + termSize.ws_col - 1) / termSize.ws_col;

    //fprintf(debugFile, "height: %d, width: %d, numChar: %d, numLines = %d\n",
    //            termSize.ws_row, termSize.ws_col, numCharacters, numLines);

    if (clearText)
    {
        for (unsigned i = 1; i < numLines; i++)
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


void gotoStatLine(void)
{
    // Clear screen below cursor, move to bottom of screen
    printf("\e[0J\e[%u;1H", termSize.ws_row + 1U);
}


void tidyStats(void)
{
    unsetTextFormat();
    fputs("\e[s", stdout);
    gotoStatLine();
    fputs("\e[u", stdout);
    setTextFormat();
}

void clearScreen(void)
{
    //fprintf(debugFile, "%.03f: height: %d, width: %d, numChar: %u\n", proc_runtime(),
    //        termSize.ws_row, termSize.ws_col, numCharacters);

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


static void resetTextFormat(void)
{
    fprintf(debugFile, "csi: %.03f: reset text format\n", proc_runtime());
    memset(currentTextFormat, 0, sizeof(currentTextFormat));
}


static void addTextFormat(char* csi_command)
{
    unsigned int format;

    if (sscanf(csi_command, "\e[%u", &format) > 0)
    {
        if (format == 0)
        {
            resetTextFormat();
        }
        else
        {
            for (unsigned i = 0; i < sizeof(currentTextFormat); i++)
            {
                if (currentTextFormat[i] == 0)
                {
                    fprintf(debugFile, "csi: %.03f: added text format %u\n", proc_runtime(),
                            format);
                    currentTextFormat[i] = format;
                    break;
                }
            }
        }
    }
    else
    {
        fprintf(debugFile, "csi: %.03f: invalid text format %s\n", proc_runtime(), csi_command);
    }
}


static void clearCsiBuffer(void)
{
    memset(csiCommandBuf, 0, sizeof(csiCommandBuf));
    pBuf = csiCommandBuf;
}


static bool checkCsiCommand(const unsigned char inputChar, bool* escaped)
{
    bool validCommand = false;
    unsigned commandLen = (pBuf - csiCommandBuf);

    if (commandLen >= (sizeof(csiCommandBuf) - 1U))
    {
        // No valid command should ever be this long, just drop it
        clearCsiBuffer();
        *escaped = false;
        //fprintf(debugFile, "csi: %.03f: too long %c (%u)\n", proc_runtime(), inputChar, inputChar);
    }
    else if ((commandLen == 1) && (inputChar != '['))
    {
        clearCsiBuffer();
        *escaped = false;
        //fprintf(debugFile, "csi: %.03f: not a csi command %c (%u)\n", proc_runtime(), inputChar, inputChar);
    }
    else
    {
        *pBuf++ = inputChar;

        if (isalpha(inputChar))
        {
            switch (inputChar)
            {
            case 'C':    // Cursor forward
            case 'D':    // Cursor back
            case 'G':    // Cursor horizontal position
            case 'K':    // Erase in line
            case 'n':    // Device Status Report
                validCommand = true;
                //fprintf(debugFile, "csi: %.03f: allowed %c (%u)\n", proc_runtime(), inputChar, inputChar);
                break;
            case 'm':    // Text formatting
                validCommand = true;
                addTextFormat(csiCommandBuf);
                break;
            default:
                //fprintf(debugFile, "csi: %.03f: blocked %c (%u)\n", proc_runtime(), inputChar, inputChar);
                clearCsiBuffer();
                break;
            }
            *escaped = false;
        }
    }

    return validCommand;
}


static void checkStats(void)
{
    if (numCharacters > termSize.ws_col)
    {
        if ((numCharacters % termSize.ws_col) == 0)
            printStats(true, true);
    }
    else if (numCharacters == termSize.ws_col)
        printStats(true, true);
}


void printChar(unsigned char character, bool verbose, unsigned char* inputBuffer)
{
    if (!verbose)
    {
        if ((inputBuffer) && (numCharacters < 2048))
            *(inputBuffer + numCharacters) = character;
    }
    checkStats();
    putchar(character);
    numCharacters++;
}


void processChar(unsigned char character, bool verbose, unsigned char* inputBuffer)
{
    static bool escaped;
    if (character == '\e')
    {
        escaped = true;
        clearCsiBuffer();
        fprintf(debugFile, "csi: %.03f: escaped\n", proc_runtime());
    }

    if (escaped)
    {
        if (checkCsiCommand(character, &escaped))
        {
            fputs(csiCommandBuf, stdout);
            clearCsiBuffer();
        }
    }
    else
    {
        printChar(character, verbose, inputBuffer);
    }
}
