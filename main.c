#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>


#define MSEC_TO_NSEC(x) (x * 1000000)
#define USEC_TO_MSEC(x) (x / 1000)

void timerCallback(union sigval timer_data)
{
    //fprintf(stderr, ".");
}


void printSpinner(void)
{
    static char spinner = '|';

    fprintf(stderr, "\033[s");

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

    fprintf(stderr, "\033[3G");
    putc('\b', stderr);
    putc(spinner, stderr);
    fprintf(stderr, "\033[u");
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
        .it_value.tv_nsec = MSEC_TO_NSEC(500),
        .it_interval.tv_sec = 0,
        .it_interval.tv_nsec = 0
    };
    struct timeval timeBefore;
    struct timeval timeAfter;
    struct timeval timeDiff;
    timer_t timer;
    char ch;


    timer_create(CLOCK_MONOTONIC, &timerEvent, &timer);
    timer_settime(timer, 0, &timerPeriod, NULL);

    fprintf(stderr, "\033[4G");
    printSpinner();
    gettimeofday(&timeBefore, NULL);

    while(read(STDIN_FILENO, &ch, 1) > 0)
    {
        if (ch == '\n')
        {
            fprintf(stderr, "\033[2K\033[4G");
            printSpinner();
            timer_settime(timer, 0, &timerPeriod, NULL);
        }
        else
        {
            //fprintf(stderr, "%c", ch);
            putc(ch, stderr);
        }
    }

    gettimeofday(&timeAfter, NULL);
    timersub(&timeAfter, &timeBefore, &timeDiff);
    printf("\033[2K\033[1GDone... Took %ld.%03ld seconds\n", timeDiff.tv_sec, USEC_TO_MSEC(timeDiff.tv_usec));
    return 0;
}