void portable_tick_create(void (*callback)(), unsigned int sec, unsigned int nsec, bool once);


#define MSEC_TO_NSEC(x) (x * 1000000L)
#define NSEC_TO_MSEC(x) (x / 1000000L)
#define SECS_IN_DAY (24 * 60 * 60)


#define	timespecsub(finish, start, diff)                        \
    do {                                                        \
        (diff)->tv_sec = (finish)->tv_sec - (start)->tv_sec;    \
        (diff)->tv_nsec = (finish)->tv_nsec - (start)->tv_nsec;	\
        if ((diff)->tv_nsec < 0) {                              \
            (diff)->tv_sec--;                                   \
            (diff)->tv_nsec += 1000000000L;	                    \
        }                                                       \
    } while (0)

#define	timespeccmp(tsp, usp, cmp)                              \
    (((tsp)->tv_sec == (usp)->tv_sec) ?                         \
        ((tsp)->tv_nsec cmp (usp)->tv_nsec) :                   \
        ((tsp)->tv_sec cmp (usp)->tv_sec))

#define	timespecadd(start, inc, output)                         \
    do {                                                        \
        (output)->tv_sec = (start)->tv_sec + (inc)->tv_sec;     \
        (output)->tv_nsec = (start)->tv_nsec + (inc)->tv_nsec;  \
        if ((output)->tv_nsec >= 1000000000L) {                 \
            (output)->tv_sec++;                                 \
            (output)->tv_nsec -= 1000000000L;                   \
        }                                                       \
    } while (0)
