
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include <osiFileName.h>

#ifndef PATH_MAX
#  define PATH_MAX 100
#endif

char *epicsGetExecName(void)
{
    size_t max = PATH_MAX;
    char *ret = NULL;
    ssize_t n;

    while(1) {
        char *temp = realloc(ret, max);
        if(!temp) {
            /* we treat alloc failure as terminal */
            free(ret);
            ret = NULL;
            break;
        }
        ret = temp;

        n = readlink("/proc/self/exe", ret, max);
        if(n == -1) {
            free(ret);
            ret = NULL;
            break;
        } else if(n < max) {
            /* readlink() never adds a nil */
            ret[n] = '\0';
            break;
        }

        max += 64;
    }

    return ret;
}

char *epicsGetExecDir(void)
{
    char *ret = epicsGetExecName();
    if(ret) {
        char *sep = strrchr(ret, '/');
        if(sep) {
            /* nil the charactor after the / */
            sep[1] = '\0';
        }
    }
    return ret;
}
