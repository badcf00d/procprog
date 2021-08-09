#include "stats.h"
#include <printf.h>     // for parse_printf_format
#include <stdarg.h>     // for va_end, va_list, va_start, va_arg
#include <stdbool.h>    // for false, bool, true
#include <stdio.h>      // for fputs, sscanf, fclose, fgets, fopen, sprintf
#include <string.h>     // for memcpy, strncmp, memset, strcat
#include <sys/ioctl.h>  // for winsize
#include <sys/time.h>   // for CLOCK_MONOTONIC
#include <time.h>       // for timespec, NULL, clock_gettime, size_t
#include "graphics.h"   // for ANSI_RESET_ALL, gotoStatLine, ANSI_FG_CYAN
#include "timer.h"      // for timespecsub, SECS_IN_DAY, SEC_TO_MSEC
#include "util.h"       // for printable_strlen

extern struct timespec procStartTime;
extern FILE* debugFile;
extern unsigned numCharacters;
extern volatile struct winsize termSize;
extern bool alternateBuffer;

static char spinner = '-';

void advanceSpinner(void)
{
    switch (spinner)
    {
        case '/':
            spinner = '-';
            break;
        case '-':
            spinner = '\\';
            break;
        case '\\':
            spinner = '|';
            break;
        case '|':
            spinner = '/';
            break;
    }
}

// On linux this will always be false on the first call
static bool getCPUUsage(float* usage)
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




static bool getMemUsage(float* usage)
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




static bool getNetdevUsage(float* download, float* upload)
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



static bool getDiskUsage(float* activity)
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


static void addStatIfRoom(char* statOutput, const char* format, ...)
{
    unsigned prevLength = printable_strlen(statOutput);
    char buffer[128] = {0};
    va_list varArgs;

    va_start(varArgs, format);
    vsprintf(buffer, format, varArgs);
    va_end(varArgs);
    
    if ((prevLength + printable_strlen(buffer)) < termSize.ws_col)
        strcat(statOutput, buffer);
}



static char* getStatFormat(char* buffer, const char* format, double amberVal, double redVal, ...)
{    
    size_t numStats = parse_printf_format(format, 0, NULL);
    bool amber = false;
    bool red = false;
    va_list varArgs;
    float stat;

    va_start(varArgs, redVal);
    for (; numStats > 0; numStats--)
    {
        stat = va_arg(varArgs, double);
        if (stat >= redVal)
            red = true;
        else if (stat >= amberVal)
            amber = true;
    }
    va_end(varArgs);
    
    if (red)
        sprintf(buffer, " [" ANSI_FG_RED "%s" ANSI_RESET_ALL "]", format);
    else if (amber)
        sprintf(buffer, " [" ANSI_FG_YELLOW "%s" ANSI_RESET_ALL "]", format);
    else
        sprintf(buffer, " [" ANSI_FG_DGRAY "%s" ANSI_RESET_ALL "]", format);
    
    return buffer;
}



void printStats(bool newLine, bool redraw)
{
    struct timespec timeDiff;
    struct timespec currentTime;
    char statOutput[256] = {0};
    char statFormat[128] = {0};
    static float cpuUsage = __FLT_MAX__;
    static float memUsage = __FLT_MAX__;
    static float diskUsage = __FLT_MAX__;
    static float download = __FLT_MAX__;
    static float upload = __FLT_MAX__;    
    unsigned numLines = numCharacters / (termSize.ws_col + 1);

    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    timespecsub(&currentTime, &procStartTime, &timeDiff);

    sprintf(statOutput, "\e[1G\e[K" ANSI_FG_CYAN "%02ld:%02ld:%02ld %c" ANSI_RESET_ALL, 
                            (timeDiff.tv_sec % SECS_IN_DAY) / 3600,
                            (timeDiff.tv_sec % 3600) / 60, 
                            (timeDiff.tv_sec % 60),
                            spinner);

    if (((cpuUsage != __FLT_MAX__) && redraw) || getCPUUsage(&cpuUsage))
    {
        getStatFormat(statFormat, "CPU: %4.1f%%", 20, 80, cpuUsage);
        addStatIfRoom(statOutput, statFormat, cpuUsage);
    }

    if (((memUsage != __FLT_MAX__) && redraw) || getMemUsage(&memUsage))
    {
        getStatFormat(statFormat, "Mem: %4.1f%%", 60, 80, memUsage);
        addStatIfRoom(statOutput, statFormat, memUsage);
    }

    if (((download != __FLT_MAX__) && (upload != __FLT_MAX__) && redraw) || getNetdevUsage(&download, &upload))
    {
        getStatFormat(statFormat, "Rx/Tx: %4.1fKB/s / %.1fKB/s", 1000, 100000, download, upload);
        addStatIfRoom(statOutput, statFormat, download, upload);
    }

    if (((diskUsage != __FLT_MAX__) && redraw) || getDiskUsage(&diskUsage))
    {
        getStatFormat(statFormat, "Disk: %4.1f%%", 20, 80, diskUsage);
        addStatIfRoom(statOutput, statFormat, diskUsage);
    }

    if (newLine)
    {
        if (numLines >= (termSize.ws_row - 2))
            fputs("\n\e[K\e[1S\e[A", stdout);
        else
            fputs("\n\e[K\n\e[A", stdout);
    }

    fputs("\e[s", stdout);
    gotoStatLine();
    fputs(statOutput, stdout);
    fputs("\e[u", stdout);
    fflush(stdout);
}
