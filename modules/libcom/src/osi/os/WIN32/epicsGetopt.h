/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
*     Synchrotronstrahlung.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef _EPICS_GETOPT_H
#define _EPICS_GETOPT_H

#ifdef _MINGW

#include <unistd.h>

#else /* _MINGW */

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API int getopt(int argc, char * const argv[], const char *optstring);

LIBCOM_API extern char *optarg;

LIBCOM_API extern int optind, opterr, optopt;

#ifdef __cplusplus
}
#endif

#endif /* _MINGW */

#endif /* _EPICS_GETOPT_H */
