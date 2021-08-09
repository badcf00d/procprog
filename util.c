#define _GNU_SOURCE

#include <time.h>               // for timespec
#include <bits/getopt_core.h>  // for optind, optarg
#include <features.h>          // for __GLIBC_MINOR__, __GLIBC__
#include <getopt.h>            // for no_argument, getopt_long, option, requ...
#include <pthread.h>           // for pthread_self, pthread_setname_np
#include <stdarg.h>            // for va_end, va_list, va_start
#include <stdbool.h>           // for false, bool, true
#include <stdio.h>             // for NULL, fopen, puts, sscanf, fclose, fgets
#include <stdlib.h>            // for exit, EXIT_FAILURE, EXIT_SUCCESS
#include <stdnoreturn.h>       // for noreturn
#include <sys/time.h>          // for CLOCK_MONOTONIC
#include "timer.h"             // for timespecsub, SEC_TO_MSEC
#include "util.h"
#if __APPLE__
#include <Availability.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

extern struct timespec procStartTime;
extern FILE* debugFile;


unsigned printable_strlen(const char *str) 
{
    unsigned length = 0;
    bool skip = false;

    while (*str != '\0')
    {
        if (*str == '\e')
            skip = true;

        if (skip)
        {
            if (*str == 'm')
                skip = false;
        } 
        else if ((*str >= ' ') && (*str <= '~'))
            length++;

        str++;
    }
    return length;
}



float proc_runtime(void)
{
    struct timespec timeDiff;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    timespecsub(&now, &procStartTime, &timeDiff);

    return timeDiff.tv_sec + (timeDiff.tv_nsec * 1e-9);
}



int setProgramName(char* name)
{
    int retVal = 0;
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


const char** getArgs(int argc, char** argv)
{
    int optc;
    bool append = false;
    char* outFilename = NULL;
    FILE* outFile = stderr;
    static struct option longOpts[] =
    {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {"version", no_argument, NULL, 'V'},
        {"append", no_argument, NULL, 'a'},
        {"output-file", required_argument, NULL, 'o'},
        {NULL, no_argument, NULL, 0}
    };
    
    
    while ((optc = getopt_long(argc, argv, "+aho:Vv", longOpts, (int*) 0)) != EOF)
    {
        switch (optc)
        {
        case 'h':
            showUsage(EXIT_SUCCESS);
        case 'v':
            //TODO verbose output
        case 'V':
            showVersion(EXIT_SUCCESS);        
        case 'a':
            append = true;
            break;
        case 'o':
            outFilename = optarg;
            break;
        default:
            showUsage(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        showError(EXIT_FAILURE, true, "Can't find a program to run, optind = %d\n\n", optind);
    }

    if (outFilename)
    {
        outFile = fopen(outFilename, (append) ? "a" : "w");
        // TODO file output

        if (outFile == NULL)
        {
            showError(EXIT_FAILURE, false, "Couldn't open file: %s\n", outFilename);
        }
    }

    return (const char **)&argv[optind];
}


noreturn void showUsage(int status)
{
    printf("Usage: %s [OPTIONS] COMMAND [ARG]...\n", PROGRAM_NAME);
    puts("Run COMMAND, showing just the most recently output line\n");

    puts("-h, --help            display this help and exit");
    puts("-V, --version         output version information and exit");
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
