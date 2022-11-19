#include "stats.h"
#include "graphics.h"   // for ANSI_RESET_ALL, gotoStatLine, ANSI_FG_CYAN
#include "main.h"       // for window_t
#include "timer.h"      // for timespecsub, SECS_IN_DAY, SEC_TO_MSEC
#include "util.h"       // for printable_strlen
#include <stdarg.h>     // for va_end, va_list, va_start
#include <stdbool.h>    // for false, bool, true
#include <stdio.h>      // for fputs, sscanf, fclose, fgets, fopen, stdout
#include <stdlib.h>     // for strtol
#include <string.h>     // for memcpy, strncmp, memset, strncat
#include <sys/ioctl.h>  // for winsize
#include <time.h>       // for NULL, timespec, clock_gettime, CLOCK_MONOTONIC


static char spinner = '-';

static void addStatIfRoom(window_t* window, char* statOutput, statColour_t status,
                          const char* format, ...) __attribute__((format(printf, 4, 5)));

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
static bool getCPUUsage(float* usage, statColour_t* status)
{
    FILE* fp;
    char* retVal;
    char statLine[256];  // Theoretically could be up to ~220 characters, usually ~50
    float interval, idleTime;
    struct procStat statBuffer;
    struct cpuStat newReading;
    static struct cpuStat oldReading;

    if ((usage == NULL) || (status == NULL))
        return false;

    fp = fopen("/proc/stat", "r");
    if (fp == NULL)
        return false;

    retVal = fgets(statLine, sizeof(statLine), fp);
    fclose(fp);
    if ((retVal == NULL) || (statLine[0] != 'c'))
    {
        return false;
    }

    if (sscanf(statLine, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
               &(statBuffer.tUser), &(statBuffer.tNice), &(statBuffer.tSystem),
               &(statBuffer.tIdle), &(statBuffer.tIoWait), &(statBuffer.tIrq),
               &(statBuffer.tSoftIrq), &(statBuffer.tSteal), &(statBuffer.tGuest),
               &(statBuffer.tGuestNice)) != 10)
    {
        return false;
    }

    newReading.tBusy = statBuffer.tUser + statBuffer.tNice + statBuffer.tSystem +
                       statBuffer.tIrq + statBuffer.tSoftIrq + statBuffer.tSteal +
                       statBuffer.tGuest + statBuffer.tGuestNice;
    newReading.tIdle = statBuffer.tIdle + statBuffer.tIoWait;

    if ((oldReading.tBusy == 0) && (oldReading.tIdle == 0))
    {
        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return false;
    }
    else
    {
        interval = (newReading.tBusy + newReading.tIdle) -
                   (oldReading.tBusy + oldReading.tIdle);
        idleTime = newReading.tIdle - oldReading.tIdle;

        if ((interval <= 0) || (idleTime >= interval))
            return false;

        *usage = ((interval - idleTime) / interval) * 100;
        if (*usage >= CPU_RED)
            *status = STAT_COLOUR_RED;
        else if (*usage >= CPU_AMBER)
            *status = STAT_COLOUR_AMBER;
        else
            *status = STAT_COLOUR_GREY;

        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return true;
    }
}




static bool getMemUsage(float* usage, statColour_t* status)
{
    char memLine[64];  // should really be around 30 characters
    FILE* fp;
    unsigned long memAvailable = 0, memTotal = 0;
    bool gotTotal = false;
    bool gotAvailable = false;

    if ((usage == NULL) || (status == NULL))
        return false;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL)
        return false;

    while ((fgets(memLine, sizeof(memLine), fp)) && (!gotTotal || !gotAvailable))
    {
        if (strncmp(memLine, "Mem", 3) == 0)
        {
            if (!gotTotal)
            {
                if (sscanf(memLine, "MemTotal: %lu", &memTotal) == 1)
                {
                    gotTotal = true;
                    continue;  // No point looking for available on this line
                }
            }
            if (!gotAvailable)
            {
                if (sscanf(memLine, "MemAvailable: %lu", &memAvailable) == 1)
                    gotAvailable = true;
            }
        }
    }
    fclose(fp);

    if (gotTotal && gotAvailable && (memTotal != 0))
    {
        *usage = (1 - ((float)memAvailable / memTotal)) * 100;
        if (*usage >= MEMORY_RED)
            *status = STAT_COLOUR_RED;
        else if (*usage >= MEMORY_AMBER)
            *status = STAT_COLOUR_AMBER;
        else
            *status = STAT_COLOUR_GREY;
        return true;
    }
    else
    {
        return false;
    }
}




static bool getNetdevUsage(float* download, float* upload, statColour_t* status)
{
    static struct netDevReading oldReading;
    struct netDevReading newReading = {0};
    unsigned long long bytesDown, bytesUp;
    struct timespec timeDiff;
    char devLine[256];  // should be no longer than ~120 characters
    float interval;
    FILE* fp;

    if ((download == NULL) || (upload == NULL) || (status == NULL))
        return false;

    memset(&newReading, 0, sizeof(newReading));
    clock_gettime(CLOCK_MONOTONIC, &newReading.time);

    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL)
        return false;

    while (fgets(devLine, sizeof(devLine), fp))
    {
        // Ignores all of the table title rows, and the loopback device
        if (*(devLine + 4) != 'l' && *(devLine + 5) != 'o' && *(devLine + 6) == ':')
        {
            if (sscanf(devLine + 7, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &bytesDown,
                       &bytesUp) == 2)
            {
                newReading.bytesDown += bytesDown;
                newReading.bytesUp += bytesUp;
            }
        }
    }
    fclose(fp);

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

        if ((interval <= 0) || (oldReading.bytesDown > newReading.bytesDown) ||
            (oldReading.bytesUp > newReading.bytesUp))
            return false;

        bytesDown = newReading.bytesDown - oldReading.bytesDown;
        bytesUp = newReading.bytesUp - oldReading.bytesUp;
        *download = (bytesDown / 1000.0f) / interval;
        *upload = (bytesUp / 1000.0f) / interval;

        if ((*download >= NET_RED) || (*upload >= NET_RED))
            *status = STAT_COLOUR_RED;
        else if ((*download >= NET_AMBER) || (*upload >= NET_AMBER))
            *status = STAT_COLOUR_AMBER;
        else
            *status = STAT_COLOUR_GREY;

        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return true;
    }
}



static bool getDiskUsage(float* activity, statColour_t* status)
{
    static struct diskReading oldReading;
    struct diskReading newReading = {0};
    struct timespec timeDiff;
    unsigned long tBusy;
    int majorNum = __INT_MAX__;
    char devLine[256];  // should be no longer than ~120 characters
    float interval;
    FILE* fp;

    if ((activity == NULL) || (status == NULL))
        return false;

    memset(&newReading, 0, sizeof(newReading));
    clock_gettime(CLOCK_MONOTONIC, &newReading.time);

    fp = fopen("/proc/diskstats", "r");
    if (fp == NULL)
        return false;

    while (fgets(devLine, sizeof(devLine), fp))
    {
        // Output generated by diskstats_show() in block/genhd.c
        // Skip major numbers we've already seen to avoid double-counting
        if ((strtol(devLine, NULL, 10) != majorNum) &&
            ((strncmp(devLine + 13, "sd", 2) == 0) ||
             (strncmp(devLine + 13, "hd", 2) == 0) ||
             (strncmp(devLine + 13, "nvme", 4) == 0)))
        {
            // Element 13 is total ms spent active
            if (sscanf(devLine, " %d %*d %*s %*u %*u %*u %*u %*u %*u %*u %*u %*u %lu",
                       &majorNum, &tBusy) == 2)
            {
                newReading.tBusy += tBusy;
            }
        }
    }
    fclose(fp);

    if (majorNum == __INT_MAX__)
    {
        return false;
    }
    else if (oldReading.time.tv_sec == 0)
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
        if (*activity >= DISK_RED)
            *status = STAT_COLOUR_RED;
        else if (*activity >= DISK_AMBER)
            *status = STAT_COLOUR_AMBER;
        else
            *status = STAT_COLOUR_GREY;

        memcpy(&oldReading, &newReading, sizeof(oldReading));
        return true;
    }
}


static void addStatIfRoom(window_t* window, char* statOutput, statColour_t status,
                          const char* format, ...)
{
    unsigned prevLength = printable_strlen(statOutput);
    char format_buffer[64] = {0};  // The longest this should be is ~45 chars
    char output_buffer[64] = {0};  // The longest this should be is ~40 chars
    va_list varArgs;

    if (status == STAT_COLOUR_RED)
        sprintf(format_buffer, " [" ANSI_FG_RED "%s" ANSI_RESET_ALL "]", format);
    else if (status == STAT_COLOUR_AMBER)
        sprintf(format_buffer, " [" ANSI_FG_YELLOW "%s" ANSI_RESET_ALL "]", format);
    else
        sprintf(format_buffer, " [" ANSI_FG_DGRAY "%s" ANSI_RESET_ALL "]", format);

    va_start(varArgs, format);
    vsprintf(output_buffer, format_buffer, varArgs);
    va_end(varArgs);

    if ((prevLength + printable_strlen(output_buffer)) < window->termSize.ws_col)
        strncat(statOutput, output_buffer, STAT_OUTPUT_LENGTH - prevLength);
}

void printStats(bool newLine, bool redraw, window_t* window)
{
    struct timespec timeDiff;
    struct timespec currentTime;
    char statOutput[STAT_OUTPUT_LENGTH] = {0};  // Max should be ~100 chars
    static float cpuUsage = __FLT_MAX__;
    static float memUsage = __FLT_MAX__;
    static float diskUsage = __FLT_MAX__;
    static float download = __FLT_MAX__;
    static float upload = __FLT_MAX__;
    unsigned numLines = window->numCharacters / (window->termSize.ws_col + 1);
    statColour_t status = STAT_COLOUR_GREY;

    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    // cppcheck-suppress unreadVariable
    timespecsub(&currentTime, &window->procStartTime, &timeDiff);

    sprintf(statOutput,
            "\e[1G\e[K" ANSI_RESET_ALL ANSI_FG_CYAN "%02ld:%02ld:%02ld %c" ANSI_RESET_ALL,
            (timeDiff.tv_sec % SECS_IN_DAY) / 3600, (timeDiff.tv_sec % 3600) / 60,
            (timeDiff.tv_sec % 60), spinner);

    if (((cpuUsage != __FLT_MAX__) && redraw) || getCPUUsage(&cpuUsage, &status))
    {
        addStatIfRoom(window, statOutput, status, "CPU: %4.1f%%", cpuUsage);
    }

    if (((memUsage != __FLT_MAX__) && redraw) || getMemUsage(&memUsage, &status))
    {
        addStatIfRoom(window, statOutput, status, "Mem: %4.1f%%", memUsage);
    }

    if (((download != __FLT_MAX__) && (upload != __FLT_MAX__) && redraw) ||
        getNetdevUsage(&download, &upload, &status))
    {
        addStatIfRoom(window, statOutput, status, "Rx/Tx: %4.1fKB/s / %.1fKB/s", download,
                      upload);
    }

    if (((diskUsage != __FLT_MAX__) && redraw) || getDiskUsage(&diskUsage, &status))
    {
        addStatIfRoom(window, statOutput, status, "Disk: %4.1f%%", diskUsage);
    }

    if (newLine)
    {
        if (numLines >= (window->termSize.ws_row - 2U))
            fputs("\n\e[1S\e[A", stdout);
        else
            fputs("\n\n\e[A", stdout);
    }

    fputs("\e[s", stdout);
    gotoStatLine(window);
    fputs(statOutput, stdout);
    fputs("\e[u", stdout);
    fflush(stdout);
}
