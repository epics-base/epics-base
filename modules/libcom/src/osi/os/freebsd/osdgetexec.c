/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/sysctl.h>

#include <osiFileName.h>

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

        n = readlink("/proc/curproc/file", ret, max);
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

    if(!ret) {
        int mib[4];
        size_t cb = max;
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PATHNAME;
        mib[3] = -1;

        ret = malloc(max);
        if(ret) {
            sysctl(mib, 4, ret, &cb, NULL, 0);
            /* TODO: error check */
        }
    }

    return ret;
}

char *epicsGetExecDir(void)
{
    char *ret = epicsGetExecName();
    if(ret) {
        char *sep = strrchr(ret, '/');
        if(sep) {
            /* nil the character after the / */
            sep[1] = '\0';
        }
    }
    return ret;
}
