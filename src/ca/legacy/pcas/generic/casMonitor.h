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

#ifndef casMonitorh
#define casMonitorh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casMonitorh
#   undef epicsExportSharedSymbols
#endif

#include "tsDLList.h"

#ifdef epicsExportSharedSymbols_casMonitorh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif


#include "caHdrLargeArray.h"
#include "casMonEvent.h"

class casMonitor;
class casClientMutex;

class casMonitorCallbackInterface {
public:
	virtual caStatus casMonitorCallBack ( 
        epicsGuard < casClientMutex > &, casMonitor &,
        const gdd & ) = 0;
protected:
    virtual ~casMonitorCallbackInterface() {}
};

class casEvent;

class casMonitor : public tsDLNode < casMonitor > {
public:
	casMonitor ( caResId clientIdIn, casChannelI & chan, 
	    ca_uint32_t nElem, unsigned dbrType,
	    const casEventMask & maskIn, 
        casMonitorCallbackInterface & );
	virtual ~casMonitor();
    void markDestroyPending ();
    void installNewEventLog ( 
        tsDLList < casEvent > & eventLogQue, 
        casMonEvent * pLog, const gdd & event );
    void show ( unsigned level ) const;
    bool selected ( const casEventMask & select ) const;
    bool matchingClientId ( caResId clientIdIn ) const;
    unsigned numEventsQueued () const;
    caStatus response ( 
        epicsGuard < casClientMutex > &, casCoreClient & client,
        const gdd & value );
	caStatus executeEvent ( casCoreClient &, 
        casMonEvent &, const gdd &,
        epicsGuard < casClientMutex > &,
        epicsGuard < evSysMutex > & );
    void * operator new ( size_t size, 
        tsFreeList < casMonitor, 1024 > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < casMonitor, 1024 > & ))
private:
	casMonEvent overFlowEvent;
	ca_uint32_t const nElem;
	casChannelI * pChannel;
    casMonitorCallbackInterface & callBackIntf;
	const casEventMask mask;
	caResId const clientId;
	unsigned char const dbrType;
	unsigned char nPend;
    bool destroyPending;
	bool ovf;
    void operator delete ( void * );
	casMonitor ( const casMonitor & );
	casMonitor & operator = ( const casMonitor & );
};

inline unsigned casMonitor::numEventsQueued () const
{
    return this->nPend;
}

inline void casMonitor::markDestroyPending ()
{    this->pChannel = 0;
}

inline bool casMonitor::matchingClientId ( caResId clientIdIn ) const
{
    return clientIdIn == this->clientId;
}

inline bool casMonitor::selected ( const casEventMask & select ) const
{
    casEventMask result ( select & this->mask );
    return result.eventsSelected () && this->pChannel;
}

#endif // casMonitorh
