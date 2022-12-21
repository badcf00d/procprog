#pragma once

#include <stdbool.h>    // for bool
#include <sys/ioctl.h>  // for winsize
#include <time.h>       // for timespec

typedef struct
{
    struct winsize termSize;
    struct timespec procStartTime;
    unsigned numCharacters;
    bool alternateBuffer;
} window_t;

typedef struct
{
    bool verbose;
    bool debug;
    bool useScrollingRegion;
} options_t;
