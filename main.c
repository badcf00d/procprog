#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <termios.h>

#include "timer.h"
#include "util.h"

/*
Useful shell one-liner to test:
make && ./procprog perl -e '$| = 1; while (1) { for (1..20) { print("$_"); select(undef, undef, undef, 1); } print "\n"}'
make && ./procprog perl -e '$| = 1; sleep(3); while (1) { for (1..3) { print("$_"); select(undef, undef, undef, 0.1); } print "\n"}'
make && ./procprog perl -e '$| = 1; for (1..3) { for (1..3) { print("$_"); select(undef, undef, undef, 0.1); } print "\n"}'
make && ./procprog perl -e '$| = 1; while (1) { for (1..99) { print("$_,$_,$_,$_,$_,$_,$_,$_,$_,$_,"); select(undef, undef, undef, 0.1); } print "\n"}'
*/

#define TIMER_LENGTH 10
#define SPINNER_POS (TIMER_LENGTH + 2)
#define OUTPUT_POS (SPINNER_POS + 2)
#define DEBUG_FILE "debug.log"

static struct timespec procStartTime;
static unsigned numCharacters = 0;
static volatile struct winsize termSize;
FILE* debugFile;
static sem_t outputMutex;
static sem_t redrawMutex;
static const char* childProcessName;
static char spinner = '-';
static char* inputBuffer;


static void returnToStartLine(bool clearText)
{
    unsigned numLines = numCharacters / (termSize.ws_col + 1);
    fprintf(debugFile, "height: %d, width: %d, numChar: %d, numLines = %d\n", 
                termSize.ws_row, termSize.ws_col, numCharacters, numLines);

    for (unsigned i = 0; i < numLines; i++)
    {
        if (clearText)
        {
            fputs("\e[2K", stderr);
        }
        fputs("\e[1A", stderr);
    }

    if (clearText)
    {
        fputs("\e[2K", stderr);
    }
    fputs("\e[1G", stderr);
}


static void gotoStatLine(void)
{
    for (int i = 0; i < termSize.ws_row; i++)
    {
        fputs("\e[1B\e[2K", stderr);
    }
    fputs("\e[1B\e[1G", stderr);
}


static void tidyStats(void)
{
    fputs("\e[s", stderr);
    gotoStatLine();
    fputs("\e[u", stderr);
}




static void advanceSpinner(void)
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
}



static void printStats(bool newLine, bool redraw)
{
    struct timespec timeDiff;
    struct timespec currentTime;
    char statOutput[150] = {0};
    char* statCursor = statOutput;
    unsigned numLines = numCharacters / (termSize.ws_col + 1);
    static float cpuUsage, memUsage, download, upload;
    static unsigned char validReadings = 0;

    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    timespecsub(&currentTime, &procStartTime, &timeDiff);

    statCursor += sprintf(statOutput, "\e[1G\e[K" ANSI_FG_CYAN "%02ld:%02ld:%02ld %c" ANSI_RESET_ALL, 
                            (timeDiff.tv_sec % SECS_IN_DAY) / 3600,
                            (timeDiff.tv_sec % 3600) / 60, 
                            (timeDiff.tv_sec % 60),
                            spinner);

    if (redraw)
    {
        if (validReadings & 0b001)
        {
            statCursor += sprintf(statCursor, " [" ANSI_FG_DGRAY "CPU: %.1f%%" ANSI_RESET_ALL "]", cpuUsage);
        }
        if (validReadings & 0b010)
        {
            statCursor += sprintf(statCursor, " [" ANSI_FG_DGRAY "Mem: %.1f%%" ANSI_RESET_ALL "]", memUsage);
        }
        if (validReadings & 0b100)
        {
            statCursor += sprintf(statCursor, " [" ANSI_FG_DGRAY "Rx/Tx: %.1fKB/s / %.1fKB/s" ANSI_RESET_ALL "]", download, upload);
        }
    }
    else
    {
        if (validReadings |= getCPUUsage(&cpuUsage))
        {
            statCursor += sprintf(statCursor, " [" ANSI_FG_DGRAY "CPU: %.1f%%" ANSI_RESET_ALL "]", cpuUsage);
        }
        if (validReadings |= (getMemUsage(&memUsage) << 1))
        {
            statCursor += sprintf(statCursor, " [" ANSI_FG_DGRAY "Mem: %.1f%%" ANSI_RESET_ALL "]", memUsage);
        }
        if (validReadings |= (getNetdevUsage(&download, &upload) << 2))
        {
            statCursor += sprintf(statCursor, " [" ANSI_FG_DGRAY "Rx/Tx: %.1fKB/s / %.1fKB/s" ANSI_RESET_ALL "]", download, upload);
        }
    }

    if (newLine)
    {
        if (numLines >= (termSize.ws_row - 2))
        {
            fputs("\n\e[K\e[1S\e[A", stderr);
        }
        else
        {
            fputs("\n\e[K\n\e[A", stderr);
        }
    }
    fputs("\e[s", stderr);
    gotoStatLine();
    fputs(statOutput, stderr);
    fputs("\e[u", stderr);
}



// There's no void in the paramaters here because Linux forces
// this callback to take an input, although we don't care about it
static void tickCallback()
{
    sem_wait(&outputMutex);
    printStats(false, false);
    sem_post(&outputMutex);
}






static void readLoop(int procStdOut[2])
{
    char inputChar;
    inputBuffer = (char*) calloc(sizeof(char), 2048);
    bool newLine = false;

    // Set the cursor to out starting position
    sem_wait(&outputMutex);
    fprintf(stderr, "\n\e[1A");
    sem_post(&outputMutex);

    portable_tick_create(tickCallback, 1, 0, false);
#if __linux__
    // CPU usage needs to be taken over a time interval
    portable_tick_create(tickCallback, 0, MSEC_TO_NSEC(10), true);
#endif

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
            sem_wait(&outputMutex);

            if (newLine == true)
            {
                advanceSpinner();
                printStats(true, true);
                returnToStartLine(true);
                memset(inputBuffer, 0, 2048);
                numCharacters = 0;
                newLine = false;
            }

            if ((inputBuffer) && (numCharacters < 2048))
            {
                *(inputBuffer + numCharacters) = inputChar;
            }
            
            if (numCharacters > termSize.ws_col)
            {
                if ((numCharacters % termSize.ws_col) == 0)
                {
                    printStats(true, true);
                }
            }
            else if (numCharacters == termSize.ws_col)
            {
                printStats(true, true);
            }

            putc(inputChar, stderr);
            //putc(inputChar, debugFile);
            //fprintf(debugFile, "   %d   \n", (numCharacters % termSize.ws_col));

            numCharacters++;
            sem_post(&outputMutex);
        }
    }
}



static void* redrawThread(void* arg)
{
    struct timespec currentTime;
    struct timespec timeoutTime;
    struct timespec debounceTime = {
        .tv_sec = 0,
        .tv_nsec = MSEC_TO_NSEC(500)
    };
    int retval;
    
    while (1)
    {
        sem_wait(&redrawMutex);
        sem_wait(&outputMutex);

        if (inputBuffer)
        {
            returnToStartLine(true);
        }

debounce:
        clock_gettime(CLOCK_REALTIME, &currentTime);
        timespecadd(&currentTime, &debounceTime, &timeoutTime);

        while ((retval = sem_timedwait(&redrawMutex, &timeoutTime)) == -1 && (errno == EINTR))
            continue;       /* Restart if interrupted by handler */

        if (retval == 0)
        {
            fprintf(debugFile, "debounce! errno %d, revtal %d\n", errno, retval);
            goto debounce;
        }

        printStats(false, true);

        if (inputBuffer)
        {
            fputs(inputBuffer, stderr);
        }
        fprintf(debugFile, "redraw done, errno %d\n", errno);
        sem_post(&outputMutex);
    }
    return NULL;
}



static void sigwinchHandler(int sig)
{
    ioctl(0, TIOCGWINSZ, &termSize);
    sem_post(&redrawMutex);
    //fprintf(debugFile, "SIGWINCH raised, window size: %d rows / %d columns\n", termSize.ws_row, termSize.ws_col);
}


static void sigintHandler(int sigNum) 
{
    struct timespec timeDiff;
    struct timespec procEndTime;
    (void)sigNum;

    fclose(debugFile);

    clock_gettime(CLOCK_MONOTONIC, &procEndTime);
    timespecsub(&procEndTime, &procStartTime, &timeDiff);

    tidyStats();
    fprintf(stderr, "\n(%s) SIGINT after %ld.%03lds\n",
                        childProcessName,
                        timeDiff.tv_sec, 
                        NSEC_TO_MSEC(timeDiff.tv_nsec));

    exit(EXIT_SUCCESS);
}



static void setupInterupts(void)
{
    struct sigaction intCatch;
    struct sigaction winchCatch;

    sigemptyset(&intCatch.sa_mask);
    intCatch.sa_flags = 0;
    intCatch.sa_handler = sigintHandler;
    if (sigaction(SIGINT, &intCatch, NULL) < 0)
        showError(EXIT_FAILURE, false, "sigaction for SIGINT failed\n");

    sigemptyset(&winchCatch.sa_mask);
    winchCatch.sa_flags = SA_RESTART;
    winchCatch.sa_handler = sigwinchHandler;
    if (sigaction(SIGWINCH, &winchCatch, NULL) < 0)
        showError(EXIT_FAILURE, false, "sigaction for SIGWINCH failed\n");
}



int main(int argc, char **argv)
{
    struct timespec procEndTime;
    struct timespec timeDiff;
    const char** commandLine;
    int procStdOut[2];
    int procStdErr[2];
    pid_t pid;
    pthread_t threadId;

    setProgramName(argv[0]);
    commandLine = getArgs(argc, argv);
    childProcessName = commandLine[0];

    pipe(procStdOut);
    pipe(procStdErr);

    sem_init(&outputMutex, 0, 1);
    sem_init(&redrawMutex, 0, 0);

    debugFile = fopen(DEBUG_FILE, "w");
    setvbuf(debugFile, NULL, _IONBF, 0);
    fprintf(debugFile, "Starting...\n");

    ioctl(0, TIOCGWINSZ, &termSize);
    clock_gettime(CLOCK_MONOTONIC, &procStartTime);
    setupInterupts();

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

        if (pthread_create(&threadId, NULL, &redrawThread, NULL) != 0)
        {
            showError(EXIT_FAILURE, false, "pthread_create failed\n");
        }

        readLoop(procStdOut);

        clock_gettime(CLOCK_MONOTONIC, &procEndTime);
        timespecsub(&procEndTime, &procStartTime, &timeDiff);

        tidyStats();
        fprintf(stderr, "\e[1G\e[2K(%s) finished in %ld.%03lds\n",
                        childProcessName,
                        timeDiff.tv_sec, 
                        NSEC_TO_MSEC(timeDiff.tv_nsec));
    }

    sem_destroy(&outputMutex);
    sem_destroy(&redrawMutex);
    fclose(debugFile);

    return 0;
}
