/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef includeCASIODH
#define includeCASIODH 

#ifdef epicsExportSharedSymbols
#   define ipIgnoreEntryEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "envDefs.h"
#include "resourceLib.h"
#include "tsFreeList.h"
#include "osiSock.h"
#include "inetAddrID.h"
#include "compilerDependencies.h"

#ifdef ipIgnoreEntryEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class caServerIO;
class casStreamOS;


// no additions below this line
#endif // includeCASIODH

