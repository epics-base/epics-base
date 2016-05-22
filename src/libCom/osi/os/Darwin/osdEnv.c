/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
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
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/*
 * Starting in Mac OS X 10.5 (Leopard) shared libraries and
 * bundles don't have direct access to environ (man environ).
 */
#include <crt_externs.h>
#define environ (*_NSGetEnviron())

#define epicsExportSharedSymbols
#include "epicsStdio.h"
#include "envDefs.h"
#include "iocsh.h"

/*
 * Set the value of an environment variable
 */
epicsShareFunc void epicsShareAPI epicsEnvSet (const char *name, const char *value)
{
    iocshEnvClear(name);
    setenv(name, value, 1);
}

/*
 * Show the value of the specified, or all, environment variables
 */
epicsShareFunc void epicsShareAPI epicsEnvShow (const char *name)
{
    if (name == NULL) {
        extern char **environ;
        char **sp;

        for (sp = environ ; (sp != NULL) && (*sp != NULL) ; sp++)
            printf ("%s\n", *sp);
    }
    else {
        const char *cp = getenv (name);
        if (cp == NULL)
            printf ("%s is not an environment variable.\n", name);
        else
            printf ("%s=%s\n", name, cp);
    }
}
