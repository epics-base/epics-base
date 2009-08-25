/*************************************************************************\
* Copyright (c) 2009 UChicago Argonna LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMath.cpp */

#define epicsExportSharedSymbols
#include <epicsMath.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4723)
#endif

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

extern "C" {
epicsShareDef float epicsNAN = makeNAN();
epicsShareDef float epicsINF = makeINF();
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

