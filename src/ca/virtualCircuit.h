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
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef virtualCircuith  
#define virtualCircuith

#include "epicsMemory.h"
#include "tsDLList.h"
#include "tsMinMax.h"

#include "comBuf.h"
#include "caServerID.h"
#include "netiiu.h"
#include "comQueSend.h"
#include "comQueRecv.h"
#include "tcpRecvWatchdog.h"
#include "tcpSendWatchdog.h"
#include "hostNameCache.h"

// a modified ca header with capacity for large arrays
struct  caHdrLargeArray {
    ca_uint32_t m_postsize;     // size of message extension 
    ca_uint32_t m_count;        // operation data count 
    ca_uint32_t m_cid;          // channel identifier 
    ca_uint32_t m_available;    // protocol stub dependent
    ca_uint16_t m_dataType;     // operation data type 
    ca_uint16_t m_cmmd;         // operation to be performed 
};

class hostNameCache;
class ipAddrToAsciiEngine;
class callbackMutex;

class tcpRecvThread : public epicsThreadRunable {
public:
    tcpRecvThread ( class tcpiiu & iiuIn, callbackMutex & cbMutexIn,
        const char * pName, unsigned int stackSize, unsigned int priority );
    virtual ~tcpRecvThread ();
    void start ();
    void exitWait ();
    void interruptSocketRecv ();
private:
    epicsThread thread;
    class tcpiiu & iiu;
    callbackMutex & cbMutex;
    void run ();
};

class tcpSendThread : public epicsThreadRunable {
public:
    tcpSendThread ( class tcpiiu & iiuIn, callbackMutex &,
        const char * pName, unsigned int stackSize, 
        unsigned int priority );
    virtual ~tcpSendThread ();
    void start ();
    void exitWait ();
    void exitWaitRelease ();
   void interruptSocketSend ();
private:
    epicsThread thread;
    class tcpiiu & iiu;
    callbackMutex & cbMutex;
    void run ();
};

class tcpiiu : 
        public netiiu, public tsDLNode < tcpiiu >,
        public tsSLNode < tcpiiu >, public caServerID, 
        private wireSendAdapter, private wireRecvAdapter {
public:
    tcpiiu ( cac & cac, callbackMutex & cbMutex, double connectionTimeout, 
        epicsTimerQueue & timerQueue, const osiSockAddr & addrIn, 
        comBufMemoryManager &, unsigned minorVersion, ipAddrToAsciiEngine & engineIn,
        const cacChannel::priLev & priorityIn );
    ~tcpiiu ();
    void start ();
    void initiateCleanShutdown ( epicsGuard < cacMutex > & );
    void initiateAbortShutdown ( epicsGuard < callbackMutex > &, 
                                    epicsGuard <cacMutex > & ); 
    void disconnectNotify ( epicsGuard <cacMutex > & );
    void beaconAnomalyNotify ();
    void beaconArrivalNotify ( 
        const epicsTime & currentTime );

    void flushRequest ();
    bool flushBlockThreshold ( epicsGuard < cacMutex > & ) const;
    void flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & );
    void blockUntilSendBacklogIsReasonable 
        ( cacNotify &, epicsGuard < cacMutex > & );
    virtual void show ( unsigned level ) const;
    bool setEchoRequestPending ();
    void createChannelRequest ( nciu & );
    void requestRecvProcessPostponedFlush ();
    void clearChannelRequest ( epicsGuard < cacMutex > &, 
        ca_uint32_t sid, ca_uint32_t cid );

    bool ca_v41_ok () const;
    bool ca_v42_ok () const;
    bool ca_v44_ok () const;
    bool ca_v49_ok () const;

    void hostName ( char *pBuf, unsigned bufLength ) const;
    bool alive () const;
    osiSockAddr getNetworkAddress () const;
    int printf ( const char *pformat, ... );
    unsigned channelCount ();
    void removeAllChannels (
        epicsGuard < callbackMutex > & cbGuard, 
        epicsGuard < cacMutex > & guard,
        class cacDisconnectChannelPrivate & );
    void installChannel ( epicsGuard < cacMutex > &, nciu & chan, 
        unsigned sidIn, ca_uint16_t typeIn, arrayElementCount countIn );
    void uninstallChan ( epicsGuard < cacMutex > &, nciu & chan );

    bool bytesArePendingInOS () const;

private:
    hostNameCache hostNameCacheInstance;
    tcpRecvThread recvThread;
    tcpSendThread sendThread;
    tcpRecvWatchdog recvDog;
    tcpSendWatchdog sendDog;
    comQueSend sendQue;
    comQueRecv recvQue;
    tsDLList < nciu > channelList;
    caHdrLargeArray curMsg;
    arrayElementCount curDataMax;
    arrayElementCount curDataBytes;
    comBufMemoryManager & comBufMemMgr;
    cac & cacRef;
    char * pCurData;
    unsigned minorProtocolVersion;
    enum iiu_conn_state { 
        iiucs_connecting, // pending circuit connect
        iiucs_connected, // live circuit
        iiucs_clean_shutdown, // live circuit will shutdown when flush completes
        iiucs_disconnected, // socket informed us of disconnect
        iiucs_abort_shutdown // socket has been closed
                } state;
    epicsEvent sendThreadFlushEvent;
    epicsEvent flushBlockEvent;
    SOCKET sock;
    unsigned contigRecvMsgCount;
    unsigned blockingForFlush;
    unsigned socketLibrarySendBufferSize;
    unsigned unacknowledgedSendBytes;
    bool busyStateDetected; // only modified by the recv thread
    bool flowControlActive; // only modified by the send process thread
    bool echoRequestPending; 
    bool oldMsgHeaderAvailable;
    bool msgHeaderAvailable;
    bool earlyFlush;
    bool recvProcessPostponedFlush;
    bool discardingPendingData;

    bool processIncoming ( 
        const epicsTime & currentTime, epicsGuard < callbackMutex > & );
    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );
    void connect ();
    const char * pHostName () const;
    void blockUntilBytesArePendingInOS ();
    void shutdown ( epicsGuard <cacMutex > & ); 

    // send protocol stubs
    void echoRequest ( epicsGuard < cacMutex > & );
    void versionMessage ( epicsGuard < cacMutex > &, const cacChannel::priLev & priority );
    void disableFlowControlRequest (epicsGuard < cacMutex > & );
    void enableFlowControlRequest (epicsGuard < cacMutex > & );
    void hostNameSetRequest ( epicsGuard < cacMutex > & );
    void userNameSetRequest ( epicsGuard < cacMutex > & );
    void writeRequest ( epicsGuard < cacMutex > &, nciu &, unsigned type, unsigned nElem, const void *pValue );
    void writeNotifyRequest ( epicsGuard < cacMutex > &, nciu &, netWriteNotifyIO &, unsigned type, unsigned nElem, const void *pValue );
    void readNotifyRequest ( epicsGuard < cacMutex > &, nciu &, netReadNotifyIO &, unsigned type, unsigned nElem );
    void subscriptionRequest ( epicsGuard < cacMutex > &, nciu &, netSubscription & subscr );
    void subscriptionCancelRequest ( epicsGuard < cacMutex > &, nciu & chan, netSubscription & subscr );
    void flushIfRecvProcessRequested ();
    bool flush (); // only to be called by the send thread

    friend void tcpRecvThread::run ();
    friend void tcpSendThread::run ();

	tcpiiu ( const tcpiiu & );
	tcpiiu & operator = ( const tcpiiu & );
};

inline bool tcpiiu::ca_v41_ok () const
{
    return CA_V41 ( this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v44_ok () const
{
    return CA_V44 ( this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v49_ok () const
{
    return CA_V49 ( this->minorProtocolVersion );
}

inline bool tcpiiu::alive () const // X aCC 361
{
    return ( this->state == iiucs_connecting || 
        this->state == iiucs_connected );
}

inline void tcpiiu::beaconAnomalyNotify ()
{
    this->recvDog.beaconAnomalyNotify ();
}

inline void tcpiiu::beaconArrivalNotify (
    const epicsTime & currentTime )
{
    this->recvDog.beaconArrivalNotify ( currentTime );
}

inline void tcpiiu::flushIfRecvProcessRequested ()
{
    if ( this->recvProcessPostponedFlush ) {
        this->flushRequest ();
        this->recvProcessPostponedFlush = false;
    }
}

inline unsigned tcpiiu::channelCount ()
{
    return this->channelList.count ();
}

inline void tcpRecvThread::interruptSocketRecv ()
{
    epicsThreadId threadId = this->thread.getId ();
    if ( threadId ) {
        epicsSocketInterruptSystemCall ( threadId );
    }
}

inline void tcpSendThread::interruptSocketSend ()
{
    epicsThreadId threadId = this->thread.getId ();
    if ( threadId ) {
        epicsSocketInterruptSystemCall ( threadId );
    }
}

#endif // ifdef virtualCircuith

