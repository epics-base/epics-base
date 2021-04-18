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

/* This is needed for vxWorks 6.8 to prevent an obnoxious compiler warning */
#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <envLib.h>

#include "epicsFindSymbol.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "errlog.h"
#include "iocsh.h"


/*
 * Set the value of an environment variable
 * Leaks memory, but the assumption is that this routine won't be
 * called often enough for the leak to be a problem.
 */
LIBCOM_API void epicsStdCall epicsEnvSet (const char *name, const char *value)
{
    size_t alen = (!name || !value) ? 2u : strlen(name) + strlen(value) + 2u; /* <NAME> '=' <VALUE> '\0' */
    const int onstack = alen <= 512u; /* use on-stack dynamic array for small strings */
    char stackarr[ onstack ? alen     : 1u]; /* gcc specific dynamic array */
    char *allocd = onstack ? NULL     : malloc(alen);
    char *cp     = onstack ? stackarr : allocd;

    if (!name || !value) {
        printf ("Usage: epicsEnvSet \"name\", \"value\"\n");

    } else if(!cp) {
        errlogPrintf("epicsEnvSet(\"%s\", \"%s\" insufficient memory\n", name, value);

    } else {
        int err;

        strcpy (cp, name);
        strcat (cp, "=");
        strcat (cp, value);

        iocshEnvClear(name);

        if((err=putenv(cp)) < 0)
            errlogPrintf("epicsEnvSet(\"%s\", \"%s\" -> %d\n", name, value, err);
    }
    /* from at least vxWorks 5.5 putenv() is making a copy, so we can free */
    free (allocd);
}

/*
 * Unset an environment variable
 * Basically destroy the name of that variable because vxWorks does not
 * support to really unset an environment variable.
 */

LIBCOM_API void epicsStdCall epicsEnvUnset (const char *name)
{
    char* var;

    if (!name) return;
    iocshEnvClear(name);
    var = getenv(name);
    if (!var) return;
    var -= strlen(name);
    var --;
    *var = 0;
}

/*
 * Show the value of the specified, or all, environment variables
 */
LIBCOM_API void epicsStdCall epicsEnvShow (const char *name)
{
    extern char **ppGlobalEnviron; /* Used in 'environ' macro but not declared in envLib.h */
    char **sp;

    for (sp = environ ; (sp != NULL) && (*sp != NULL) ; sp++) {
        if (!**sp) continue; /* skip unset environment variables */
        if (!name || epicsStrnGlobMatch(*sp, strchr(*sp, '=') - *sp, name))
            printf ("%s\n", *sp);
    }
}
