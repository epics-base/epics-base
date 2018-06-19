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
#ifndef _EPICS_GETOPT_H
#define _EPICS_GETOPT_H

#ifdef _MINGW

#include <unistd.h>

#else /* _MINGW */

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int getopt(int argc, char * const argv[], const char *optstring);

epicsShareExtern char *optarg;

epicsShareExtern int optind, opterr, optopt;

#ifdef __cplusplus
}
#endif

#endif /* _MINGW */

#endif /* _EPICS_GETOPT_H */
