/*************************************************************************\
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *
 * Macro expansion of environment variables
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define epicsExportSharedSymbols
#include <epicsAssert.h>
#include <epicsString.h>
#include "macLib.h"

char * epicsShareAPI
macEnvExpand(const char *str)
{
    MAC_HANDLE *handle;
    static char *pairs[] = { "", "environ", NULL, NULL };
    long status;
    int destCapacity = 200;
    char *dest = NULL; 
    int n;

    status = macCreateHandle(&handle, pairs);
    assert(status == 0);
    do {
        destCapacity *= 2;
        /*
         * Use free/malloc rather than realloc since there's no need to
         * bother copying the contents if realloc needs to move the buffer
         */
        free(dest);
        dest = malloc(destCapacity);
        assert(dest != 0);
        n = macExpandString(handle, str, dest, destCapacity);
    } while (n >= (destCapacity - 1));
    if (n < 0) {
        free(dest);
        dest = NULL;
    }
    else {
        int unused = destCapacity - ++n;
        if (unused >= 20)
            dest = realloc(dest, n);
    }
    status = macDeleteHandle(handle);
    assert(status == 0);
    return dest;
}
