
#ifdef __cplusplus
extern "C" {
#endif

#ifdef vxWorks
#include <stdarg.h>
int epicsPrintf(const char *pFormat, ...);
int epicsVprintf (const char *pFormat, va_list pvar);
int iocLogVPrintf(const char *pFormat, va_list pvar);
int iocLogPrintf(const char *pFormat, ...);
#else
#define epicsPrintf printf
#define epicsVprintf vprintf
#endif

#ifdef __cplusplus
}
#endif

