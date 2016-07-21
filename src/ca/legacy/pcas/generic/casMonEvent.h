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

#ifndef casMonEventh
#define casMonEventh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casMonEventh
#   undef epicsExportSharedSymbols
#endif

#include "tsFreeList.h"
#include "smartGDDPointer.h"

#ifdef epicsExportSharedSymbols_casMonEventh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casEvent.h"

class casMonEvent : public casEvent {
public:
	casMonEvent ( class casMonitor & monitor );
	casMonEvent ( class casMonitor & monitor, const gdd & value );
	~casMonEvent ();
    void clear ();
	void assign ( const gdd & value );
    void swapValues ( casMonEvent & );
    void * operator new ( size_t size, 
        tsFreeList < casMonEvent, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < casMonEvent, 1024, epicsMutexNOOP > & ))
private:
    class casMonitor & monitor;
	smartConstGDDPointer pValue;
    void operator delete ( void * );
	caStatus cbFunc ( 
        casCoreClient &, 
        epicsGuard < casClientMutex > &,
        epicsGuard < evSysMutex > & );
	casMonEvent ( const casMonEvent & );
	casMonEvent & operator = ( const casMonEvent & );
};

inline casMonEvent::casMonEvent ( class casMonitor & monitorIn ) :
    monitor ( monitorIn ) {}

inline casMonEvent::casMonEvent ( 
    class casMonitor & monitorIn, const gdd & value ) :
        monitor ( monitorIn ), pValue ( value ) {}

inline void casMonEvent::clear ()
{
    this->pValue.set ( 0 );
}

inline void * casMonEvent::operator new ( size_t size, 
    tsFreeList < class casMonEvent, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#endif // casMonEventh

