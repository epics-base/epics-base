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
macEnvExpand(char *str)
{
    MAC_HANDLE *handle;
    static char *pairs[] = { "", "environ", NULL, NULL };
    int destCapacity = 200;
    char *dest = NULL; 
    int n;
    char *ret;

    assert(macCreateHandle(&handle, pairs) == 0);
    do {
        destCapacity *= 2;
        assert((dest = realloc(dest, destCapacity)) != 0);
        n = macExpandString(handle, str, dest, destCapacity);
    } while (n >= (destCapacity - 1));
    if (n < 0)
        ret = NULL;
    else
        ret = epicsStrDup(dest);
    free(dest);
    assert(macDeleteHandle(handle) == 0);
    return ret;
}
