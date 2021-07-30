#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#if __linux__
#include <sys/prctl.h>
#endif
#if __APPLE__
#include <Availability.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

#include "util.h"
#include "timer.h"


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


const char** getArgs(int argc, char** argv)
{
    int optc;
    bool append = false;
    char* outFilename = NULL;
    FILE* outFile = stderr;
    static struct option longOpts[] =
    {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {"append", no_argument, NULL, 'a'},
        {"output-file", required_argument, NULL, 'o'},
        {NULL, no_argument, NULL, 0}
    };
    
    
    while ((optc = getopt_long(argc, argv, "+aho:V", longOpts, (int*) 0)) != EOF)
    {
        switch (optc)
        {
        case 'h':
            showUsage(EXIT_SUCCESS);        
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



// On linux this will always be false on the first call
bool getCPUUsage(float* usage)
{
    if (usage == NULL)
        return false;

#ifdef __APPLE__
    mach_msg_type_number_t count;
    host_cpu_load_info_data_t r_load;
    struct cpuStat newReading;
    static struct cpuStat oldReading;

    count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t *)&r_load, &count)) 
    {
        return false;
    }

    newReading.tBusy = r_load.cpu_ticks[CPU_STATE_SYSTEM] + r_load.cpu_ticks[CPU_STATE_USER] + r_load.cpu_ticks[CPU_STATE_NICE];
    newReading.tIdle = r_load.cpu_ticks[CPU_STATE_IDLE];
#elif __linux__
    FILE *fp;
    char* retVal;
    char statLine[256]; // Theoretically this could be up to ~220 characters, usually ~50
    float interval, idleTime;
    struct procStat statBuffer;
    struct cpuStat newReading;
    static struct cpuStat oldReading;

    fp = fopen("/proc/stat", "r");
    if (fp == NULL)
    {
        return false;
    }

    retVal = fgets(statLine, sizeof(statLine), fp);
    fclose(fp);
    if ((retVal == NULL) || (statLine[0] != 'c'))
    {
        return false;
    }

    sscanf(statLine, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &(statBuffer.tUser), 
        &(statBuffer.tNice), &(statBuffer.tSystem), &(statBuffer.tIdle), &(statBuffer.tIoWait), 
        &(statBuffer.tIrq), &(statBuffer.tSoftIrq), &(statBuffer.tSteal), &(statBuffer.tGuest),
        &(statBuffer.tGuestNice));
    
    newReading.tBusy = statBuffer.tUser + statBuffer.tNice + statBuffer.tSystem + statBuffer.tIrq + 
        statBuffer.tSoftIrq + statBuffer.tSteal + statBuffer.tGuest + statBuffer.tGuestNice;
    newReading.tIdle = statBuffer.tIdle + statBuffer.tIoWait;
#else
    #error "Don't have a CPU usage implemenatation for this OS"
#endif

    if ((oldReading.tBusy == 0) && (oldReading.tIdle == 0))
    {
        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return false;
    }
    else
    {
        interval = (newReading.tBusy + newReading.tIdle) - (oldReading.tBusy + oldReading.tIdle);
        idleTime = newReading.tIdle - oldReading.tIdle;

        if (interval <= 0)
            return false;

        *usage = ((interval - idleTime) / interval) * 100;
        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return true;
    }
}




bool getMemUsage(float* usage)
{
    if (usage == NULL)
        return false;

#ifdef __APPLE__
    // TODO
#elif __linux__
    char memLine[64]; // should really be around 30 characters
	FILE *fp;
	unsigned long memAvailable, memTotal;
    unsigned char fieldsFound = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL)
    {
        return false;
    }
    
    while ((fgets(memLine, sizeof(memLine), fp)) && (fieldsFound != 0b11))
    {
        if (strncmp(memLine, "Mem", 3) == 0)
        {
            if (((fieldsFound & 0b01) == 0) && 
                ((fieldsFound |= sscanf(memLine, "MemTotal: %lu", &memTotal)) > 0))
            {
                continue;
            }
            fieldsFound |= sscanf(memLine, "MemAvailable: %lu", &memAvailable) << 1;
        }
	}
	fclose(fp);
#else
    #error "Don't have a memory usage implemenatation for this OS"
#endif

    if ((fieldsFound == 0b11) && (memTotal != 0))
    {
        *usage = (1 - ((float)memAvailable / memTotal)) * 100;
        return true;
    }
    else
    {
        return false;
    }
}




bool getNetdevUsage(float* download, float* upload)
{
    if ((download == NULL) || (upload == NULL))
        return false;

#ifdef __APPLE__
    // TODO
#elif __linux__
    static struct netDevReading oldReading;
    struct netDevReading newReading = {0};
    unsigned long long bytesDown, bytesUp;
    struct timespec timeDiff;
    char devLine[256]; // should be no longer than ~120 characters
    float interval;
	FILE *fp;

    memset(&newReading, 0, sizeof(newReading));
    clock_gettime(CLOCK_MONOTONIC, &newReading.time);

    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL)
    {
        return false;
    }

    while (fgets(devLine, sizeof(devLine), fp))
    {
        // Ignores all of the table title rows, and the loopback device
        if (*(devLine + 4) != 'l' && *(devLine + 5) != 'o' && *(devLine + 6) == ':')
        {
            sscanf(devLine + 7, "%llu %*u %*u %*u %*u %*u %*u %*u " 
                                "%llu %*u %*u %*u %*u %*u %*u %*u", &bytesDown, &bytesUp);
            newReading.bytesDown += bytesDown;
            newReading.bytesUp += bytesUp;
            //fprintf(debugFile, "Read line, bytesDown %llu, bytesUp %llu\n", bytesDown, bytesUp);
        }
	}
	fclose(fp);
#else
    #error "Don't have a network usage implemenatation for this OS"
#endif

    if ((newReading.bytesDown == 0) && (newReading.bytesUp == 0))
    {
        return false;
    }
    if (oldReading.time.tv_sec == 0)
    {
        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return false;
    }
    else
    {
        timespecsub(&newReading.time, &oldReading.time, &timeDiff);
        interval = timeDiff.tv_sec + (timeDiff.tv_nsec * 1e-9);

        if ((interval <= 0) || 
            (oldReading.bytesDown > newReading.bytesDown) || 
            (oldReading.bytesUp > newReading.bytesUp))
            return false;

        bytesDown = newReading.bytesDown - oldReading.bytesDown;
        bytesUp = newReading.bytesUp - oldReading.bytesUp;
        *download = (bytesDown / 1000.0f) / interval;
        *upload = (bytesUp / 1000.0f) / interval;

        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return true;
    }
}



bool getDiskUsage(float* activity)
{
    if (activity == NULL)
        return false;

#ifdef __APPLE__
    // TODO
#elif __linux__
    static struct diskReading oldReading;
    struct diskReading newReading;
    struct timespec timeDiff;
    unsigned long tBusy;
    char devLine[256]; // should be no longer than ~120 characters
    float interval;
	FILE *fp;

    memset(&newReading, 0, sizeof(newReading));
    clock_gettime(CLOCK_MONOTONIC, &newReading.time);

    fp = fopen("/proc/diskstats", "r");
    if (fp == NULL)
    {
        return false;
    }

    while (fgets(devLine, sizeof(devLine), fp))
    {
        //fprintf(debugFile, "Read line, %s", devLine + 13);
        if ((strncmp(devLine + 13, "sd", 2) == 0) || (strncmp(devLine + 13, "hd", 2) == 0))
        {
            sscanf(devLine + 13, "%*s %*u %*u %*u %*u %*u %*u %*u %*u %*u %lu", &tBusy);
            newReading.tBusy += tBusy;
            //fprintf(debugFile, "Read line, tBusy %lu\n", tBusy);
        }
	}
	fclose(fp);
#else
    #error "Don't have a disk usage implemenatation for this OS"
#endif

    if (oldReading.time.tv_sec == 0)
    {
        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return false;
    }
    else
    {
        timespecsub(&newReading.time, &oldReading.time, &timeDiff);
        interval = SEC_TO_MSEC(timeDiff.tv_sec + (timeDiff.tv_nsec * 1e-9));

        if ((interval <= 0) || (oldReading.tBusy > newReading.tBusy))
            return false;

        *activity = (100 * (newReading.tBusy - oldReading.tBusy)) / interval;
        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return true;
    }
}
