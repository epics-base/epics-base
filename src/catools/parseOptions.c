/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
*     Synchrotronstrahlung.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 *  $Id$
 *
 *  Author: Ralph Lange (BESSY)
 *
 */

/* IMPORTANT NOTE:
 * ---------------
 * This library is a workaround for achieving getopt() functionality in
 * EPICS Base 3.14.5 - it will be replaced as soon as possible without
 * further notice.
 *
 * DO NOT WRITE ANY CODE BASED ON THIS LIBRARY - IT WILL BREAK.
 *
 */

#include <string.h>
#include "parseOptions.h"

int optIndex = 1;
char *optArgument = NULL;
char optChar;

static unsigned int charIndex;

int parseOptions (int argc, char * const argv[], const char *options)
{
    unsigned int i;
    char *opt;

    if (optIndex >= argc) return -1;
    opt = argv[optIndex];

    if (charIndex >= strlen(opt)) /* End of option group */
    {
        charIndex = 0;
        optIndex++;
    }

    if (charIndex == 0)         /* Search for next option group */
    {
        for (; optIndex < argc; optIndex++)
        {
            if (*(opt = argv[optIndex]) == '-')
            {
                if (*(opt+1) != '-')
                {               /* New option group found */
                    charIndex++;
                    break;
                } else
                    continue;
            }
                                /* No more options */
            return -1;
        }
        if (charIndex == 0) return -1; /* No more options */
    }

    optChar = opt[charIndex];   /* Test candidate */

    for (i = 0; i < strlen(options); i++) /* Check for validity */
    {
        if (optChar == options[i]) /* Found option in option string! */
        {
            if (options[i+1] == ':') /* Option has an argument? */
            {                           /* Set optArgument accordingly */
                optIndex++;     /* Point to next argument */
                if (charIndex+1 >= strlen(opt))
                {               /* End of option group */
                    optArgument = argv[optIndex];
                    charIndex = 0;
                    if (*optArgument == '-')
                        return options[0] == ':' ? ':' : '?';
                    optIndex++;
                } else {                /* point to remaining argument */
                    optArgument = opt + charIndex+1;
                }
                charIndex = 0;
            } else {
                optArgument = 0;
                charIndex++;
            }
            return optChar;
        }
    }
                                /* Option not found */
    charIndex++;
    return '?';
}
