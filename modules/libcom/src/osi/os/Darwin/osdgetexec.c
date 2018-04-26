
#include <string.h>
#include <stdlib.h>

#include <mach-o/dyld.h>

#define epicsExportSharedSymbols
#include <osiFileName.h>

char *epicsGetExecName(void)
{
    uint32_t max = 64u;
    char *ret = NULL;

    while(1) {
        char *temp = realloc(ret, max);
        if(!temp) {
            /* we treat alloc failure as terminal */
            free(ret);
            ret = NULL;
            break;
        }
        ret = temp;

        /* cf. "man 3 dyld" */
        if(_NSGetExecutablePath(ret, &max)==0) {
            /* max left unchanged */
            ret[max-1] = '\0';
            break;
        }
        /* max has been updated with required size */
    }

    /* TODO: _NSGetExecutablePath() doesn't follow symlinks */

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
