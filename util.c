#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <string.h>
#if __linux__
#include <sys/prctl.h>
#endif
#if __APPLE__
#include <Availability.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

#include "util.h"


extern FILE* debugFile;


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



// On linux this will always be false on the first call
bool getCPUUsage(float* usage)
{
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
    char statLine[256]; // Theoretically this could be up to ~220 characters
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

    if ((newReading.tBusy + newReading.tIdle) == 0)
    {
        return false;
    }
    if ((oldReading.tBusy + oldReading.tIdle) == 0)
    {
        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return false;
    }
    else if (usage != NULL)
    {
        unsigned long long intervalTime = (newReading.tBusy + newReading.tIdle) - (oldReading.tBusy + oldReading.tIdle);
        unsigned long long idleTime = newReading.tIdle - oldReading.tIdle;
        *usage = (((float)intervalTime - idleTime) / intervalTime) * 100;

        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return true;
    }
    else
    {
        return false;
    }
}




bool getMemUsage(float* usage)
{
    char memLine[60]; /* should really be around 30 characters */
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
            fieldsFound |= sscanf(memLine, "MemTotal: %lu", &memTotal);
            fieldsFound |= sscanf(memLine, "MemAvailable: %lu", &memAvailable) << 1;
        }
	}
	fclose(fp);

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
