/*************************************************************************\
* Copyright (c) 2009 UChicago Argonna LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMath.cpp */

#define epicsExportSharedSymbols
#include <epicsMath.h>

static float makeNAN ( void )
{
    float a = 0, b = 0;
    return a / b;
}

static float makeINF ( void )
{
    float a = 1, b = 0;
    return a / b;
}

epicsShareDef float epicsNAN = makeNAN();
epicsShareDef float epicsINF = makeINF();
