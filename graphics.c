#include "stats.h"      // for printStats
#include <ctype.h>      // for isalpha
#include <stdbool.h>    // for bool, true, false
#include <stdio.h>      // for fputs, stdout, printf, putchar, sscanf
#include <string.h>     // for memset
#include <sys/ioctl.h>  // for winsize

#include "graphics.h"
#include "main.h"


static char csiCommandBuf[16] = {0};
static char* pBuf = csiCommandBuf;
static unsigned char currentTextFormat[8] = {
    0};  // This should be plenty of simultaneous styles



void setTextFormat(void)
{
    for (unsigned i = 0; i < sizeof(currentTextFormat); i++)
    {
        if (currentTextFormat[i] != 0)
        {
            printf("\e[%um", currentTextFormat[i]);
        }
    }
}

void unsetTextFormat(void)
{
    fputs("\e[0m", stdout);
}


void returnToStartLine(bool clearText, window_t* window)
{
    unsigned numLines = (window->numCharacters + window->termSize.ws_col - 1) /
                        window->termSize.ws_col;

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


void gotoStatLine(window_t* window)
{
    // Clear screen below cursor, move to bottom of screen
    printf("\e[0J\e[%u;1H", window->termSize.ws_row + 1U);
}


void tidyStats(window_t* window)
{
    unsetTextFormat();
    fputs("\e[s", stdout);
    gotoStatLine(window);
    fputs("\e[u", stdout);
    setTextFormat();
}

void clearScreen(window_t* window)
{
    if (window->alternateBuffer)
    {
        // Erase screen + saved lines
        fputs("\e[2J\e[3J\e[1;1H", stdout);
    }
    else
    {
        returnToStartLine(false, window);
        // Clears screen from cursor to end, switches to Alternate Screen Buffer
        // Erases saved lines, sets cursor to top left
        fputs("\e[0J\e[?1049h\e[3J\e[1;1H", stdout);
        window->alternateBuffer = true;
    }
}


static void resetTextFormat(void)
{
    memset(currentTextFormat, 0, sizeof(currentTextFormat));
}


static void addTextFormat(char* csi_command)
{
    unsigned format;

    if (sscanf(csi_command, "\e[%u", &format) != 1)
        return;

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
                currentTextFormat[i] = format;
                break;
            }
        }
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
    }
    else if ((commandLen == 1) && (inputChar != '['))
    {
        clearCsiBuffer();
        *escaped = false;
    }
    else
    {
        *pBuf++ = inputChar;

        if (isalpha(inputChar))
        {
            switch (inputChar)
            {
            case 'C':  // Cursor forward
            case 'D':  // Cursor back
            case 'G':  // Cursor horizontal position
            case 'K':  // Erase in line
            case 'n':  // Device Status Report
                validCommand = true;
                break;
            case 'm':  // Text formatting
                validCommand = true;
                addTextFormat(csiCommandBuf);
                break;
            default:
                clearCsiBuffer();
                break;
            }
            *escaped = false;
        }
    }

    return validCommand;
}


static void checkStats(window_t* window)
{
    if (window->numCharacters > window->termSize.ws_col)
    {
        if ((window->numCharacters % window->termSize.ws_col) == 0)
            printStats(true, true, window);
    }
    else if (window->numCharacters == window->termSize.ws_col)
        printStats(true, true, window);
}


void printChar(unsigned char character, unsigned char* inputBuffer, options_t* options,
               window_t* window)
{
    if (!options->verbose)
    {
        if ((inputBuffer) && (window->numCharacters < 2048))
            *(inputBuffer + window->numCharacters) = character;
    }
    checkStats(window);
    putchar(character);

    window->numCharacters += 1;
}


void processChar(unsigned char character, unsigned char* inputBuffer, options_t* options,
                 window_t* window)
{
    static bool escaped;
    if (character == '\e')
    {
        escaped = true;
        clearCsiBuffer();
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
        printChar(character, inputBuffer, options, window);
    }
}


void tabToSpaces(unsigned char* inputBuffer, options_t* options, window_t* window)
{
    printChar(' ', inputBuffer, options, window);

    if (window->numCharacters > window->termSize.ws_col)
    {
        while ((window->numCharacters - window->termSize.ws_col) % 8)
            printChar(' ', inputBuffer, options, window);
    }
    else
    {
        while ((window->numCharacters % 8) &&
               (window->numCharacters < window->termSize.ws_col))
            printChar(' ', inputBuffer, options, window);
    }
}
