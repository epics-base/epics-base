/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include <osiFileName.h>

char *epicsGetExecName(void)
{
    size_t max = 128;
    char *ret = NULL;
    DWORD n;

    while(1) {
        char *temp = realloc(ret, max);
        if(!temp) {
            /* we treat alloc failure as terminal */
            free(ret);
            ret = NULL;
            break;
        }
        ret = temp;

        n = GetModuleFileName(NULL, ret, max);
        if(n == 0) {
            free(ret);
            ret = NULL;
            break;
        } else if(n < max) {
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
        char *sep = strrchr(ret, '\\');
        if(sep) {
            /* nil the character after the / */
            sep[1] = '\0';
        }
    }
    return ret;
}
