/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdlib.h>

#include <mach-o/dyld.h>

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
            return NULL;
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

    /* Resolve soft-links */
    char *res = realpath(ret, NULL);
    free(ret);

    return res;
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
