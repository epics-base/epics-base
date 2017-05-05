/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

#ifndef JLINKZ_H
#define JLINKZ_H

#include <dbJLink.h>
#include <epicsMutex.h>
#include <epicsTypes.h>

#include <shareLib.h>

epicsShareExtern
int numzalloc;

typedef struct {
    jlink base;
    epicsMutexId lock;
    unsigned isset:1;
    unsigned isopen:1;
    epicsInt32 value;
} zpriv;

#endif /* JLINKZ_H */
