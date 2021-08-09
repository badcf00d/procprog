#define _GNU_SOURCE
#include <ctype.h>        // for isprint
#include <errno.h>        // for EINTR, errno
#include <printf.h>       // for parse_printf_format
#include <pthread.h>      // for pthread_create, pthread_t
#include <semaphore.h>    // for sem_post, sem_wait, sem_destroy, sem_init
#include <signal.h>       // for sigaction, sigemptyset, sa_handler, SA_RESTART
#include <stdarg.h>       // for va_end, va_list, va_start, va_arg
#include <stdbool.h>      // for false, true, bool
#include <stdio.h>        // for fputs, fflush, stdout, printf, sprintf, NULL
#include <stdlib.h>       // for EXIT_FAILURE, calloc, exit, WEXITSTATUS
#include <stdnoreturn.h>  // for noreturn
#include <string.h>       // for memset, strcat, strsignal
#include <sys/ioctl.h>    // for winsize, ioctl, TIOCGWINSZ
#include <sys/time.h>     // for CLOCK_MONOTONIC, CLOCK_REALTIME
#include <sys/wait.h>     // for wait
#include <time.h>         // for timespec, clock_gettime
#include <unistd.h>       // for close, dup2, pipe, sleep, execvp, fork, read
#include "timer.h"        // for portable_tick_create, NSEC_TO_MSEC, timespe...
#include "util.h"         // for showError, ANSI_RESET_ALL, printable_strlen

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

#define TIMER_LENGTH 10
#define SPINNER_POS (TIMER_LENGTH + 2)
#define OUTPUT_POS (SPINNER_POS + 2)
#define DEBUG_FILE "debug.log"

#define CPU_USAGE_FORMAT " [" ANSI_FG_DGRAY "CPU: %4.1f%%" ANSI_RESET_ALL "]"
#define MEM_USAGE_FORMAT " [" ANSI_FG_DGRAY "Mem: %4.1f%%" ANSI_RESET_ALL "]"
#define NET_USAGE_FORMAT " [" ANSI_FG_DGRAY "Rx/Tx: %4.1fKB/s / %.1fKB/s" ANSI_RESET_ALL "]"
#define DISK_USAGE_FORMAT " [" ANSI_FG_DGRAY "Disk: %4.1f%%" ANSI_RESET_ALL "]"

struct timespec procStartTime;
FILE* debugFile;
static unsigned numCharacters = 0;
static volatile struct winsize termSize;
static sem_t outputMutex;
static sem_t redrawMutex;
static const char* childProcessName;
static char spinner = '-';
static char* inputBuffer;
static bool alternateBuffer = false;


static void returnToStartLine(bool clearText)
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


static void gotoStatLine(void)
{
    // Clear screen below cursor, move to bottom of screen
    fputs("\e[0J\e[9999;1H", stdout);
}


static void tidyStats(void)
{
    fputs("\e[s", stdout);
    gotoStatLine();
    fputs("\e[u", stdout);
}

static void clearScreen(void)
{
    fprintf(debugFile, "%.03f: height: %d, width: %d, numChar: %u\n", 
                proc_runtime(), termSize.ws_row, termSize.ws_col, numCharacters);

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
    fflush(stdout);
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




static void addStatIfRoom(char* statOutput, const char* format, ...)
{
    unsigned prevLength = printable_strlen(statOutput);
    char buffer[128] = {0};
    va_list varArgs;

    va_start(varArgs, format);
    vsprintf(buffer, format, varArgs);
    va_end(varArgs);
    
    if ((prevLength + printable_strlen(buffer)) < termSize.ws_col)
        strcat(statOutput, buffer);
}



static char* getStatFormat(char* buffer, const char* format, double amberVal, double redVal, ...)
{    
    size_t numStats = parse_printf_format(format, 0, NULL);
    bool amber = false;
    bool red = false;
    va_list varArgs;
    float stat;

    va_start(varArgs, redVal);
    for (; numStats > 0; numStats--)
    {
        stat = va_arg(varArgs, double);
        if (stat >= redVal)
            red = true;
        else if (stat >= amberVal)
            amber = true;
    }
    va_end(varArgs);
    
    if (red)
        sprintf(buffer, " [" ANSI_FG_RED "%s" ANSI_RESET_ALL "]", format);
    else if (amber)
        sprintf(buffer, " [" ANSI_FG_YELLOW "%s" ANSI_RESET_ALL "]", format);
    else
        sprintf(buffer, " [" ANSI_FG_DGRAY "%s" ANSI_RESET_ALL "]", format);
    
    return buffer;
}



static void printStats(bool newLine, bool redraw)
{
    struct timespec timeDiff;
    struct timespec currentTime;
    char statOutput[256] = {0};
    char statFormat[128] = {0};
    static float cpuUsage = __FLT_MAX__;
    static float memUsage = __FLT_MAX__;
    static float diskUsage = __FLT_MAX__;
    static float download = __FLT_MAX__;
    static float upload = __FLT_MAX__;    
    unsigned numLines = numCharacters / (termSize.ws_col + 1);

    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    timespecsub(&currentTime, &procStartTime, &timeDiff);

    sprintf(statOutput, "\e[1G\e[K" ANSI_FG_CYAN "%02ld:%02ld:%02ld %c" ANSI_RESET_ALL, 
                            (timeDiff.tv_sec % SECS_IN_DAY) / 3600,
                            (timeDiff.tv_sec % 3600) / 60, 
                            (timeDiff.tv_sec % 60),
                            spinner);

    if (((cpuUsage != __FLT_MAX__) && redraw) || getCPUUsage(&cpuUsage))
    {
        getStatFormat(statFormat, "CPU: %4.1f%%", 20, 80, cpuUsage);
        addStatIfRoom(statOutput, statFormat, cpuUsage);
    }

    if (((memUsage != __FLT_MAX__) && redraw) || getMemUsage(&memUsage))
    {
        getStatFormat(statFormat, "Mem: %4.1f%%", 60, 80, memUsage);
        addStatIfRoom(statOutput, statFormat, memUsage);
    }

    if (((download != __FLT_MAX__) && (upload != __FLT_MAX__) && redraw) || getNetdevUsage(&download, &upload))
    {
        getStatFormat(statFormat, "Rx/Tx: %4.1fKB/s / %.1fKB/s", 1000, 100000, download, upload);
        addStatIfRoom(statOutput, statFormat, download, upload);
    }

    if (((diskUsage != __FLT_MAX__) && redraw) || getDiskUsage(&diskUsage))
    {
        getStatFormat(statFormat, "Disk: %4.1f%%", 20, 80, diskUsage);
        addStatIfRoom(statOutput, statFormat, diskUsage);
    }

    if (newLine)
    {
        if (numLines >= (termSize.ws_row - 2))
            fputs("\n\e[K\e[1S\e[A", stdout);
        else
            fputs("\n\e[K\n\e[A", stdout);
    }

    fputs("\e[s", stdout);
    gotoStatLine();
    fputs(statOutput, stdout);
    fputs("\e[u", stdout);
    fflush(stdout);
}



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
    inputBuffer = (char*) calloc(sizeof(char), 2048);

    sem_wait(&outputMutex);
    fputs("\e[?25l", stdout);   // Hides cursor
    fputs("\n\e[1A", stdout);   // Set the cursor to out starting position
    sem_post(&outputMutex);

    portable_tick_create(tickCallback, 1, 0, false);
#if __linux__
    // CPU usage needs to be taken over a time interval
    portable_tick_create(tickCallback, 0, MSEC_TO_NSEC(50), true);
#endif
}


static void* readLoop(void* arg)
{
    char inputChar;
    bool newLine = false;
    int procStdOut[] = {
        *(&((int*)arg)[0]),
        *(&((int*)arg)[1])
    };

    initConsole();
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
        .tv_nsec = MSEC_TO_NSEC(300)
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
            continue;       /* Restart if interrupted by handler */

        if (retval == 0)
        {
            //fprintf(debugFile, "debounce! errno %d, revtal %d\n", errno, retval);
            goto debounce;
        }

        printStats(false, true);

        if (inputBuffer)
        {
            fputs(inputBuffer, stdout);
            fflush(stdout);
        }
        //fprintf(debugFile, "redraw done, errno %d\n", errno);
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

    tidyStats();
    printf("\n(%s) %s (signal %d) after %.03fs\n",
                        childProcessName,
                        strsignal(sigNum),
                        sigNum,
                        proc_runtime());

    if (alternateBuffer)
        fputs("\e[?1049l", stdout); // Switch to normal screen buffer
    fputs("\e[?25h", stdout);       // Shows cursor
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



noreturn static int runCommand(int procStdOut[2], int procStdErr[2], const char** commandLine)
{
    const char* command;

    close(procStdOut[0]);
    close(procStdErr[0]);
    // TODO: pipe stderr into stdout
    dup2(procStdOut[1], STDOUT_FILENO);
    dup2(procStdErr[1], STDERR_FILENO);
    close(procStdOut[1]);
    close(procStdErr[1]);

    command = commandLine[0];
    int status_code = execvp(command, (char *const *)commandLine);
    showError(EXIT_FAILURE, false, "cannot run %s, execvp returned %d\n", command, status_code);
    /* does not return */
}


static void readOutput(int procStdOut[2], int procStdErr[2])
{
    pthread_t threadId, readThread;
    int exitStatus;

    close(procStdOut[1]);
    close(procStdErr[1]);

    if (pthread_create(&threadId, NULL, &redrawThread, NULL) != 0)
        showError(EXIT_FAILURE, false, "pthread_create failed\n");
    
    if (pthread_create(&readThread, NULL, &readLoop, procStdOut) != 0)
        showError(EXIT_FAILURE, false, "pthread_create failed\n");

    wait(&exitStatus);
    tidyStats();

    if (WIFSTOPPED(exitStatus))
        printf("\e[1G\e[2K(%s) stopped by signal %d in %.03fs\n",
                childProcessName, WSTOPSIG(exitStatus), proc_runtime());
    else if (WIFSIGNALED(exitStatus))
        printf("\e[1G\e[2K(%s) terminated by signal %d in %.03fs\n",
                childProcessName, WTERMSIG(exitStatus), proc_runtime());
    else if (WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus))
        printf("\e[1G\e[2K(%s) exited with non-zero status %d in %.03fs\n",
                childProcessName, WEXITSTATUS(exitStatus), proc_runtime());
    else
        printf("\e[1G\e[2K(%s) finished in %.03fs\n",
                childProcessName, proc_runtime());
}



int main(int argc, char **argv)
{
    const char** commandLine;
    int procStdOut[2];
    int procStdErr[2];
    pid_t pid;


    setProgramName(argv[0]);
    commandLine = getArgs(argc, argv);
    childProcessName = commandLine[0];

    pipe(procStdOut);
    pipe(procStdErr);

    sem_init(&outputMutex, 0, 1);
    sem_init(&redrawMutex, 0, 0);

    debugFile = fopen(DEBUG_FILE, "w");
    //setvbuf(debugFile, NULL, _IONBF, 0);
    fprintf(debugFile, "Starting...\n");

    ioctl(0, TIOCGWINSZ, &termSize);
    clock_gettime(CLOCK_MONOTONIC, &procStartTime);
    setupInterupts();

    pid = fork();
    if (pid < 0)
        showError(EXIT_FAILURE, false, "fork failed\n");
    else if (pid == 0)
        runCommand(procStdOut, procStdErr, commandLine);
    else
        readOutput(procStdOut, procStdErr);

    if (alternateBuffer)
        fputs("\e[?1049l", stdout); // Switch to normal screen buffer
    fputs("\e[?25h", stdout);       // Shows cursor
    fflush(stdout);

    sem_destroy(&outputMutex);
    sem_destroy(&redrawMutex);
    fclose(debugFile);

    return 0;
}
