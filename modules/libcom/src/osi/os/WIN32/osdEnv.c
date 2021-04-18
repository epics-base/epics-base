/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osdEnv.c */
/*
 * Author: Eric Norum
 *   Date: May 7, 2001
 *
 * Routines to modify/display environment variables and EPICS parameters
 *
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "epicsStdio.h"
#include "epicsString.h"
#include "errlog.h"
#include "envDefs.h"
#include "osiUnistd.h"
#include "epicsFindSymbol.h"
#include "iocsh.h"

static
void setEnv(const char *name, const char *value)
{
    errno_t err = _putenv_s(name, value);
    if(err)
        errlogPrintf("Can't set environment %s=\"%s\" : %d\n", name, value, (int)err);
}

/*
 * Set the value of an environment variable
 * Leaks memory, but the assumption is that this routine won't be
 * called often enough for the leak to be a problem.
 */
LIBCOM_API void epicsStdCall epicsEnvSet (const char *name, const char *value)
{
    iocshEnvClear(name);
    setEnv(name, value);
}

/*
 * Unset an environment variable
 * Using putenv with a an existing name plus "=" (without value) deletes
 */

LIBCOM_API void epicsStdCall epicsEnvUnset (const char *name)
{
    iocshEnvClear(name);
    setEnv(name, "");
}

/*
 * Show the value of the specified, or all, environment variables
 */
LIBCOM_API void epicsStdCall epicsEnvShow (const char *name)
{
    char **sp;

    for (sp = environ ; (sp != NULL) && (*sp != NULL) ; sp++) {
        if (!name || epicsStrnGlobMatch(*sp, strchr(*sp, '=') - *sp, name))
            printf ("%s\n", *sp);
    }
}
