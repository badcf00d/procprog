#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>



#define MSEC_TO_NSEC(x) (x * 1000000)
#define USEC_TO_MSEC(x) (x / 1000)
static unsigned int freeTimer = 0;
static unsigned int timerLength = 10;
static char spinner = '|';



static int countDigits(int n) 
{
    unsigned int num = abs(n);
    unsigned int testNum = 10, numDigits = 1;

    while(1) 
    {
        if (num < testNum)
            return numDigits;
        if (testNum > INT_MAX / 10)
            return numDigits + 1;

        testNum *= 10;
        numDigits++;
    }
}


static int min(int a, int b) 
{
    if (a > b)
        return b;
    return a;
}

static int max(int a, int b) 
{
    if (a < b)
        return b;
    return a;
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

    fprintf(stderr, "\033[s\033[%dG\b\b %c  \033[u", timerLength + 3, spinner);
}


static void timerCallback(union sigval timer_data)
{
    unsigned int hours = min((freeTimer / 3600), 99);
    unsigned int minutes = min((freeTimer / 60) - (hours * 60), 60);
    unsigned int seconds = min(freeTimer - (minutes * 60), 60);

    fprintf(stderr, "\033[s\033[1G[%02u:%02u:%02u] %c\033[u", hours, minutes, seconds, spinner);
    freeTimer++;
}


static void readLoop()
{
    char ch;
    bool newLine = false;

    fprintf(stderr, "\033[%dG", timerLength + 4);

    while(read(STDIN_FILENO, &ch, 1) > 0)
    {
        if (ch == '\n')
        {
            newLine = true;
        }
        else
        {
            if (newLine == true)
            {
                fprintf(stderr, "\033[%dG\033[0K", timerLength + 4);
                printSpinner();
                newLine = false;
            }

            putc(ch, stderr);
        }
    }
}


int main()
{
    struct sigevent timerEvent = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = timerCallback,
        .sigev_notify_attributes = NULL
    };
    struct itimerspec timerPeriod = {
        .it_value.tv_sec = 0,
        .it_value.tv_nsec = 1, // Has the effect of firing essentially immediately
        .it_interval.tv_sec = 1, 
        .it_interval.tv_nsec = 0
    };
    struct timeval timeBefore;
    struct timeval timeAfter;
    struct timeval timeDiff;
    timer_t timer;

    timer_create(CLOCK_MONOTONIC, &timerEvent, &timer);
    timer_settime(timer, 0, &timerPeriod, NULL);

    fputs("\033[?25h", stderr); // Hides the cursor (?25h shows it again)
    printSpinner();

    gettimeofday(&timeBefore, NULL);
    readLoop();
    gettimeofday(&timeAfter, NULL);
    
    timersub(&timeAfter, &timeBefore, &timeDiff);
    fprintf(stderr, "\033[%dG\033[0K Done - %ld.%03ld seconds\n", timerLength + 1, timeDiff.tv_sec, USEC_TO_MSEC(timeDiff.tv_usec));
    return 0;
}