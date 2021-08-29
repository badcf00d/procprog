#define _GNU_SOURCE
#include <ctype.h>          // for isprint
#include <errno.h>          // for EINTR, errno
#include <pthread.h>        // for pthread_create, pthread_t
#include <semaphore.h>      // for sem_post, sem_wait, sem_destroy, sem_init
#include <signal.h>         // for sigaction, sigemptyset, sa_handler, SA_RESTART
#include <stdbool.h>        // for false, true, bool
#include <stdio.h>          // for fputs, printf, fflush, stdout, NULL, fclose
#include <stdlib.h>         // for EXIT_FAILURE, calloc, exit, WEXITSTATUS
#include <stdnoreturn.h>    // for noreturn
#include <string.h>         // for memset, strsignal
#include <sys/ioctl.h>      // for ioctl, winsize, TIOCGWINSZ
#include <sys/time.h>       // for CLOCK_MONOTONIC, CLOCK_REALTIME
#include <sys/wait.h>       // for wait
#include <time.h>           // for clock_gettime, timespec
#include <unistd.h>         // for close, dup2, pipe, execvp, fork, read, STDE...
#include "graphics.h"       // for tidyStats, clearScreen, returnToStartLine
#include "stats.h"          // for printStats, advanceSpinner
#include "timer.h"          // for portable_tick_create, MSEC_TO_NSEC, timespe...
#include "util.h"           // for showError, proc_runtime, getArgs, setProgra...

/*
Useful shell one-liner to test:
make && ./procprog perl -e '$| = 1; while (1) { for (1..80) { print("$_\t"); } print "\n"}'
make && ./procprog perl -e '$| = 1; while (1) { for (1..20) { print("$_\t"); select(undef, undef, undef, 0.1); } print "\n"}'
make && ./procprog perl -e '$| = 1; while (1) { for (1..20) { print("$_"); select(undef, undef, undef, 0.1); } print "\n"}'
make && ./procprog perl -e '$| = 1; sleep(3); while (1) { for (1..3) { print("$_"); select(undef, undef, undef, 0.1); } print "\n"}'
make && ./procprog perl -e '$| = 1; for (1..3) { for (1..3) { print("$_"); select(undef, undef, undef, 0.1); } print "\n"}'
make && ./procprog perl -e '$| = 1; while (1) { for (1..99) { print("$_,$_,$_,$_,$_,$_,$_,$_,$_,$_,"); select(undef, undef, undef, 0.1); } print "\n"}'
make && ./procprog perl -e '$| = 1; while (1) { for (1..30) { print("$_,$_,$_,$_,$_,$_,$_,$_,$_,$_,"); select(undef, undef, undef, 0.1); } print "\n"}'
*/

#define DEBUG_FILE "debug.log"

struct timespec procStartTime;
FILE* debugFile;
unsigned numCharacters = 0;
volatile struct winsize termSize;
bool alternateBuffer = false;

static sem_t outputMutex;
static sem_t redrawMutex;
static const char* childProcessName;
static char* inputBuffer;
static FILE* outputFile;




// There's no void in the paramaters here because Linux forces
// this callback to take an input, although we don't care about it
static void tickCallback()
{
    sem_wait(&outputMutex);
    printStats(false, false);
    sem_post(&outputMutex);
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



static void printChar(char character)
{
    if ((inputBuffer) && (numCharacters < 2048))
        *(inputBuffer + numCharacters) = character;

    checkStats();
    putchar(character);
    numCharacters++;
}


static void tabToSpaces(void)
{
    printChar(' ');
    while ((numCharacters % 8) != 0)
    {
        printChar(' ');
    }
}


static void initConsole(void)
{
    inputBuffer = (char*)calloc(sizeof(char), 2048);

    sem_wait(&outputMutex);
    fputs("\e[?25l", stdout);    // Hides cursor
    fputs("\n\e[1A", stdout);    // Set the cursor to out starting position
    sem_post(&outputMutex);
}


static void* readLoop(void* arg)
{
    char inputChar;
    bool newLine = false;
    int procPipe = *(int*)arg;

    initConsole();
    while (read(procPipe, &inputChar, 1) > 0)
    {
        if (outputFile != NULL)
            fwrite(&inputChar, sizeof(inputChar), 1, outputFile);
        if (inputChar == '\n')
        {
            // We don't want to erase the line immediately
            // after the new line character, as this usually
            // means lines get deleted as soon as they are output.
            newLine = true;
            //fprintf(debugFile, "%.03f: inputchar = \\n (%d) (%d)\n",
            //    proc_runtime(), inputChar, isprint(inputChar));
        }
        else
        {
            sem_wait(&outputMutex);

            if (newLine == true)
            {
                advanceSpinner();
                returnToStartLine(true);
                printStats(false, true);
                memset(inputBuffer, 0, 2048);
                numCharacters = 0;
                newLine = false;
            }

            if (inputChar == '\t')
                tabToSpaces();
            else if (isprint(inputChar))
                printChar(inputChar);

            //fprintf(debugFile, "%.03f: inputchar = %c (%d) (%d)\n",
            //    proc_runtime(), inputChar, inputChar, isprint(inputChar));

            fflush(stdout);
            sem_post(&outputMutex);
        }
    }
    return NULL;
}



static void* redrawThread(void* arg)
{
    struct timespec currentTime;
    struct timespec timeoutTime;
    struct timespec debounceTime = {
        .tv_sec = 0,
        .tv_nsec = MSEC_TO_NSEC(300),
    };
    int retval;
    (void)(arg);

    while (1)
    {
        sem_wait(&redrawMutex);
        sem_wait(&outputMutex);
        clearScreen();

    debounce:
        clock_gettime(CLOCK_REALTIME, &currentTime);
        timespecadd(&currentTime, &debounceTime, &timeoutTime);

        while ((retval = sem_timedwait(&redrawMutex, &timeoutTime)) == -1 && (errno == EINTR))
            continue; /* Restart if interrupted by handler */

        if (retval == 0)
            goto debounce;

        printStats(false, true);
        if (inputBuffer)
        {
            fputs(inputBuffer, stdout);
            fflush(stdout);
        }
        sem_post(&outputMutex);
    }
    return NULL;
}



static void sigwinchHandler(int sigNum)
{
    (void)(sigNum);
    ioctl(0, TIOCGWINSZ, &termSize);
    sem_post(&redrawMutex);
    //fprintf(debugFile, "SIGWINCH raised, window size: %d rows / %d columns\n", termSize.ws_row, termSize.ws_col);
}


static void sigintHandler(int sigNum)
{
    (void)sigNum;
    fclose(debugFile);

    if (outputFile != NULL)
        fclose(outputFile);

    tidyStats();
    printf("\n(%s) %s (signal %d) after %.03fs\n", childProcessName, strsignal(sigNum), sigNum,
           proc_runtime());

    if (alternateBuffer)
        fputs("\e[?1049l", stdout);    // Switch to normal screen buffer
    fputs("\e[?25h", stdout);          // Shows cursor
    fflush(stdout);

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
    if (sigaction(SIGTERM, &intCatch, NULL) < 0)
        showError(EXIT_FAILURE, false, "sigaction for SIGTERM failed\n");
    if (sigaction(SIGQUIT, &intCatch, NULL) < 0)
        showError(EXIT_FAILURE, false, "sigaction for SIGQUIT failed\n");

    sigemptyset(&winchCatch.sa_mask);
    winchCatch.sa_flags = SA_RESTART;
    winchCatch.sa_handler = sigwinchHandler;
    if (sigaction(SIGWINCH, &winchCatch, NULL) < 0)
        showError(EXIT_FAILURE, false, "sigaction for SIGWINCH failed\n");
}



noreturn static int runCommand(int procPipe[2], const char** commandLine)
{
    const char* command;
    int status_code;

    dup2(procPipe[1], STDERR_FILENO);
    dup2(procPipe[1], STDOUT_FILENO);
    close(procPipe[0]);
    close(procPipe[1]);

    command = commandLine[0];
    status_code = execvp(command, (char* const*)commandLine);
    showError(EXIT_FAILURE, false, "cannot run %s, execvp returned %d\n", command, status_code);
    /* does not return */
}


static void readOutput(int procPipe[2])
{
    pthread_t threadId, readThread;
    int exitStatus;

    close(procPipe[1]);    // Close write end of fd, only need read
    setupInterupts();

    if (pthread_create(&readThread, NULL, &readLoop, &procPipe[0]) != 0)
        showError(EXIT_FAILURE, false, "pthread_create failed\n");

    if (pthread_create(&threadId, NULL, &redrawThread, NULL) != 0)
        showError(EXIT_FAILURE, false, "pthread_create failed\n");


    portable_tick_create(tickCallback, 1, 0, false);
#if __linux__
    // CPU usage needs to be taken over a time interval
    portable_tick_create(tickCallback, 0, MSEC_TO_NSEC(50), true);
#endif

    wait(&exitStatus);
    pthread_join(readThread, NULL);    // Wait for everything to complete
    tidyStats();

    if (WIFSTOPPED(exitStatus))
        printf("\n\e[1G\e[2K(%s) stopped by signal %d in %.03fs\n", childProcessName,
               WSTOPSIG(exitStatus), proc_runtime());
    else if (WIFSIGNALED(exitStatus))
        printf("\n\e[1G\e[2K(%s) terminated by signal %d in %.03fs\n", childProcessName,
               WTERMSIG(exitStatus), proc_runtime());
    else if (WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus))
        printf("\n\e[1G\e[2K(%s) exited with non-zero status %d in %.03fs\n", childProcessName,
               WEXITSTATUS(exitStatus), proc_runtime());
    else
        printf("\n\e[1G\e[2K(%s) finished in %.03fs\n", childProcessName, proc_runtime());
}



int main(int argc, char** argv)
{
    const char** commandLine;
    int procPipe[2];
    pid_t pid;

    if (pipe(procPipe) != 0)
        showError(EXIT_FAILURE, false, "pipe failed\n");

    if (sem_init(&outputMutex, false, 1) != 0)
        showError(EXIT_FAILURE, false, "sem_init failed\n");
    if (sem_init(&redrawMutex, false, 0) != 0)
        showError(EXIT_FAILURE, false, "sem_init failed\n");

    debugFile = fopen(DEBUG_FILE, "w");
    //setvbuf(debugFile, NULL, _IONBF, 0);
    fprintf(debugFile, "Starting...\n");

    setProgramName(argv[0]);
    commandLine = getArgs(argc, argv, &outputFile);
    childProcessName = commandLine[0];

    ioctl(0, TIOCGWINSZ, &termSize);
    clock_gettime(CLOCK_MONOTONIC, &procStartTime);

    pid = fork();
    if (pid < 0)
        showError(EXIT_FAILURE, false, "fork failed\n");
    else if (pid == 0)
        runCommand(procPipe, commandLine);
    else
        readOutput(procPipe);

    if (alternateBuffer)
        fputs("\e[?1049l", stdout);    // Switch to normal screen buffer
    fputs("\e[?25h", stdout);          // Shows cursor
    fflush(stdout);

    sem_destroy(&outputMutex);
    sem_destroy(&redrawMutex);
    fclose(debugFile);

    if (outputFile != NULL)
        fclose(outputFile);

    return 0;
}
