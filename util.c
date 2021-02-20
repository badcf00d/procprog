#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#if __linux__
#include <sys/prctl.h>
#endif
#if __APPLE__
#include <Availability.h>
#endif

#include "util.h"


int countDigits(int n)
{
    unsigned int num = abs(n);
    unsigned int testNum = 10, numDigits = 1;

    while(1) 
    {
        if (num < testNum)
            return numDigits;
        if (testNum > INT_MAX / 10)
            return numDigits + 1;

        testNum *= 10;
        numDigits++;
    }
}


int min(int a, int b) 
{
    if (a > b)
        return b;
    return a;
}


int max(int a, int b) 
{
    if (a < b)
        return b;
    return a;
}


int setProgramName(char* name)
{
    int retVal;
    char nameBuf[16];

	snprintf(nameBuf, sizeof(nameBuf), "%s", name);

#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 12
	retVal = pthread_setname_np(pthread_self(), nameBuf);
#elif  __linux__
	if(prctl(PR_SET_NAME, (unsigned long)nameBuf, 0, 0, 0)) 
    {
		retVal = errno;
	}
#elif __MAC_10_6
	retVal = pthread_setname_np(nameBuf);
#else
    #error "Missing a method to set program name"
#endif

    return retVal;
}


noreturn void showUsage(int status)
{
    printf("Usage: %s [OPTIONS] COMMAND [ARG]...\n", PROGRAM_NAME);
    puts("Run COMMAND, showing just the most recently output line\n");

    puts("-h, --help            display this help and exit");
    puts("-v, --version         output version information and exit");
    puts("-a, --append          with -o FILE, append instead of overwriting");
    puts("-o, --output=FILE     write to FILE instead of stderr");

    exit(status);
}



noreturn void showVersion(int status)
{
    printf("%s %s (Built %s)\nWritten by %s, Copyright %s\n", 
                PROGRAM_NAME, VERSION, __DATE__, AUTHORS, BUILD_YEAR);
    
    exit(status);
}


noreturn void showError(int status, bool shouldShowUsage, const char* format, ...)
{
    va_list varArgs;

    fputs("\e[0;31mError\e[0m: ", stdout);
    
    va_start(varArgs, format);
    vprintf(format, varArgs);
    va_end(varArgs);

    if (shouldShowUsage)
    {
        showUsage(status);
    }
    else
    {
        exit(status);
    }
}


