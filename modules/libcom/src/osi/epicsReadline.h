/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INC_epicsReadline_H
#define INC_epicsReadline_H

#ifdef __cplusplus
extern "C" {
#endif

#include <shareLib.h>
#include <stdio.h>

epicsShareFunc void * epicsShareAPI epicsReadlineBegin (FILE *in);
epicsShareFunc char * epicsShareAPI epicsReadline (const char *prompt, void *context);
epicsShareFunc void   epicsShareAPI epicsReadlineEnd (void *context);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsReadline_H */
