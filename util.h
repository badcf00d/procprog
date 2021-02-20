
#define AUTHORS "Peter Frost"
#define PROGRAM_NAME "procprog"
#define VERSION "0.1"
#define BUILD_YEAR  (__DATE__ + 7)


int countDigits(int n);
int min(int a, int b);
int max(int a, int b);
int setProgramName(char* name);
noreturn void showUsage(int status);
noreturn void showVersion(int status);
noreturn void showError(int status, bool shouldShowUsage, const char* format, ...);
