/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Revision-Id$
 *
 * Macro expansion of environment variables
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "errlog.h"
#include "epicsString.h"
#include "macLib.h"

char * epicsShareAPI
macEnvExpand(const char *str)
{
    MAC_HANDLE *handle;
    static char *pairs[] = { "", "environ", NULL, NULL };
    long destCapacity = 128;
    char *dest = NULL;
    int n;

    if (macCreateHandle(&handle, pairs)){
        errlogMessage("macEnvExpand: macCreateHandle failed.");
        return NULL;
    }

    do {
        destCapacity *= 2;
        /*
         * Use free/malloc rather than realloc since there's no need to
         * keep the original contents.
         */
        free(dest);
        dest = malloc(destCapacity);
        if(!dest)
            goto done;

        n = macExpandString(handle, str, dest, destCapacity);
    } while (n >= (destCapacity - 1));

    if (n < 0) {
        free(dest);
        dest = NULL;
    } else {
        size_t unused = destCapacity - ++n;

        if (unused >= 20)
            dest = realloc(dest, n);
    }

done:
    if (macDeleteHandle(handle))
        errlogMessage("macEnvExpand: macDeleteHandle failed.");
    return dest;
}
