/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef casEventh
#define casEventh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casEventh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "tsDLList.h"

#ifdef epicsExportSharedSymbols_casEventh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casdef.h"

class casCoreClient;

class evSysMutex;
class casClientMutex;
template < class MUTEX > class epicsGuard;

class casEvent : public tsDLNode < casEvent > {
public:
    virtual caStatus cbFunc ( 
        casCoreClient &, 
        epicsGuard < casClientMutex > &,
        epicsGuard < evSysMutex > & ) = 0;
protected:
    epicsShareFunc virtual ~casEvent();
};

#endif // casEventh

