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
