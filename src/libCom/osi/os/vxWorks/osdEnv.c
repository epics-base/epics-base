/* osdEnv.c */
/*
 * $Id$
 *
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

#include <errlog.h>
#include <envDefs.h>
#include <cantProceed.h>

#define epicsExportSharedSymbols
#include "epicsFindSymbol.h"

/*
 * Set the value of an environment variable
 * Leaks memory, but the assumption is that this routine won't be
 * called often enough for the leak to be a problem.
 */
epicsShareFunc void epicsShareAPI epicsEnvSet (char *name, char *value)
{
    char *cp;

	cp = mallocMustSucceed (strlen (name) + strlen (value) + 2, "epicsEnvSet");
	strcpy (cp, name);
	strcat (cp, "=");
	strcat (cp, value);
	if (putenv (cp) < 0) {
		errPrintf(
                -1L,
                __FILE__,
                __LINE__,
                "Failed to set environment parameter \"%s\" to \"%s\": %s\n",
                name,
                value,
                strerror (errno));
        free (cp);
	}
}

/*
 * Show the value of the specified, or all, environment variables
 */
epicsShareFunc void epicsShareAPI epicsEnvShow (int argc, char **argv)
{
    if (argc <= 1) {
        envShow (0);
    }
    else {
        int i;

        for (i = 1 ; i < argc ; i++) {
            const char *cp = getenv (argv[i]);
            if (cp == NULL)
                printf ("%s is not an environment variable.\n", argv[i]);
            else
                printf ("%s=%s\n", argv[i], cp);
        }
    }
}
