
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef caServerIh
#define caServerIh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_caServerIh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "tsFreeList.h"
#include "caProto.h"

#ifdef epicsExportSharedSymbols_caServerIh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casdef.h"
#include "clientBufMemoryManager.h"
#include "casEventRegistry.h"
#include "caServerIO.h"
#include "ioBlocked.h"
#include "caServerDefs.h"

class casStrmClient;
class beaconTimer;
class beaconAnomalyGovernor;
class casIntfOS;
class casMonitor;
class casChannelI;

caStatus convertContainerMemberToAtomic (class gdd & dd,
         aitUint32 appType, aitUint32 requestedCount, aitUint32 nativeCount);

// Keep the old signature for backward compatibility
inline caStatus convertContainerMemberToAtomic (class gdd & dd,
         aitUint32 appType, aitUint32 elemCount)
{ return convertContainerMemberToAtomic(dd, appType, elemCount, elemCount); }

class caServerI : 
    public caServerIO, 
    public ioBlockedList, 
    public casEventRegistry {
public:
    caServerI ( caServer &tool );
    ~caServerI ();
    bool roomForNewChannel() const;
    unsigned getDebugLevel() const { return debugLevel; }
    inline void setDebugLevel ( unsigned debugLevelIn );
    void show ( unsigned level ) const;
    void destroyMonitor ( casMonitor & );
    caServer * getAdapter ();
    caServer * operator -> ();
    void connectCB ( casIntfOS & );
    casEventMask valueEventMask () const; // DBE_VALUE registerEvent("value")
    casEventMask logEventMask () const;     // DBE_LOG registerEvent("log") 
    casEventMask alarmEventMask () const; // DBE_ALARM registerEvent("alarm") 
    casEventMask propertyEventMask () const; // DBE_PROPERTY registerEvent("property") 
    unsigned subscriptionEventsProcessed () const;
    void incrEventsProcessedCounter ();
    unsigned subscriptionEventsPosted () const;
    void updateEventsPostedCounter ( unsigned nNewPosts );
    void generateBeaconAnomaly ();
    casMonitor & casMonitorFactory ( casChannelI &, 
        caResId clientId, const unsigned long count, 
        const unsigned type, const casEventMask &, 
        class casMonitorCallbackInterface & );
    void casMonitorDestroy ( casMonitor & );
    void destroyClient ( casStrmClient & );
    static void dumpMsg ( 
        const char * pHostName, const char * pUserName,
        const struct caHdrLargeArray * mp, const void * dp, 
        const char * pFormat, ... );
    bool ioIsPending () const;
    void incrementIOInProgCount ();
    void decrementIOInProgCount ();
private:
    clientBufMemoryManager clientBufMemMgr;
    tsFreeList < casMonitor, 1024 > casMonitorFreeList;
    tsDLList < casStrmClient > clientList;
    tsDLList < casIntfOS > intfList;
    mutable epicsMutex mutex;
    mutable epicsMutex diagnosticCountersMutex;
    caServer & adapter;
    beaconTimer & beaconTmr;
    beaconAnomalyGovernor & beaconAnomalyGov;
    unsigned debugLevel;
    unsigned nEventsProcessed; 
    unsigned nEventsPosted; 
    unsigned ioInProgressCount;

    casEventMask valueEvent; // DBE_VALUE registerEvent("value")
    casEventMask logEvent;  // DBE_LOG registerEvent("log")
    casEventMask alarmEvent; // DBE_ALARM registerEvent("alarm")
    casEventMask propertyEvent;  // DBE_PROPERTY registerEvent("property")

    caStatus attachInterface ( const caNetAddr & addr, bool autoBeaconAddr,
            bool addConfigAddr );

    virtual void addMCast(const osiSockAddr&);

    void sendBeacon ( ca_uint32_t beaconNo );

    caServerI ( const caServerI & );
    caServerI & operator = ( const caServerI & );

    friend class beaconAnomalyGovernor;
    friend class beaconTimer;
};


inline caServer * caServerI::getAdapter()
{
    return & this->adapter;
}

inline caServer * caServerI::operator -> ()
{
    return this->getAdapter();
}

inline void caServerI::setDebugLevel(unsigned debugLevelIn)
{
    this->debugLevel = debugLevelIn;
}

inline casEventMask caServerI::valueEventMask() const
{
    return this->valueEvent;
}

inline casEventMask caServerI::logEventMask() const
{
    return this->logEvent;
}

inline casEventMask caServerI::alarmEventMask() const
{
    return this->alarmEvent;
}

inline casEventMask caServerI::propertyEventMask() const
{
    return this->propertyEvent;
}

inline bool caServerI :: ioIsPending () const
{
    return ( ioInProgressCount > 0u );
}

inline void caServerI :: incrementIOInProgCount ()
{
    assert ( ioInProgressCount < UINT_MAX );
    ioInProgressCount++;
}

inline void caServerI :: decrementIOInProgCount ()
{
    assert ( ioInProgressCount > 0 );
    ioInProgressCount--;
    this->ioBlockedList::signal ();
}

#endif // caServerIh
