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
*/

static unsigned int freeTimer;
static unsigned int hours;
static unsigned int minutes;
static unsigned int seconds;
static unsigned int timerLength = 10;
#define DEBUG_FILE "debug.log"
static char spinner = '|';
static FILE* debugFile;
static sem_t mutex;




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

    fprintf(stderr, "\e[s\e[%dG\b\b %c  \e[u", timerLength + 3, spinner);
}




static void timerCallback(union sigval timer_data)
{
    unsigned int hours = min((freeTimer / 3600), 99);
    unsigned int minutes = min((freeTimer / 60) - (hours * 60), 60);
    unsigned int seconds = min(freeTimer - ((hours * 3600) + (minutes * 60)), 60);
    sem_wait(&mutex);

    fprintf(stderr, "\e[s\e[1G[%02u:%02u:%02u] %c\e[u", hours, minutes, seconds, spinner);
    freeTimer++;
    sem_post(&mutex);
}




static void readLoop(int procStdOut[2])
{
    char ch;
    bool newLine = false;

    fprintf(stderr, "\e[%dG", timerLength + 4);

    while(read(procStdOut[0], &ch, 1) > 0)
    {
        if (ch == '\n')
        {
            newLine = true;
        }
        else
        {
            sem_wait(&mutex);

            if (newLine == true)
            {
                fprintf(stderr, "\e[%dG\e[0K", timerLength + 4);
                printSpinner();
                newLine = false;
            }

            putc(ch, stderr);
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
    int procStdOut[2];
    int procStdErr[2];
    const char** commandLine;

    setProgramName(argv[0]);
    commandLine = getArgs(argc, argv);
    pipe(procStdOut);
    pipe(procStdErr);
    sem_init(&mutex, 0, 1);
    debugFile = fopen(DEBUG_FILE, "w");
    setvbuf(debugFile, NULL, _IONBF, 0);
    fprintf(debugFile, "Starting...");

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

        gettimeofday(&timeBefore, NULL);
        readLoop(procStdOut);
        gettimeofday(&timeAfter, NULL);
        timersub(&timeAfter, &timeBefore, &timeDiff);
        fprintf(stderr, "\e[2K\e[1G[%02u:%02u:%02u] Done - %ld.%03lds\n", hours, minutes, seconds, timeDiff.tv_sec, USEC_TO_MSEC(timeDiff.tv_usec));
    }

    sem_destroy(&mutex);
    fclose(debugFile);

    return 0;
}
