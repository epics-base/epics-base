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
#include "tsSLList.h"
#include "tsDLList.h"

#include "comBuf.h"
#include "caServerID.h"
#include "netiiu.h"
#include "comQueSend.h"
#include "comQueRecv.h"
#include "tcpRecvWatchdog.h"
#include "tcpSendWatchdog.h"
#include "tcpKillTimer.h"

enum iiu_conn_state { iiu_connecting, iiu_connected, iiu_disconnected };

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
private:
    epicsThread thread;
    class tcpiiu & iiu;
    callbackMutex & cbMutex;
    void run ();
};

class tcpSendThread : public epicsThreadRunable {
public:
    tcpSendThread ( class tcpiiu & iiuIn,
        const char * pName, unsigned int stackSize, unsigned int priority );
    virtual ~tcpSendThread ();
    void start ();
    bool exitWait ( double delay );
private:
    class tcpiiu & iiu;
    epicsThread thread;
    void run ();
};

class tcpiiu : 
        public netiiu, public tsDLNode < tcpiiu >,
        public tsSLNode < tcpiiu >, public caServerID, 
        private wireSendAdapter, private wireRecvAdapter {
public:
    tcpiiu ( cac &cac, callbackMutex & cbMutex, double connectionTimeout, 
        epicsTimerQueue & timerQueue, const osiSockAddr &addrIn, 
        unsigned minorVersion, ipAddrToAsciiEngine & engineIn,
        const cacChannel::priLev & priorityIn );
    ~tcpiiu ();
    void start ( epicsGuard < callbackMutex > & );
    void cleanShutdown ();
    void forcedShutdown ();
    void shutdown ( epicsGuard < callbackMutex > & cbLocker, 
        epicsGuard < cacMutex > & guard, bool discardPendingMessages );
    void beaconAnomalyNotify ();
    void beaconArrivalNotify ();

    void flushRequest ();
    bool flushBlockThreshold ( epicsGuard < cacMutex > & ) const;
    void flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & );
    void blockUntilSendBacklogIsReasonable 
        ( epicsGuard < callbackMutex > * pCallbackGuard, 
        epicsGuard < cacMutex > & primaryGuard );
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
    void removeAllChannels ( epicsGuard < callbackMutex > & cbGuard, 
                                epicsGuard < cacMutex > & guard,
                                class cacDisconnectChannelPrivate & );
    void installChannel ( epicsGuard < cacMutex > &, nciu & chan, 
        unsigned sidIn, ca_uint16_t typeIn, arrayElementCount countIn );
    void uninstallChannel ( epicsGuard < callbackMutex > &,
        epicsGuard < cacMutex > &, nciu & chan );

private:
    tcpRecvThread recvThread;
    tcpSendThread sendThread;
    tcpRecvWatchdog recvDog;
    tcpSendWatchdog sendDog;
    tcpKillTimer killTimer;
    comQueSend sendQue;
    comQueRecv recvQue;
    tsDLList < nciu > channelList;
    caHdrLargeArray curMsg;
    arrayElementCount curDataMax;
    arrayElementCount curDataBytes;
    epics_auto_ptr < hostNameCache > pHostNameCache;
    cac & cacRef;
    char * pCurData;
    unsigned minorProtocolVersion;
    iiu_conn_state state;
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
    bool sockCloseCompleted;
    bool earlyFlush;
    bool recvProcessPostponedFlush;

    void stopThreads ();
    bool processIncoming ( epicsGuard < callbackMutex > & );
    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );
    void connect ();
    const char * pHostName () const;

    // send protocol stubs
    void echoRequest ( epicsGuard < cacMutex > & );
    void versionMessage ( epicsGuard < cacMutex > &, const cacChannel::priLev & priority );
    void disableFlowControlRequest (epicsGuard < cacMutex > & );
    void enableFlowControlRequest (epicsGuard < cacMutex > & );
    void hostNameSetRequest (epicsGuard < cacMutex > & );
    void userNameSetRequest (epicsGuard < cacMutex > & );
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
    if ( this->state == iiu_connecting || 
        this->state == iiu_connected ) {
        return true;
    }
    else {
        return false;
    }
}

inline void tcpiiu::beaconAnomalyNotify ()
{
    this->recvDog.beaconAnomalyNotify ();
}

inline void tcpiiu::beaconArrivalNotify ()
{
    this->recvDog.beaconArrivalNotify ();
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

#endif // ifdef virtualCircuith

