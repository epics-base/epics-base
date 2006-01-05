/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsStdlib.h*/
/*Author: Eric Norum */

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsScanDouble(const char *str, double *dest);
epicsShareFunc int epicsScanFloat(const char *str, float *dest);

#include <stdlib.h>
#include <osdStrtod.h>

#ifdef __cplusplus
}
#endif

