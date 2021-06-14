#ifdef __APPLE__
#include <mach/mach.h>
#endif

#define AUTHORS "Peter Frost"
#define PROGRAM_NAME "procprog"
#define VERSION "0.1"
#define BUILD_YEAR  (__DATE__ + 7)

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


struct cpuStat
{
#ifdef __APPLE__
    natural_t tBusy;
    natural_t tIdle;
#elif __linux__
    unsigned long long tBusy;
    unsigned long long tIdle;
#else
    #error "Don't have a CPU usage implemenatation for this OS"
#endif
};


int countDigits(int n);
int min(int a, int b);
int max(int a, int b);
int setProgramName(char* name);
noreturn void showUsage(int status);
noreturn void showVersion(int status);
noreturn void showError(int status, bool shouldShowUsage, const char* format, ...);
bool getCPUUsage(float* usage);
bool getMemUsage(float* usage);

