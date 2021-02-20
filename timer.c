#ifdef __APPLE__

#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>

#else

#include <time.h>
#include <sys/time.h>
#include <signal.h>

#endif






void portable_tick_create(void (*callback)())
{
#ifdef __APPLE__

    /*
        Credit to https://stackoverflow.com/questions/44807302/create-c-timer-in-macos/52905687#52905687
    */

    static dispatch_queue_t queue;
    static dispatch_source_t timer1;

    queue = dispatch_queue_create("timerQueue", 0);
    timer1 = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_event_handler(timer1, ^{callback;});

    dispatch_source_set_cancel_handler(timer1, ^{
        dispatch_release(timer1);
        dispatch_release(queue);
    });

    dispatch_time_t start = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC);
    dispatch_source_set_timer(timer1, start, NSEC_PER_SEC, 0);
    dispatch_resume(timer1);

#elif __linux__

    struct sigevent timerEvent = 
    {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = callback,
        .sigev_notify_attributes = NULL
    };
    struct itimerspec timerPeriod = 
    {
        .it_value.tv_sec = 0,
        .it_value.tv_nsec = 1, // Has the effect of firing essentially immediately
        .it_interval.tv_sec = 1, 
        .it_interval.tv_nsec = 0
    };
    timer_t timer;

    timer_create(CLOCK_MONOTONIC, &timerEvent, &timer);
    timer_settime(timer, 0, &timerPeriod, NULL);

#else
    #error "Don't have a timer implementation for this system"
#endif
}
