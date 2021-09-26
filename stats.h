#include <stdbool.h>    // for bool
#include <time.h>       // for timespec

#define STAT_FORMAT_LENGTH 128


struct procStat
{
    unsigned long long tUser;
    unsigned long long tNice;
    unsigned long long tSystem;
    unsigned long long tIdle;
    unsigned long long tIoWait;
    unsigned long long tIrq;
    unsigned long long tSoftIrq;
    unsigned long long tSteal;
    unsigned long long tGuest;
    unsigned long long tGuestNice;
};

struct netDevReading
{
    struct timespec time;
    unsigned long long bytesDown;
    unsigned long long bytesUp;
};

struct diskReading
{
    struct timespec time;
    unsigned long tBusy;
};


struct cpuStat
{
    unsigned long long tBusy;
    unsigned long long tIdle;
};

void printStats(bool newLine, bool redraw);
void advanceSpinner(void);
