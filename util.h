#pragma once

#include "main.h"
#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <stdnoreturn.h>  // for noreturn

#define AUTHORS "Peter Frost"
#define PROGRAM_NAME "procprog"
#define BUILD_YEAR (&(__DATE__)[7])
#define CONTACTS "mail@pfrost.me"


unsigned printable_strlen(const char* str);
const char** getArgs(int argc, char** argv, FILE** outputFile, options_t* options);
noreturn void showUsage(int status);
noreturn void showVersion(int status);
noreturn void showError(int status, bool shouldShowUsage, const char* format, ...)
    __attribute__((format(printf, 3, 4)));
double proc_runtime(window_t* window);
