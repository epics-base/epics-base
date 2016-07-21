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

/* This is needed for vxWorks 6.8 to prevent an obnoxious compiler warning */
#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <envLib.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsFindSymbol.h"
#include "epicsStdio.h"
#include "errlog.h"
#include "iocsh.h"

/*
 * Set the value of an environment variable
 * Leaks memory, but the assumption is that this routine won't be
 * called often enough for the leak to be a problem.
 */
epicsShareFunc void epicsShareAPI epicsEnvSet (const char *name, const char *value)
{
    char *cp;

    iocshEnvClear(name);
    
    cp = mallocMustSucceed (strlen (name) + strlen (value) + 2, "epicsEnvSet");
    strcpy (cp, name);
    strcat (cp, "=");
    strcat (cp, value);
    if (putenv (cp) < 0) {
        errPrintf(-1L, __FILE__, __LINE__,
            "Failed to set environment parameter \"%s\" to \"%s\": %s\n",
            name, value, strerror (errno));
        free (cp);
    }
}

/*
 * Show the value of the specified, or all, environment variables
 */
epicsShareFunc void epicsShareAPI epicsEnvShow (const char *name)
{
    if (name == NULL) {
        envShow (0);
    }
    else {
        const char *cp = getenv (name);
        if (cp == NULL)
            printf ("%s is not an environment variable.\n", name);
        else
            printf ("%s=%s\n", name, cp);
    }
}
