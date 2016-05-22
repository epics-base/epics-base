/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// casOSD.h - Channel Access Server OS dependent wrapper
// 

#ifndef includeCASOSDH 
#define includeCASOSDH 

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_includeCASOSDH
#   undef epicsExportSharedSymbols
#endif
#include "fdManager.h"
#ifdef epicsExportSharedSymbols_includeCASOSDH
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class caServerI;

class casServerReg;

class casDGReadReg;
class casDGBCastReadReg;
class casDGWriteReg;






class casStreamWriteReg;
class casStreamReadReg;




// no additions below this line
#endif // includeCASOSDH 

