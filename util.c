#define _GNU_SOURCE

#include <unistd.h>         // for optind, optarg
#include <features.h>       // for __GLIBC_MINOR__, __GLIBC__
#include <getopt.h>         // for no_argument, getopt_long, option, requ...
#include <pthread.h>        // for pthread_self, pthread_setname_np
#include <stdarg.h>         // for va_end, va_list, va_start
#include <stdbool.h>        // for bool, false, true
#include <stdio.h>          // for puts, NULL, printf, fopen, fputs, snpr...
#include <stdlib.h>         // for exit, EXIT_FAILURE, EXIT_SUCCESS
#include <stdnoreturn.h>    // for noreturn
#include <sys/ioctl.h>      // for winsize
#include <sys/time.h>       // for CLOCK_MONOTONIC
#include <time.h>           // for timespec, clock_gettime
#include "graphics.h"       // for ANSI_FG_RED, ANSI_RESET_ALL
#include "timer.h"          // for timespecsub
#include "util.h"

extern struct timespec procStartTime;
extern unsigned numCharacters;
extern volatile struct winsize termSize;


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



void tabToSpaces(bool verbose, unsigned char* inputBuffer)
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



double proc_runtime(void)
{
    struct timespec timeDiff;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    timespecsub(&now, &procStartTime, &timeDiff);

    return timeDiff.tv_sec + (timeDiff.tv_nsec * 1e-9);
}



const char** getArgs(int argc, char** argv, FILE** outputFile, bool* verbose, bool* debug)
{
    static struct option longOpts[] = {
        {"append", no_argument, NULL, 'a'},  {"debug", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},    {"output-file", required_argument, NULL, 'o'},
        {"verbose", no_argument, NULL, 'v'}, {"version", no_argument, NULL, 'V'},
        {NULL, no_argument, NULL, 0}};
    int optc;
    bool append = false;
    char* outFilename = NULL;
    *verbose = false;
    *debug = false;

    while ((optc = getopt_long(argc, argv, "+adho:vV", longOpts, (int*)0)) != EOF)
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
        case 'd':
            *debug = true;
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
    puts("Run COMMAND with a less verbose output and system usage data\n");

    printf("Usage: %s [OPTION]... COMMAND [ARG]...\n\n", PROGRAM_NAME);
    puts("\t-a, --append          with -o FILE, append instead of overwriting");
    puts("\t-d, --debug           create a very verbose debug file of stdout and stdin");
    puts("\t-h, --help            display this help and exit");
    puts("\t-o, --output=FILE     write to FILE instead of stderr\n");
    puts("\t-v, --verbose         display all output from the child process");
    puts("\t-V, --version         output version information and exit");

    puts("Examples:");
    puts("\tprocproc make         build a project, showing progress and system usage");
    puts("\tprocprog -v 7z b      run a benchmark to test the usage display and show all output\n");

    printf("Report bugs to <%s>\n", CONTACTS);

    exit(status);
}



noreturn void showVersion(int status)
{
    printf("%s %s\n\n", PROGRAM_NAME, VERSION);
    printf("Copyright (C) %s %s\n", BUILD_YEAR, AUTHORS);
    printf("All rights reserved.\n");
    printf("License BSD 2-Clause \"Simplified\" License: "
           "<https://spdx.org/licenses/BSD-2-Clause.html>\n\n");
    printf("Written by %s\n", AUTHORS);

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
