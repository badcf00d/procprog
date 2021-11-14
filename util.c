#define _GNU_SOURCE

#include <time.h>                // for timespec
#include <bits/getopt_core.h>    // for optind, optarg
#include <features.h>            // for __GLIBC_MINOR__, __GLIBC__
#include <getopt.h>              // for no_argument, getopt_long, option, requ...
#include <pthread.h>             // for pthread_self, pthread_setname_np
#include <stdarg.h>              // for va_end, va_list, va_start
#include <stdbool.h>             // for false, bool, true
#include <stdio.h>               // for NULL, fopen, puts, sscanf, fclose, fgets
#include <stdlib.h>              // for exit, EXIT_FAILURE, EXIT_SUCCESS
#include <stdnoreturn.h>         // for noreturn
#include <sys/time.h>            // for CLOCK_MONOTONIC
#include <sys/ioctl.h>           // for ioctl, winsize, TIOCGWINSZ, TIOCSWINSZ
#include <string.h>
#include <ctype.h>
#include "timer.h"    // for timespecsub, SEC_TO_MSEC
#include "util.h"
#include "graphics.h"
#include "stats.h"

extern struct timespec procStartTime;
extern FILE* debugFile;
extern unsigned numCharacters;
extern volatile struct winsize termSize;
static char csiCommandBuf[16] = {0};
static char* pBuf = csiCommandBuf;

static void clear_csi_buffer(void)
{
    memset(csiCommandBuf, 0, sizeof(csiCommandBuf));
    pBuf = csiCommandBuf;
}

static bool check_csi_command(const unsigned char inputChar, bool* escaped)
{
    bool validCommand = false;
    unsigned commandLen = (pBuf - csiCommandBuf);

    if (commandLen >= (sizeof(csiCommandBuf) - 1U))
    {
        // No valid command should ever be this long, just drop it
        clear_csi_buffer();
        *escaped = false;
        //fprintf(debugFile, "csi: %.03f: too long %c (%u)\n", proc_runtime(), inputChar, inputChar);
    }
    else if ((commandLen == 1) && (inputChar != '['))
    {
        clear_csi_buffer();
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
            case 'm':    // Test formatting
            case 'n':    // Test formatting
                validCommand = true;
                //fprintf(debugFile, "csi: %.03f: allowed %c (%u)\n", proc_runtime(), inputChar, inputChar);
                break;
            default:
                //fprintf(debugFile, "csi: %.03f: blocked %c (%u)\n", proc_runtime(), inputChar, inputChar);
                clear_csi_buffer();
                break;
            }
            *escaped = false;
        }
    }

    return validCommand;
}

unsigned printable_strlen(const char* str)
{
    unsigned length = 0;
    bool skip = false;

    while (*str != '\0')
    {
        if (*str == '\e')
            skip = true;

        if (skip)
        {
            if (*str == 'm')
                skip = false;
        }
        else if ((*str >= ' ') && (*str <= '~'))
            length++;

        str++;
    }
    return length;
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


void printChar(unsigned char character, bool verbose, char* inputBuffer)
{
    static bool escaped;
    if (character == '\e')
    {
        escaped = true;
        clear_csi_buffer();
        fprintf(debugFile, "csi: %.03f: escaped\n", proc_runtime());
    }

    if (escaped)
    {
        if (check_csi_command(character, &escaped))
        {
            fputs(csiCommandBuf, stdout);
            clear_csi_buffer();
        }
    }
    else
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
}


void tabToSpaces(bool verbose, char* inputBuffer)
{
    printChar(' ', verbose, inputBuffer);

    if (numCharacters > termSize.ws_col)
    {
        while ((numCharacters - termSize.ws_col) % 8)
            printChar(' ', verbose, inputBuffer);
    }
    else
    {
        while ((numCharacters % 8) && (numCharacters < termSize.ws_col))
            printChar(' ', verbose, inputBuffer);
    }
}



float proc_runtime(void)
{
    struct timespec timeDiff;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    timespecsub(&now, &procStartTime, &timeDiff);

    return timeDiff.tv_sec + (timeDiff.tv_nsec * 1e-9);
}



int setProgramName(char* name)
{
    int retVal = 0;
    char nameBuf[16];

    snprintf(nameBuf, sizeof(nameBuf), "%s", name);

#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 12
    retVal = pthread_setname_np(pthread_self(), nameBuf);
#elif __linux__
    if (prctl(PR_SET_NAME, (unsigned long)nameBuf, 0, 0, 0))
    {
        retVal = errno;
    }
#elif __MAC_10_6
    retVal = pthread_setname_np(nameBuf);
#else
#error "Missing a method to set program name"
#endif

    return retVal;
}


const char** getArgs(int argc, char** argv, FILE** outputFile, bool* verbose)
{
    static struct option longOpts[] = {{"help", no_argument, NULL, 'h'},
                                       {"verbose", no_argument, NULL, 'v'},
                                       {"version", no_argument, NULL, 'V'},
                                       {"append", no_argument, NULL, 'a'},
                                       {"output-file", required_argument, NULL, 'o'},
                                       {NULL, no_argument, NULL, 0}};
    int optc;
    bool append = false;
    char* outFilename = NULL;

    while ((optc = getopt_long(argc, argv, "+aho:vV", longOpts, (int*)0)) != EOF)
    {
        switch (optc)
        {
        case 'h':
            showUsage(EXIT_SUCCESS);    // Doesn't return
        case 'v':
            *verbose = true;
            break;
        case 'V':
            showVersion(EXIT_SUCCESS);    // Doesn't return
        case 'a':
            append = true;
            break;
        case 'o':
            outFilename = optarg;
            break;
        default:
            showUsage(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        showError(EXIT_FAILURE, true, "Can't find a program to run, optind = %d\n\n", optind);
    }

    if (outFilename)
    {
        *outputFile = fopen(outFilename, (append) ? "a" : "w");

        if (*outputFile == NULL)
        {
            showError(EXIT_FAILURE, false, "Couldn't open file: %s\n", outFilename);
        }
    }

    return (const char**)&argv[optind];
}


noreturn void showUsage(int status)
{
    printf("Usage: %s [OPTIONS] COMMAND [ARG]...\n", PROGRAM_NAME);
    puts("Run COMMAND, showing just the most recently output line\n");

    puts("-h, --help            display this help and exit");
    puts("-V, --version         output version information and exit");
    puts("-a, --append          with -o FILE, append instead of overwriting");
    puts("-o, --output=FILE     write to FILE instead of stderr");

    exit(status);
}



noreturn void showVersion(int status)
{
    printf("%s %s (Built %s)\nWritten by %s, Copyright %s\n", PROGRAM_NAME, VERSION, __DATE__,
           AUTHORS, BUILD_YEAR);

    exit(status);
}


noreturn void showError(int status, bool shouldShowUsage, const char* format, ...)
{
    va_list varArgs;

    fputs(ANSI_FG_RED "Error " ANSI_RESET_ALL, stderr);

    va_start(varArgs, format);
    vprintf(format, varArgs);
    va_end(varArgs);

    if (shouldShowUsage)
    {
        showUsage(status);
    }
    else
    {
        exit(status);
    }
}
