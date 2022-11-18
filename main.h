#pragma once

#include <stdbool.h>
#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <stdnoreturn.h>  // for noreturn
#include <sys/ioctl.h>
#include <time.h>

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
} options_t;
