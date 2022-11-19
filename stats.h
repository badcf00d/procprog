#pragma once

#include "main.h"
#include <stdbool.h>  // for bool
#include <time.h>     // for timespec

#define STAT_OUTPUT_LENGTH 128

#define CPU_AMBER 20.f
#define CPU_RED 80.f

#define MEMORY_AMBER 60.f
#define MEMORY_RED 80.f

#define DISK_AMBER 20.f
#define DISK_RED 80.f

#define NET_AMBER 1000.f
#define NET_RED 10000.f

typedef enum
{
    STAT_COLOUR_GREY,
    STAT_COLOUR_AMBER,
    STAT_COLOUR_RED,
} statColour_t;

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

void printStats(bool newLine, bool redraw, window_t* window);
void advanceSpinner(void);
