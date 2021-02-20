#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <semaphore.h>

#include "timer.h"
#include "util.h"

/*
    Useful shell one-liner to test:
    ./procprog perl -e '$| = 1; while (1) { for (1..3) { print("$_"); sleep(1); } print "\n"}'
    ./procprog perl -e '$| = 1; while (1) { for (1..9) { print("$_$_$_$_$_$_$_$_$_$_$_$_$_$_$_$_$_$_$_$_"); sleep(1); } print "\n"}'
*/

#define TIMER_LENGTH 10
#define SPINNER_POS (TIMER_LENGTH + 2)
#define OUTPUT_POS (SPINNER_POS + 2)
#define DEBUG_FILE "debug.log"

static char spinner = '|';
static struct timespec procStartTime;
static unsigned int numCharacters = OUTPUT_POS;
static struct winsize termSize;
static FILE* debugFile;
static sem_t mutex;

static void returnToStartLine(bool clearText)
{
    unsigned int numLines;

    ioctl(0, TIOCGWINSZ, &termSize);
    numLines = numCharacters / termSize.ws_col;

    fprintf(debugFile, "width: %d, numChar: %d, numLines = %d\n", termSize.ws_col, numCharacters, numLines);

    for (unsigned int i = 0; i < numLines; i++)
    {
        if (clearText)
        {
            fprintf(stderr, "\e[2K\e[1A");
        }
        else
        {
            fprintf(stderr, "\e[1A");
        }
    }
}


static void returnToContentPos(void)
{
    unsigned int numLines, numCharIn;

    ioctl(0, TIOCGWINSZ, &termSize);
    numLines = numCharacters / termSize.ws_col;
    numCharIn = numCharacters - (termSize.ws_col * numLines);

    fprintf(debugFile, "numLines = %d, numCharIn = %d\n", numLines, numCharIn);

    if (numLines)
    {
        fprintf(stderr, "\e[%uB\e[%uG", numLines, numCharIn);
    }
    else
    {
        fprintf(stderr, "\e[%uG", numCharIn);
    }
}




static void printSpinner(void)
{
    switch (spinner)
    {
        case '/':
            spinner = '-';
            break;
        case '-':
            spinner = '\\';
            break;
        case '\\':
            spinner = '|';
            break;
        case '|':
            spinner = '/';
            break;
    }

    //fprintf(stderr, "%c ", spinner);
    fprintf(stderr, "\e[%uG\e[0K%c ", SPINNER_POS, spinner);
}



// There's no void in the paramaters here because Linux forces
// this callback to take an input, although we don't care about it
static void tickCallback()
{
    struct timespec timeDiff;
    struct timespec currentTime;

    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    timespecsub(&currentTime, &procStartTime, &timeDiff);

    sem_wait(&mutex);

    returnToStartLine(false);
    fprintf(stderr, "\e[1G[%02ld:%02ld:%02ld] %c", 
                        (timeDiff.tv_sec % SECS_IN_DAY) / 3600,
                        (timeDiff.tv_sec % 3600) / 60, 
                        (timeDiff.tv_sec % 60), 
                        spinner);
    returnToContentPos();

    sem_post(&mutex);
}






static void readLoop(int procStdOut[2])
{
    char inputChar;
    bool newLine = false;

    // Set the cursor to out starting position
    fprintf(stderr, "\e[%dG", OUTPUT_POS);

    while (read(procStdOut[0], &inputChar, 1) > 0)
    {
        if (inputChar == '\n')
        {
            // We don't want to erase the line immediately
            // after the new line character, as this usually
            // means lines get deleted as soon as they are output.
            newLine = true;
        }
        else
        {
            sem_wait(&mutex);

            if (newLine == true)
            {
                returnToStartLine(true);
                printSpinner();
                newLine = false;
                numCharacters = OUTPUT_POS;
            }

            putc(inputChar, stderr);
            numCharacters++;

            sem_post(&mutex);
        }
    }
}



static const char** getArgs(int argc, char** argv)
{
    int optc;
    bool append = false;
    char* outFilename = NULL;
    FILE* outFile = stderr;
    static struct option longOpts[] =
    {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {"append", no_argument, NULL, 'a'},
        {"output-file", required_argument, NULL, 'o'},
        {NULL, no_argument, NULL, 0}
    };
    
    
    while ((optc = getopt_long(argc, argv, "+aho:v", longOpts, (int*) 0)) != EOF)
    {
        switch (optc)
        {
        case 'h':
            showUsage(EXIT_SUCCESS);        
        case 'v':
            showVersion(EXIT_SUCCESS);        
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
        outFile = fopen(outFilename, (append) ? "a" : "w");

        if (outFile == NULL)
        {
            showError(EXIT_FAILURE, false, "Couldn't open file: %s\n", outFilename);
        }
    }

    return (const char **)&argv[optind];
}




int main(int argc, char **argv)
{
    struct timespec procEndTime;
    struct timespec timeDiff;
    const char** commandLine;
    int procStdOut[2];
    int procStdErr[2];
    pid_t pid;

    setProgramName(argv[0]);
    commandLine = getArgs(argc, argv);
    pipe(procStdOut);
    pipe(procStdErr);
    sem_init(&mutex, 0, 1);
    debugFile = fopen(DEBUG_FILE, "w");
    setvbuf(debugFile, NULL, _IONBF, 0);
    fprintf(debugFile, "Starting...");

    clock_gettime(CLOCK_MONOTONIC, &procStartTime);

    pid = fork();
    if (pid < 0)
    {
        showError(EXIT_FAILURE, false, "fork failed\n");
    }
    else if (pid == 0)
    {
        close(procStdOut[0]);    // close reading end in the child
        close(procStdErr[0]);    // close reading end in the child

        dup2(procStdOut[1], STDOUT_FILENO);  // send stdout to the pipe
        dup2(procStdErr[1], STDERR_FILENO);  // send stderr to the pipe

        close(procStdOut[1]);    // this descriptor is no longer needed
        close(procStdErr[1]);    // this descriptor is no longer needed

        const char* command = commandLine[0];

        int status_code = execvp(command, (char *const *)commandLine);
        showError(EXIT_FAILURE, false, "cannot run %s, execvp returned %d\n", command, status_code);
        return 1;
    }
    else
    {
        close(procStdOut[1]);  // close the write end of the pipe in the parent
        close(procStdErr[1]);  // close the write end of the pipe in the parent

        portable_tick_create(tickCallback);
        printSpinner();

        readLoop(procStdOut);

        clock_gettime(CLOCK_MONOTONIC, &procEndTime);
        timespecsub(&procEndTime, &procStartTime, &timeDiff);

        fprintf(stderr, "\e[2K\e[1G[%02ld:%02ld:%02ld] Done - %ld.%03lds\n", 
                        (timeDiff.tv_sec % 3600) / 60, 
                        timeDiff.tv_sec % 60, 
                        timeDiff.tv_sec, 
                        timeDiff.tv_sec, 
                        NSEC_TO_MSEC(timeDiff.tv_nsec));
    }

    sem_destroy(&mutex);
    fclose(debugFile);

    return 0;
}
