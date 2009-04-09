/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMath.cpp */

#define epicsExportSharedSymbols
#include <epicsMath.h>

static float makeNAN ( void )
{
    float a,b,c;
    a = 0.0;
    b = 0.0;
    c = a / b;
    return c;
}

static float makeINF ( void )
{
    float a,b,c;
    a = 1.0;
    b = 0.0;
    c = a / b;
    return c;
}

epicsShareDef float epicsNAN = makeNAN();
epicsShareDef float epicsINF = makeINF();
