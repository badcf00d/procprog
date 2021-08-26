#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#else
#include <bits/types/sigevent_t.h>           // for sigev_notify_attributes
#include <bits/types/struct_itimerspec.h>    // for itimerspec
#include <bits/types/timer_t.h>              // for timer_t
#include <signal.h>                          // for SIGEV_THREAD
#include <stdbool.h>                         // for bool
#include <sys/time.h>                        // for CLOCK_MONOTONIC
#include <time.h>                            // for timer_create, timer_settime
#endif




void portable_tick_create(void (*callback)(), unsigned int sec, unsigned int nsec, bool once)
{
#ifdef __APPLE__

    /*
        Credit to https://stackoverflow.com/questions/44807302/create-c-timer-in-macos/52905687#52905687
    */

    static dispatch_queue_t queue;
    static dispatch_source_t timer;

    queue = dispatch_queue_create("timerQueue", 0);
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);

    dispatch_source_set_event_handler(timer, ^{
      callback();
      if (once)
      {
          dispatch_source_cancel(timer);
      }
    });

    dispatch_source_set_cancel_handler(timer, ^{
      dispatch_release(timer);
      dispatch_release(queue);
    });

    dispatch_time_t start = dispatch_time(DISPATCH_TIME_NOW, (once) ? nsec : 1);
    dispatch_source_set_timer(timer, start, sec * NSEC_PER_SEC, 0);
    dispatch_resume(timer);

#elif __linux__

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

#else
#error "Don't have a timer implementation for this system"
#endif
}
