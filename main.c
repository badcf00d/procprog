#define _GNU_SOURCE

#include <bits/types/__sigval_t.h>    // for __sigval_t
#include <ctype.h>                    // for isprint
#include <errno.h>                    // for EINTR, errno
#include <pthread.h>                  // for pthread_create, pthread_join, pth...
#include <semaphore.h>                // for sem_post, sem_wait, sem_destroy
#include <signal.h>                   // for sigaction, sigemptyset, sa_handler
#include <stdbool.h>                  // for false, true, bool
#include <stdio.h>                    // for fflush, NULL, printf, fclose, fputs
#include <stdlib.h>                   // for EXIT_FAILURE, calloc, exit, WEXIT...
#include <stdnoreturn.h>              // for noreturn
#include <string.h>                   // for memset, strsignal
#include <sys/ioctl.h>                // for winsize, ioctl, TIOCGWINSZ
#include <sys/time.h>                 // for CLOCK_MONOTONIC, CLOCK_REALTIME
#include <sys/wait.h>                 // for wait
#include <termios.h>                  // for tcsetattr, tcgetattr
#include <time.h>                     // for clock_gettime, timespec
#include <unistd.h>                   // for close, STDIN_FILENO, dup2, read
#include "graphics.h"                 // for setScrollArea, gotoStatLine, clea...
#include "stats.h"                    // for printStats, advanceSpinner
#include "timer.h"                    // for tick_create, MSEC_TO_NSEC
#include "util.h"                     // for showError, proc_runtime, printChar

#define DEBUG_FILE "debug.log"

struct timespec procStartTime;
unsigned numCharacters = 0;
volatile struct winsize termSize;
bool alternateBuffer = false;

static sem_t outputMutex;
static sem_t redrawMutex;
static const char* childProcessName;
static unsigned char* inputBuffer;
static FILE* outputFile;
static FILE* debugFile;
static bool verbose;
static bool debug;
struct termios termRestore;



static void tickCallback(__sigval_t sv)
{
    (void)sv;
    sem_wait(&outputMutex);
    unsetTextFormat();
    printStats(false, false);
    setTextFormat();
    sem_post(&outputMutex);
}


static void initConsole(void)
{
    struct termios term;

    if (!verbose)
    {
        inputBuffer = (unsigned char*)calloc(sizeof(unsigned char), 2048);
        if (!inputBuffer)
            showError(EXIT_FAILURE, false, "Input buffer calloc failed\n");
    }

    sem_wait(&outputMutex);

    tcgetattr(STDIN_FILENO, &term);
    tcgetattr(STDIN_FILENO, &termRestore);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    fputs("\n\e[1A", stdout);    // Set the cursor to our starting position
    sem_post(&outputMutex);
}


static void* readLoop(void* arg)
{
    unsigned char inputChar;
    bool newLine = false;
    int procPipe = *(int*)arg;

    while (read(procPipe, &inputChar, 1) > 0)
    {
        if (outputFile)
            fwrite(&inputChar, sizeof(inputChar), 1, outputFile);

        sem_wait(&outputMutex);

        if (verbose)
        {
            if (inputChar == '\t')
            {
                tabToSpaces(verbose, inputBuffer);
            }
            else if (inputChar == '\n')
            {
                unsetTextFormat();
                printStats(true, true);
                numCharacters = 0;
                setTextFormat();
            }
            else if (isprint(inputChar) || (inputChar == '\e') || (inputChar == '\b'))
            {
                processChar(inputChar, verbose, inputBuffer);
            }
        }
        else
        {
            if ((inputChar >= '\n') && (inputChar <= '\r'))
            {
                if (!newLine)
                {
                    advanceSpinner();
                    newLine = true;
                }
            }
            else
            {
                if (newLine)
                {
                    memset(inputBuffer, 0, 2048);
                    unsetTextFormat();
                    printStats(false, true);
                    returnToStartLine(true);
                    setTextFormat();
                    numCharacters = 0;
                    newLine = false;
                }

                if (inputChar == '\t')
                    tabToSpaces(verbose, inputBuffer);
                else if (isprint(inputChar) || (inputChar == '\e') || (inputChar == '\b'))
                    processChar(inputChar, verbose, inputBuffer);
            }
        }

        if (debug)
            fprintf(debugFile, "%.03f: %c (%u)\n", proc_runtime(), inputChar, inputChar);

        fflush(stdout);
        sem_post(&outputMutex);
    }
    return NULL;
}



static void* inputLoop(void* arg)
{
    unsigned char inputChar;
    int childStdIn = *(int*)arg;

    while (read(STDIN_FILENO, &inputChar, 1) > 0)
    {
        if (outputFile)
            fwrite(&inputChar, sizeof(inputChar), 1, outputFile);

        sem_wait(&outputMutex);

        if (isprint(inputChar))
        {
            processChar(inputChar, verbose, inputBuffer);
            fflush(stdout);
        }
        write(childStdIn, &inputChar, 1);

        if (debug)
            fprintf(debugFile, "stdin: %.03f: %c (%u)\n", proc_runtime(), inputChar, inputChar);

        sem_post(&outputMutex);
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

        if (verbose)
            tidyStats();
        else
            clearScreen();
        fflush(stdout);

    debounce:
        clock_gettime(CLOCK_REALTIME, &currentTime);
        timespecadd(&currentTime, &debounceTime, &timeoutTime);

        while ((retval = sem_timedwait(&redrawMutex, &timeoutTime)) == -1 && (errno == EINTR))
            continue; /* Restart if interrupted by handler */

        if (retval == 0)
            goto debounce;

        printStats(false, true);
        if ((!verbose) && (inputBuffer))
        {
            fputs((const char*)inputBuffer, stdout);
            fflush(stdout);
        }
        sem_post(&outputMutex);
    }
    return NULL;
}



static void sigwinchHandler(int sigNum)
{
    (void)(sigNum);
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize);
    sem_post(&redrawMutex);
}


static noreturn void sigintHandler(int sigNum)
{
    (void)sigNum;
    if (debug)
        fclose(debugFile);
    if (outputFile)
        fclose(outputFile);

    tidyStats();
    unsetTextFormat();
    printf("\n(%s) %s (signal %d) after %.03fs\n", childProcessName, strsignal(sigNum), sigNum,
           proc_runtime());

    if (alternateBuffer)
        fputs("\e[?1049l", stdout);    // Switch to normal screen buffer
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSANOW, &termRestore);
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



noreturn static int runCommand(int outputPipe[2], int inputPipe[2], const char** commandLine)
{
    const char* command;
    int status_code;

    dup2(outputPipe[1], STDERR_FILENO);
    dup2(outputPipe[1], STDOUT_FILENO);
    dup2(inputPipe[0], STDIN_FILENO);
    close(outputPipe[0]);
    close(outputPipe[1]);
    close(inputPipe[0]);
    close(inputPipe[1]);

    command = commandLine[0];
    status_code = execvp(command, (char* const*)commandLine);
    showError(EXIT_FAILURE, false, "cannot run %s, execvp returned %d\n", command, status_code);
    /* does not return */
}


static void readOutput(int outputPipe[2], int inputPipe[2])
{
    pthread_t threadId, readThread, inputThread;
    int exitStatus;

    close(outputPipe[1]);    // Close write end of fd, only need read
    close(inputPipe[0]);     // Close read end of fd, only need write
    setupInterupts();
    initConsole();

    if (pthread_create(&readThread, NULL, &readLoop, &outputPipe[0]) != 0)
        showError(EXIT_FAILURE, false, "pthread_create failed\n");

    if (pthread_create(&threadId, NULL, &redrawThread, NULL) != 0)
        showError(EXIT_FAILURE, false, "pthread_create failed\n");

    if (pthread_create(&inputThread, NULL, &inputLoop, &inputPipe[1]) != 0)
        showError(EXIT_FAILURE, false, "pthread_create failed\n");

    tick_create(tickCallback, 1U, 0U, false);
#if __linux__
    // CPU usage needs to be taken over a time interval
    tick_create(tickCallback, 0U, MSEC_TO_NSEC(50U), true);
#endif

    wait(&exitStatus);
    pthread_join(readThread, NULL);    // Wait for everything to complete

    if (alternateBuffer)
        fputs("\e[?1049l", stdout);    // Switch to normal screen buffer
    unsetTextFormat();
    gotoStatLine();
    tcsetattr(STDIN_FILENO, TCSANOW, &termRestore);

    if (WIFSTOPPED(exitStatus))
        printf("(%s) stopped by signal %d in %.03fs\n", childProcessName, WSTOPSIG(exitStatus),
               proc_runtime());
    else if (WIFSIGNALED(exitStatus))
        printf("(%s) terminated by signal %d in %.03fs\n", childProcessName, WTERMSIG(exitStatus),
               proc_runtime());
    else if (WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus))
        printf("(%s) exited with non-zero status %d in %.03fs\n", childProcessName,
               WEXITSTATUS(exitStatus), proc_runtime());
    else
        printf("(%s) finished in %.03fs\n", childProcessName, proc_runtime());
}



static void initDebugFile(const char* program_name)
{
    time_t rawtime;
    char time_string[32];    // Should be ~18 characters
    char* debug_filename;

    time(&rawtime);
    strftime(time_string, 32, "%d.%m.%Y-%H.%M.%S", localtime(&rawtime));
    asprintf(&debug_filename, "%s_%s.log", program_name, time_string);

    if (debug_filename)
    {
        debugFile = fopen(debug_filename, "w");
        free(debug_filename);
    }

    if (!debugFile)
    {
        showError(EXIT_FAILURE, false, "debug file creation failed\n");
    }
}



int main(int argc, char** argv)
{
    const char** commandLine;
    int outputPipe[2];
    int inputPipe[2];
    pid_t pid;

    if (pipe(outputPipe) != 0)
        showError(EXIT_FAILURE, false, "pipe failed\n");
    if (pipe(inputPipe) != 0)
        showError(EXIT_FAILURE, false, "pipe failed\n");

    if (sem_init(&outputMutex, false, 1) != 0)
        showError(EXIT_FAILURE, false, "sem_init failed\n");
    if (sem_init(&redrawMutex, false, 0) != 0)
        showError(EXIT_FAILURE, false, "sem_init failed\n");

    setProgramName(argv[0]);
    commandLine = getArgs(argc, argv, &outputFile, &verbose, &debug);
    childProcessName = commandLine[0];

    if (debug)
        initDebugFile(childProcessName);

    ioctl(0, TIOCGWINSZ, &termSize);
    clock_gettime(CLOCK_MONOTONIC, &procStartTime);

    pid = fork();
    if (pid < 0)
        showError(EXIT_FAILURE, false, "fork failed\n");
    else if (pid == 0)
        runCommand(outputPipe, inputPipe, commandLine);
    else
        readOutput(outputPipe, inputPipe);

    if (alternateBuffer)
        fputs("\e[?1049l", stdout);    // Switch to normal screen buffer
    fflush(stdout);

    sem_destroy(&outputMutex);
    sem_destroy(&redrawMutex);

    if (debug)
        fclose(debugFile);

    if (outputFile)
        fclose(outputFile);

    return 0;
}
