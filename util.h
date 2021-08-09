#ifdef __APPLE__
#include <mach/mach.h>
#endif
#include <stdbool.h>      // for bool
#include <stdnoreturn.h>  // for noreturn


#define AUTHORS "Peter Frost"
#define PROGRAM_NAME "procprog"
#define VERSION "0.1"
#define BUILD_YEAR  (&(__DATE__)[7])


unsigned printable_strlen(const char *str);
int setProgramName(char* name);
const char** getArgs(int argc, char** argv);
noreturn void showUsage(int status);
noreturn void showVersion(int status);
noreturn void showError(int status, bool shouldShowUsage, const char* format, ...);
float proc_runtime(void);
