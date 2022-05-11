/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdlib.h>

#include <osiFileName.h>

char *epicsGetExecName(void)
{
    const char *raw = getexecname();
    char *ret = NULL;
    /* manpage says getexecname() might return a relative path.  we treat this as an error */
    if(raw[0]=='/') {
        ret = strdup(raw);
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
