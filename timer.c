#include <signal.h>     // for SIGEV_THREAD, sigevent
#include <stdbool.h>    // for bool
#include <time.h>       // for timer_create, timer_settime


void tick_create(void (*callback)(__sigval_t), unsigned int sec, unsigned int nsec, bool once)
{
    struct sigevent timerEvent = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = callback,
        .sigev_notify_attributes = NULL,
    };
    struct itimerspec timerPeriod = {
        .it_value.tv_sec = (once) ? sec : 0,
        .it_value.tv_nsec = (once) ? nsec : 1,    // If !once we want the timer to fire immediately
        .it_interval.tv_sec = (once) ? 0 : sec,
        .it_interval.tv_nsec = (once) ? 0 : nsec,
    };
    timer_t timer;

    timer_create(CLOCK_MONOTONIC, &timerEvent, &timer);
    timer_settime(timer, 0, &timerPeriod, NULL);
}
