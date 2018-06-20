/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  
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

#include "tsDLList.h"

#include "comBuf.h"
#include "caServerID.h"
#include "netiiu.h"
#include "comQueSend.h"
#include "comQueRecv.h"
#include "tcpRecvWatchdog.h"
#include "tcpSendWatchdog.h"
#include "hostNameCache.h"
#include "SearchDest.h"
#include "compilerDependencies.h"

class callbackManager;

// a modified ca header with capacity for large arrays
struct caHdrLargeArray {
    ca_uint32_t m_postsize;     // size of message extension 
    ca_uint32_t m_count;        // operation data count 
    ca_uint32_t m_cid;          // channel identifier 
    ca_uint32_t m_available;    // protocol stub dependent
    ca_uint16_t m_dataType;     // operation data type 
    ca_uint16_t m_cmmd;         // operation to be performed 
};

class ipAddrToAsciiEngine;

class tcpRecvThread : private epicsThreadRunable {
public:
    tcpRecvThread ( 
        class tcpiiu & iiuIn, epicsMutex & cbMutexIn, cacContextNotify &,
        const char * pName, unsigned int stackSize, unsigned int priority );
    virtual ~tcpRecvThread ();
    void start ();
    void exitWait ();
    bool exitWait ( double delay );
    void interruptSocketRecv ();
    void show ( unsigned level ) const;
private:
    epicsThread thread;
    class tcpiiu & iiu;
    epicsMutex & cbMutex;
    cacContextNotify & ctxNotify;
    void run ();
    void connect (
        epicsGuard < epicsMutex > & guard );
    bool validFillStatus ( 
        epicsGuard < epicsMutex > & guard, 
        const statusWireIO & stat );
};

class tcpSendThread : private epicsThreadRunable {
public:
    tcpSendThread ( 
        class tcpiiu & iiuIn, const char * pName, 
        unsigned int stackSize, unsigned int priority );
    virtual ~tcpSendThread ();
    void start ();
    void exitWait ();
    void interruptSocketSend ();
    void show ( unsigned level ) const;
private:
    epicsThread thread;
    class tcpiiu & iiu;
    void run ();
};

class SearchDestTCP : public SearchDest {
public:
    SearchDestTCP ( cac &, const osiSockAddr & );
    void searchRequest ( epicsGuard < epicsMutex > & guard,
         const char * pbuf, size_t len );
    void show ( epicsGuard < epicsMutex > & guard, unsigned level ) const;
    void setCircuit ( tcpiiu * );
    void disable ();
    void enable ();
private:
    tcpiiu * _ptcpiiu;
    cac & _cac;
    const osiSockAddr _addr;
    bool _active;
};

class tcpiiu :
        public netiiu, public tsDLNode < tcpiiu >,
        public tsSLNode < tcpiiu >, public caServerID, 
        private wireSendAdapter, private wireRecvAdapter {
    friend void SearchDestTCP::searchRequest ( epicsGuard < epicsMutex > & guard,
                                               const char * pbuf, size_t len );
public:
    tcpiiu ( cac & cac, epicsMutex & mutualExclusion, epicsMutex & callbackControl, 
        cacContextNotify &, double connectionTimeout, epicsTimerQueue & timerQueue, 
        const osiSockAddr & addrIn, comBufMemoryManager &, unsigned minorVersion, 
        ipAddrToAsciiEngine & engineIn, const cacChannel::priLev & priorityIn,
        SearchDestTCP * pSearchDestIn = NULL);
    ~tcpiiu ();
    void start (
        epicsGuard < epicsMutex > & );
    void responsiveCircuitNotify ( 
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void sendTimeoutNotify ( 
        callbackManager & cbMgr,
        epicsGuard < epicsMutex > & guard );
    void receiveTimeoutNotify( 
        callbackManager &,
        epicsGuard < epicsMutex > & );
    void beaconAnomalyNotify ( 
        epicsGuard < epicsMutex > & );
    void beaconArrivalNotify ( 
        epicsGuard < epicsMutex > & );
    void probeResponseNotify ( 
        epicsGuard < epicsMutex > & );

    void flushRequest ( 
        epicsGuard < epicsMutex > & );
    unsigned requestMessageBytesPending ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void flush ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard );

    void show ( unsigned level ) const;
    bool setEchoRequestPending ( 
        epicsGuard < epicsMutex > & );
    void requestRecvProcessPostponedFlush (
        epicsGuard < epicsMutex > & );
    void clearChannelRequest ( 
        epicsGuard < epicsMutex > &, 
        ca_uint32_t sid, ca_uint32_t cid );

    bool ca_v41_ok (
        epicsGuard < epicsMutex > & ) const;
    bool ca_v42_ok (
        epicsGuard < epicsMutex > & ) const;
    bool ca_v44_ok (
        epicsGuard < epicsMutex > & ) const;
    bool ca_v49_ok (
        epicsGuard < epicsMutex > & ) const;

    unsigned getHostName ( 
        epicsGuard < epicsMutex > &,
        char *pBuf, unsigned bufLength ) const throw ();
    bool alive (
        epicsGuard < epicsMutex > & ) const;
    bool connecting (
        epicsGuard < epicsMutex > & ) const;
    bool receiveThreadIsBusy ( 
        epicsGuard < epicsMutex > & );
    osiSockAddr getNetworkAddress (
        epicsGuard < epicsMutex > & ) const;
    int printFormated ( 
        epicsGuard < epicsMutex > & cbGuard, 
        const char *pformat, ... );
    unsigned channelCount ( 
        epicsGuard < epicsMutex > & );
    void disconnectAllChannels (
        epicsGuard < epicsMutex > & cbGuard, 
        epicsGuard < epicsMutex > & guard, class udpiiu & );
    void unlinkAllChannels (
        epicsGuard < epicsMutex > & cbGuard, 
        epicsGuard < epicsMutex > & guard );
    void installChannel ( 
        epicsGuard < epicsMutex > &, nciu & chan, 
        unsigned sidIn, ca_uint16_t typeIn, arrayElementCount countIn );
    void uninstallChan ( 
        epicsGuard < epicsMutex > & guard, nciu & chan );
    bool connectNotify ( 
        epicsGuard < epicsMutex > &, nciu & chan );
    
    void searchRespNotify ( 
        const epicsTime &, const caHdrLargeArray & );
    void versionRespNotify ( const caHdrLargeArray & );

    void * operator new ( size_t size, 
        tsFreeList < class tcpiiu, 32, epicsMutexNOOP >  & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < class tcpiiu, 32, epicsMutexNOOP > & ))

private:
    hostNameCache hostNameCacheInstance;
    tcpRecvThread recvThread;
    tcpSendThread sendThread;
    tcpRecvWatchdog recvDog;
    tcpSendWatchdog sendDog;
    comQueSend sendQue;
    comQueRecv recvQue;
    // nciu state field tells us which list
    // protected by the callback mutex
    tsDLList < nciu > createReqPend;
    tsDLList < nciu > createRespPend;
    tsDLList < nciu > v42ConnCallbackPend;
    tsDLList < nciu > subscripReqPend;
    tsDLList < nciu > connectedList;
    tsDLList < nciu > unrespCircuit;
    tsDLList < nciu > subscripUpdateReqPend;
    caHdrLargeArray curMsg;
    arrayElementCount curDataMax;
    arrayElementCount curDataBytes;
    comBufMemoryManager & comBufMemMgr;
    cac & cacRef;
    char * pCurData;
    SearchDestTCP * pSearchDest;
    epicsMutex & mutex;
    epicsMutex & cbMutex;
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
    unsigned channelCountTot;
    bool _receiveThreadIsBusy;
    bool busyStateDetected; // only modified by the recv thread
    bool flowControlActive; // only modified by the send process thread
    bool echoRequestPending; 
    bool oldMsgHeaderAvailable;
    bool msgHeaderAvailable;
    bool earlyFlush;
    bool recvProcessPostponedFlush;
    bool discardingPendingData;
    bool socketHasBeenClosed;
    bool unresponsiveCircuit;

    bool processIncoming ( 
        const epicsTime & currentTime, callbackManager & );
    unsigned sendBytes ( const void *pBuf, 
        unsigned nBytesInBuf, const epicsTime & currentTime );
    void recvBytes ( 
        void * pBuf, unsigned nBytesInBuf, statusWireIO & );
    const char * pHostName (
        epicsGuard < epicsMutex > & ) const throw ();
    double receiveWatchdogDelay (
        epicsGuard < epicsMutex > & ) const;
    void unresponsiveCircuitNotify ( 
        epicsGuard < epicsMutex > & cbGuard, 
        epicsGuard < epicsMutex > & guard );
    void initiateCleanShutdown ( 
        epicsGuard < epicsMutex > & );
    void initiateAbortShutdown ( 
        epicsGuard < epicsMutex > & ); 
    void disconnectNotify (
        epicsGuard < epicsMutex > & );
    bool bytesArePendingInOS () const;
    void decrementBlockingForFlushCount ( 
        epicsGuard < epicsMutex > & guard );
    bool isNameService () const;

    // send protocol stubs
    void echoRequest ( 
        epicsGuard < epicsMutex > & );
    void versionMessage ( 
        epicsGuard < epicsMutex > &, const cacChannel::priLev & priority );
    void disableFlowControlRequest (
        epicsGuard < epicsMutex > & );
    void enableFlowControlRequest (
        epicsGuard < epicsMutex > & );
    void hostNameSetRequest ( 
        epicsGuard < epicsMutex > & );
    void userNameSetRequest ( 
        epicsGuard < epicsMutex > & );
    void createChannelRequest ( 
        nciu &, epicsGuard < epicsMutex > & );
    void writeRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        unsigned type, arrayElementCount nElem, const void *pValue );
    void writeNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        netWriteNotifyIO &, unsigned type, 
        arrayElementCount nElem, const void *pValue );
    void readNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        netReadNotifyIO &, unsigned type, 
        arrayElementCount nElem );
    void subscriptionRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netSubscription & subscr );
    void subscriptionUpdateRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu & chan, netSubscription & subscr );
    void subscriptionCancelRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu & chan, netSubscription & subscr );
    void flushIfRecvProcessRequested (
        epicsGuard < epicsMutex > & );
    bool sendThreadFlush ( 
        epicsGuard < epicsMutex > & );

    // netiiu stubs
    void uninstallChanDueToSuccessfulSearchResponse ( 
        epicsGuard < epicsMutex > &, nciu &, const class epicsTime & );
    bool searchMsg (
        epicsGuard < epicsMutex > &, ca_uint32_t id, 
            const char * pName, unsigned nameLength );

    friend class tcpRecvThread;
    friend class tcpSendThread;

	tcpiiu ( const tcpiiu & );
	tcpiiu & operator = ( const tcpiiu & );
    void operator delete ( void * );
};

inline void * tcpiiu::operator new ( size_t size, 
    tsFreeList < class tcpiiu, 32, epicsMutexNOOP > & mgr )
{
    return mgr.allocate ( size );
}
    
#ifdef CXX_PLACEMENT_DELETE
inline void tcpiiu::operator delete ( void * pCadaver, 
    tsFreeList < class tcpiiu, 32, epicsMutexNOOP > & mgr )
{
    mgr.release ( pCadaver );
}
#endif

inline bool tcpiiu::ca_v41_ok (
    epicsGuard < epicsMutex > & ) const
{
    return CA_V41 ( this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v44_ok (
    epicsGuard < epicsMutex > & ) const
{
    return CA_V44 ( this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v49_ok (
    epicsGuard < epicsMutex > & ) const
{
    return CA_V49 ( this->minorProtocolVersion );
}

inline bool tcpiiu::alive (
    epicsGuard < epicsMutex > & ) const
{
    return ( this->state == iiucs_connecting || 
        this->state == iiucs_connected );
}

inline bool tcpiiu::connecting (
    epicsGuard < epicsMutex > & ) const
{
    return ( this->state == iiucs_connecting );
}

inline bool tcpiiu::receiveThreadIsBusy ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->_receiveThreadIsBusy;
}

inline void tcpiiu::beaconAnomalyNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    //guard.assertIdenticalMutex ( this->cacRef.mutexRef () );
    this->recvDog.beaconAnomalyNotify ( guard );
}

inline void tcpiiu::beaconArrivalNotify (
    epicsGuard < epicsMutex > & guard )
{
    //guard.assertIdenticalMutex ( this->cacRef.mutexRef () );
    this->recvDog.beaconArrivalNotify ( guard );
}

inline void tcpiiu::probeResponseNotify (
    epicsGuard < epicsMutex > & cbGuard )
{
    this->recvDog.probeResponseNotify ( cbGuard );
}

inline bool tcpiiu::isNameService () const
{
    return ( this->pSearchDest != NULL );
}

inline void SearchDestTCP::setCircuit ( tcpiiu * piiu )
{
    _ptcpiiu = piiu;
}

#endif // ifdef virtualCircuith
