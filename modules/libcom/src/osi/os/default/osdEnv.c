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
#include "epicsVersion.h"
#include "errlog.h"
#include "envDefs.h"
#include "osiUnistd.h"
#include "epicsFindSymbol.h"
#include "iocsh.h"

#ifdef __rtems__
#  include <rtems.h>
#  define RTEMS_VERSION_INT VERSION_INT(__RTEMS_MAJOR__, __RTEMS_MINOR__, 0, 0)
#endif

#if defined(__RTEMS_MAJOR__) && RTEMS_VERSION_INT<VERSION_INT(4,10,0,0)
   /* newlib w/ RTEMS <=4.9 returns void */
#  define unSetEnv(name) ({unsetenv(name); 0;})
#else
#  define unSetEnv(name) unsetenv(name)
#endif

/*
 * Set the value of an environment variable
 * Leaks memory, but the assumption is that this routine won't be
 * called often enough for the leak to be a problem.
 */
LIBCOM_API void epicsStdCall epicsEnvSet (const char *name, const char *value)
{
    iocshEnvClear(name);
    if(setenv(name, value, 1))
        errlogPrintf("setenv(\"%s\", \"%s\") -> %d\n", name, value, errno);
}

/*
 * Unset an environment variable
 * Using putenv with a an existing name but without "=..." deletes
 */

LIBCOM_API void epicsStdCall epicsEnvUnset (const char *name)
{
    iocshEnvClear(name);
    if(unSetEnv(name))
        errlogPrintf("unsetenv(\"%s\") -> %d\n", name, errno);
}

/*
 * Show the value of the specified, or all, environment variables
 */
LIBCOM_API void epicsStdCall epicsEnvShow (const char *name)
{
    extern char **environ;
    char **sp;

    for (sp = environ ; (sp != NULL) && (*sp != NULL) ; sp++) {
        if (!name || epicsStrnGlobMatch(*sp, strchr(*sp, '=') - *sp, name))
            printf ("%s\n", *sp);
    }
}
