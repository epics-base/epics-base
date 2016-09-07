
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

#ifndef casCoreClienth
#define casCoreClienth

#include "caServerI.h"
#include "ioBlocked.h"
#include "casMonitor.h"
#include "casEventSys.h"
#include "casCtx.h"

class casClientMutex : public epicsMutex {
};

//
// casCoreClient
// (this will eventually support direct communication
// between the client lib and the server lib)
//
class casCoreClient : public ioBlocked,
    private casMonitorCallbackInterface {
public:
	casCoreClient ( caServerI & serverInternal ); 
	virtual ~casCoreClient ();
	virtual void show ( unsigned level ) const;

    void installAsynchIO ( class casAsyncPVAttachIOI & io );
    void uninstallAsynchIO ( class casAsyncPVAttachIOI & io );
    void installAsynchIO ( class casAsyncPVExistIOI & io );
    void uninstallAsynchIO ( class casAsyncPVExistIOI & io );

	caServerI & getCAS () const;

	//
	// one virtual function for each CA request type that has
	// asynchronous completion
	//
	virtual caStatus asyncSearchResponse (
        epicsGuard < casClientMutex > &, const caNetAddr & outAddr, 
		const caHdrLargeArray &, const pvExistReturn &,
        ca_uint16_t protocolRevision, ca_uint32_t sequenceNumber );
	virtual caStatus createChanResponse (
        epicsGuard < casClientMutex > &,
	    casCtx &, const pvAttachReturn &);
	virtual caStatus readResponse (
        epicsGuard < casClientMutex > &,
		casChannelI *, const caHdrLargeArray &, 
        const gdd &, const caStatus ); 
	virtual caStatus readNotifyResponse (
        epicsGuard < casClientMutex > &,
		casChannelI *, const caHdrLargeArray &, 
        const gdd &, const caStatus );
	virtual caStatus writeResponse ( 
        epicsGuard < casClientMutex > &, casChannelI &, 
        const caHdrLargeArray &, const caStatus );
	virtual caStatus writeNotifyResponse ( 
        epicsGuard < casClientMutex > &, casChannelI &, 
        const caHdrLargeArray &, const caStatus );
	virtual caStatus monitorResponse ( 
        epicsGuard < casClientMutex > &, casChannelI &, 
        const caHdrLargeArray &, const gdd &, 
        const caStatus status );
	virtual caStatus accessRightsResponse ( 
        epicsGuard < casClientMutex > &, casChannelI * );
    virtual caStatus enumPostponedCreateChanResponse ( 
        epicsGuard < casClientMutex > &,
        casChannelI &, const caHdrLargeArray & );
	virtual caStatus channelCreateFailedResp ( 
        epicsGuard < casClientMutex > &,
        const caHdrLargeArray &, const caStatus createStatus );
    virtual caStatus channelDestroyEventNotify ( 
        epicsGuard < casClientMutex > & guard, 
        casChannelI * const pChan, ca_uint32_t sid );
    virtual void casChannelDestroyFromInterfaceNotify ( 
       casChannelI & chan, bool immediateDestroyNeeded );

	virtual ca_uint16_t protocolRevision () const = 0;

	// used only with DG clients 
	virtual caNetAddr fetchLastRecvAddr () const;
    virtual ca_uint32_t datagramSequenceNumber () const;

    bool okToStartAsynchIO ();
	void setDestroyPending ();

    casProcCond eventSysProcess();

	caStatus addToEventQueue ( casAsyncIOI &, 
        bool & onTheQueue, bool & posted );
    void removeFromEventQueue ( casAsyncIOI &, 
        bool & onTheEventQueue );
	void addToEventQueue ( 
        casChannelI &, bool & inTheEventQueue );
    void removeFromEventQueue ( class casChannelI &, 
        bool & inTheEventQueue );
    void addToEventQueue ( class channelDestroyEvent & ev );
    void enableEvents ();
    void disableEvents ();
    caStatus casMonitorCallBack ( 
        epicsGuard < casClientMutex > &, 
        casMonitor &, const gdd & );
    void postEvent ( tsDLList <casMonitor > &, 
        const casEventMask &select, const gdd &event );

    casMonitor & monitorFactory ( 
        casChannelI & ,
        caResId clientId, 
        const unsigned long count, 
        const unsigned type, 
        const casEventMask & );
    void destroyMonitor ( casMonitor & mon );

    void casMonEventDestroy ( 
        casMonEvent &, epicsGuard < evSysMutex > & );

protected:
    casEventSys eventSys;
    mutable casClientMutex mutex;
	casCtx ctx;
    bool userStartedAsyncIO;

private:
    // for io that does not have a channel
	tsDLList < casAsyncIOI > ioList;

    // not pure because the base class noop must be called
    // when in the destructor
    virtual void eventSignal ();

	casCoreClient ( const casCoreClient & );
	casCoreClient & operator = ( const casCoreClient & );
};

inline caServerI & casCoreClient::getCAS() const
{
	return *this->ctx.getServer();
}

inline bool casCoreClient::okToStartAsynchIO ()
{
    if ( ! this->userStartedAsyncIO ) {
        this->userStartedAsyncIO = true;
        return true;
    }
    return false;
}

inline void casCoreClient::postEvent ( 
    tsDLList < casMonitor > & monitorList, 
    const casEventMask & select, const gdd & event )
{
    bool signalNeeded = 
        this->eventSys.postEvent ( monitorList, select, event );
    if ( signalNeeded ) {
        this->eventSignal ();
    }
}

inline casProcCond casCoreClient :: eventSysProcess ()
{
    epicsGuard < casClientMutex > guard ( this->mutex );
	return this->eventSys.process ( guard );
}

inline caStatus casCoreClient::addToEventQueue ( casAsyncIOI & io, 
                      bool & onTheQueue, bool & posted )
{
    bool wakeupNeeded;
	caStatus status =  this->eventSys.addToEventQueue ( io, 
        onTheQueue, posted, wakeupNeeded );
	if ( wakeupNeeded ) {
		this->eventSignal ();
	}
    return status;
}

inline void casCoreClient::removeFromEventQueue ( 
    casAsyncIOI & io, bool & onTheEventQueue )
{
    this->eventSys.removeFromEventQueue ( io, onTheEventQueue );
}

inline void casCoreClient::addToEventQueue ( 
    casChannelI & ev, bool & inTheEventQueue )
{
	bool signalNeeded = 
        this->eventSys.addToEventQueue ( ev, inTheEventQueue );
	if ( signalNeeded ) {
		this->eventSignal ();
	}
}

inline void casCoreClient::removeFromEventQueue ( class casChannelI & io, 
    bool & inTheEventQueue )
{
    this->eventSys.removeFromEventQueue ( io, inTheEventQueue );
}

inline void casCoreClient::addToEventQueue ( class channelDestroyEvent & ev )
{
    bool wakeUpNeeded = this->eventSys.addToEventQueue ( ev );
    if ( wakeUpNeeded ) {
		this->eventSignal ();
    }
}

inline void casCoreClient::enableEvents ()
{
    this->eventSys.eventsOn ();
    this->eventSignal (); // wake up the event queue consumer
}

inline void casCoreClient::disableEvents ()
{
    bool signalNeeded =
        this->eventSys.eventsOff ();
    if ( signalNeeded ) {
		this->eventSignal ();
    }
}

inline void casCoreClient::setDestroyPending ()
{
    this->eventSys.setDestroyPending ();
    this->eventSignal ();
}

inline void casCoreClient::casMonEventDestroy ( 
    casMonEvent & ev, epicsGuard < evSysMutex > & guard )
{
    this->eventSys.casMonEventDestroy ( ev, guard );
}

#endif // casCoreClienth

