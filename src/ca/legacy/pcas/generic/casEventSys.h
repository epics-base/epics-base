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

#ifndef casEventSysh
#define casEventSysh

#if defined ( epicsExportSharedSymbols  )
#   undef epicsExportSharedSymbols
#   define casEventSysh_restore_epicsExportSharedSymbols
#endif

#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsMutex.h"

#if defined ( casEventSysh_restore_epicsExportSharedSymbols )
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casdef.h"
#include "casEvent.h"

/*
 * maximum peak log entries for each event block (registartion)
 * (events cached into the last queue entry if over flow occurs)
 * (if this exceeds 256 then the casMonitor::nPend must
 * be assigned a new data type)
 */
#define individualEventEntries 16u

/*
 * maximum average log entries for each event block (registartion)
 * (events cached into the last queue entry if over flow occurs)
 * (if this exceeds 256 then the casMonitor::nPend must
 * be assigned a new data type)
 */
#define averageEventEntries 4u

enum casProcCond { casProcOk, casProcDisconnect };

class casMonitor;
class casMonEvent;
class casCoreClient;

template < class MUTEX > class epicsGuard;

class evSysMutex : public epicsMutex {};

class casEventSys {
public:
	casEventSys ( casCoreClient & );
	~casEventSys ();
	void show ( unsigned level ) const;
	casProcCond process ( epicsGuard < casClientMutex > & guard );
	void installMonitor ();
	void removeMonitor ();
    void prepareMonitorForDestroy ( casMonitor & mon );
    bool postEvent ( tsDLList < casMonitor > & monitorList, 
        const casEventMask & select, const gdd & event );
	caStatus addToEventQueue ( class casAsyncIOI &, 
        bool & onTheQueue, bool & posted, bool & signalNeeded );
    void removeFromEventQueue ( class casAsyncIOI &, 
        bool & onTheEventQueue );
	bool addToEventQueue ( 
        casChannelI &, bool & inTheEventQueue );
    void removeFromEventQueue ( class casChannelI &, 
        bool & inTheEventQueue );
	bool addToEventQueue ( class channelDestroyEvent & );
	bool getNDuplicateEvents () const;
	void setDestroyPending ();
	void eventsOn ();
	bool eventsOff ();
    void casMonEventDestroy ( 
        casMonEvent &, epicsGuard < evSysMutex > & );
private:
    mutable evSysMutex mutex;
	tsDLList < casEvent > eventLogQue;
	tsDLList < casEvent > ioQue;
    tsFreeList < casMonEvent, 1024, epicsMutexNOOP > casMonEventFreeList;
    casCoreClient & client;
	class casEventPurgeEv * pPurgeEvent; // flow control purge complete event
	unsigned numSubscriptions; // N subscriptions installed
	unsigned maxLogEntries; // max log entries
	bool destroyPending;
	bool replaceEvents; // replace last existing event on queue
	bool dontProcessSubscr; // flow ctl is on - dont process subscr event queue

	bool full () const;
	casEventSys ( const casEventSys & );
	casEventSys & operator = ( const casEventSys & );
    friend class casEventPurgeEv;
};

/*
 * when this event reaches the top of the queue we
 * know that all duplicate events have been purged
 * and that now no events should not be sent to the
 * client until it exits flow control mode
 */
class casEventPurgeEv : public casEvent {
public:
    casEventPurgeEv ( class casEventSys & );
private:
    casEventSys & evSys;
	caStatus cbFunc ( 
        casCoreClient &, 
        epicsGuard < casClientMutex > &,
        epicsGuard < evSysMutex > & );
    casEventPurgeEv & operator = ( const casEventPurgeEv & ); 
    casEventPurgeEv ( const casEventPurgeEv & ); 
};

//
// casEventSys::casEventSys ()
//
inline casEventSys::casEventSys ( casCoreClient & clientIn ) :
    client ( clientIn ),
	pPurgeEvent ( NULL ),
	numSubscriptions ( 0u ),
	maxLogEntries ( individualEventEntries ),
	destroyPending ( false ),
	replaceEvents ( false ), 
	dontProcessSubscr ( false ) 
{
}

#endif // casEventSysh

