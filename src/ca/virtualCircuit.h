
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

#include <new> // needed by comQueueSend

#include "epicsTimer.h"
#include "ipAddrToAsciiAsynchronous.h"

#include "comBuf.h"
#include "netiiu.h"

enum iiu_conn_state {iiu_connecting, iiu_connected, iiu_disconnected};

class nciu;
class tcpiiu;

class bufferReservoir {
public:
    ~bufferReservoir ();
    void addOneBuffer ();
    comBuf *fetchOneBuffer ();
    unsigned nBytes ();
    void drain ();
private:
    tsDLList < comBuf > reservedBufs;
};

class comQueSend {
public:
    comQueSend ( wireSendAdapter & );
    ~comQueSend ();
    void clear ();
    void reserveSpace ( unsigned msgSize );
    unsigned occupiedBytes () const;
    bool flushEarlyThreshold ( unsigned nBytesThisMsg ) const;
    bool flushBlockThreshold ( unsigned nBytesThisMsg ) const;
    bool dbr_type_ok ( unsigned type );
    void pushUInt16 ( const ca_uint16_t value );
    void pushUInt32 ( const ca_uint32_t value );
    void pushFloat32 ( const ca_float32_t value );
    void pushString ( const char *pVal, unsigned nElem );
    void push_dbr_type ( unsigned type, const void *pVal, unsigned nElem );
    comBuf * popNextComBufToSend ();
private:
    wireSendAdapter & wire;
    tsDLList < comBuf > bufs;
    bufferReservoir reservoir;
    unsigned nBytesPending;
    void copy_dbr_string ( const void *pValue, unsigned nElem );
    void copy_dbr_short ( const void *pValue, unsigned nElem );
    void copy_dbr_float ( const void *pValue, unsigned nElem );
    void copy_dbr_char ( const void *pValue, unsigned nElem );
    void copy_dbr_long ( const void *pValue, unsigned nElem );
    void copy_dbr_double ( const void *pValue, unsigned nElem );
    typedef void ( comQueSend::*copyFunc_t ) (  
        const void *pValue, unsigned nElem );
    static const copyFunc_t dbrCopyVector [39];
};

static const unsigned maxBytesPendingTCP = 0x4000;

class comQueRecv {
public:
    comQueRecv ();
    ~comQueRecv ();
    unsigned occupiedBytes () const;
    bool copyOutBytes ( void *pBuf, unsigned nBytes );
    void pushLastComBufReceived ( comBuf & );
    void clear ();
private:
    tsDLList < comBuf > bufs;
};

class tcpRecvWatchdog : private epicsTimerNotify {
public:
    tcpRecvWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpRecvWatchdog ();
    void rescheduleRecvTimer ();
    void messageArrivalNotify ();
    void beaconArrivalNotify ();
    void beaconAnomalyNotify ();
    void connectNotify ();
    void cancel ();
    void show ( unsigned level ) const;
private:
    const double period;
    epicsTimer &timer;
    tcpiiu &iiu;
    bool responsePending;
    bool beaconAnomaly;
    expireStatus expire ();
};

class tcpSendWatchdog : private epicsTimerNotify {
public:
    tcpSendWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpSendWatchdog ();
    void start ();
    void cancel ();
private:
    const double period;
    epicsTimer &timer;
    tcpiiu &iiu;
    expireStatus expire ();
};

class hostNameCache : public ipAddrToAsciiAsynchronous {
public:
    hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine );
    void destroy ();
    void ioCompletionNotify ( const char *pHostName );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~hostNameCache ();
private:
    bool ioComplete;
    char hostNameBuf [128];
    static tsFreeList < class hostNameCache, 16 > freeList;
    static epicsMutex freeListMutex;
};

extern "C" void cacSendThreadTCP ( void *pParam );
extern "C" void cacRecvThreadTCP ( void *pParam );

class tcpiiu : 
        public netiiu, public tsDLNode < tcpiiu >,
        private wireSendAdapter, private wireRecvAdapter {
public:
    tcpiiu ( cac &cac, double connectionTimeout, epicsTimerQueue &timerQueue );
    ~tcpiiu ();
    bool initiateConnect ( const osiSockAddr &addrIn, unsigned minorVersion, 
        class bhe &bhe, ipAddrToAsciiEngine &engineIn );
    void connect ();
    void processIncoming ();
    void destroy ();
    void cleanShutdown ();
    void forcedShutdown ();
    void beaconAnomalyNotify ();
    void beaconArrivalNotify ();

    bool fullyConstructed () const;
    void flushRequest ();
    bool flushBlockThreshold () const;
    void flushRequestIfAboveEarlyThreshold ();
    void blockUntilSendBacklogIsReasonable ( epicsMutex & );
    virtual void show ( unsigned level ) const;
    bool setEchoRequestPending ();

    bool ca_v41_ok () const;
    bool ca_v42_ok () const;
    bool ca_v44_ok () const;

    void hostName ( char *pBuf, unsigned bufLength ) const;
    const char * pHostName () const; // deprecated - please do not use
    bool isVirtaulCircuit ( const char *pChannelName, const osiSockAddr &addr ) const;
    bool alive () const;
    double beaconPeriod () const;
    bhe * getBHE () const;

    SOCKET getSock() const;
    bool trueOnceOnly ();

private:
    tcpRecvWatchdog recvDog;
    tcpSendWatchdog sendDog;
    comQueSend sendQue;
    comQueRecv recvQue;
    osiSockAddr addr;
    hostNameCache *pHostNameCache;
    caHdr curMsg;
    unsigned long curDataMax;
    class bhe *pBHE;
    char *pCurData;
    unsigned minorProtocolVersion;
    iiu_conn_state state;
    epicsEventId sendThreadFlushSignal;
    epicsEventId recvThreadRingBufferSpaceAvailableSignal;
    epicsEventId sendThreadExitSignal;
    epicsEventId recvThreadExitSignal;
    epicsEventId flushBlockSignal;
    SOCKET sock;
    unsigned contigRecvMsgCount;
    unsigned blockingForFlush;
    bool fullyConstructedFlag;
    bool busyStateDetected; // only modified by the recv thread
    bool flowControlActive; // only modified by the send process thread
    bool echoRequestPending; 
    bool msgHeaderAvailable;
    bool sockCloseCompleted;
    bool f_trueOnceOnly;
    bool earlyFlush;

    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );

    friend void cacSendThreadTCP ( void *pParam );
    friend void cacRecvThreadTCP ( void *pParam );

    void lastChannelDetachNotify ();

    // send protocol stubs
    void echoRequest ();
    void noopRequest ();
    void disableFlowControlRequest ();
    void enableFlowControlRequest ();
    void hostNameSetRequest ();
    void userNameSetRequest ();
    void writeRequest ( nciu &, unsigned type, unsigned nElem, const void *pValue );
    void writeNotifyRequest ( nciu &, netWriteNotifyIO &, unsigned type, unsigned nElem, const void *pValue );
    void readNotifyRequest ( nciu &, netReadNotifyIO &, unsigned type, unsigned nElem );
    void createChannelRequest ( nciu & );
    void clearChannelRequest ( nciu & );
    void subscriptionRequest ( nciu &, netSubscription &subscr );
    void subscriptionCancelRequest ( nciu &, netSubscription &subscr );

    bool flush (); // only to be called by the send thread
};

inline bufferReservoir::~bufferReservoir ()
{
    this->drain ();
}

inline comBuf *bufferReservoir::fetchOneBuffer ()
{
    return this->reservedBufs.get ();
}

inline void bufferReservoir::addOneBuffer ()
{
    comBuf *pBuf = new comBuf;
    if ( ! pBuf ) {
        throw std::bad_alloc();
    }
    this->reservedBufs.add ( *pBuf );
}

inline unsigned bufferReservoir::nBytes ()
{
    return ( this->reservedBufs.count () * comBuf::capacityBytes () );
}

inline void bufferReservoir::drain ()
{
    comBuf *pBuf;
    while ( ( pBuf = this->reservedBufs.get () ) ) {
        pBuf->destroy ();
    }
}

inline bool comQueSend::dbr_type_ok ( unsigned type )
{
    if ( type >= ( sizeof ( this->dbrCopyVector ) / sizeof ( this->dbrCopyVector[0] )  ) ) {
        return false;
    }
    if ( ! this->dbrCopyVector [type] ) {
        return false;
    }
    return true;
}

//
// 1) This routine does not return status because of the following
// argument.  The routine can fail because the wire disconnects or 
// because their isnt memory to create a buffer. For the former we 
// just discard the message, but do not fail. For the latter we 
// shutdown() the connection and discard the rest of the message
// (this eliminates the possibility of message fragments getting
// onto the wire).
//
// 2) Arguments here are a bit verbose until compilers all implement
// member template functions.
//

template < class T >
inline void comQueSend_copyIn ( unsigned &nBytesPending, 
         tsDLList < comBuf > &comBufList, bufferReservoir &reservoir,
                               const T *pVal, unsigned nElem )
{
    nBytesPending += sizeof ( T ) * nElem;

    comBuf *pComBuf = comBufList.last ();
    if ( pComBuf ) {
        unsigned nCopied = pComBuf->copyIn ( pVal, nElem );
        if ( nElem > nCopied ) {
            comQueSend_copyInWithReservour ( comBufList, reservoir, &pVal[nCopied], 
                nElem - nCopied );
        }
    }
    else {
        comQueSend_copyInWithReservour ( comBufList, reservoir, pVal, nElem );
    }
}

template < class T >
void comQueSend_copyInWithReservour ( 
         tsDLList < comBuf > &comBufList, bufferReservoir &reservoir,
                               const T *pVal, unsigned nElem )
{
    unsigned nCopied = 0u;
    while ( nElem > nCopied ) {
        comBuf *pComBuf = reservoir.fetchOneBuffer ();
        //
        // This fails only if space was not preallocated.
        // See comments at the top of this program on
        // why space must always be preallocated.
        //
        assert ( pComBuf );
        nCopied += pComBuf->copyIn ( &pVal[nCopied], nElem - nCopied );
        comBufList.add ( *pComBuf );
    }
}

template < class T >
inline void comQueSend_copyIn ( unsigned &nBytesPending, 
       tsDLList < comBuf > &comBufList, bufferReservoir &reservoir,
                               const T &val )
{
    nBytesPending += sizeof ( T );

    comBuf *pComBuf = comBufList.last ();
    if ( pComBuf ) {
        if ( pComBuf->copyIn ( &val, 1u ) >= 1u ) {
            return;
        }
    }

    pComBuf = reservoir.fetchOneBuffer ();
    //
    // This fails only if space was not preallocated.
    // See comments at the top of this program on
    // space must always be preallocated.
    //
    assert ( pComBuf );
    pComBuf->copyIn ( &val, 1u );
    comBufList.add ( *pComBuf );
}

inline void comQueSend::pushUInt16 ( const ca_uint16_t value )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, value );
}

inline void comQueSend::pushUInt32 ( const ca_uint32_t value )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, value );
}

inline void comQueSend::pushFloat32 ( const ca_float32_t value )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, value );
}

inline void comQueSend::pushString ( const char *pVal, unsigned nElem )
{
    comQueSend_copyIn ( this->nBytesPending, 
        this->bufs, this->reservoir, pVal, nElem );
}

// it is assumed that dbr_type_ok() was called prior to calling this routine
// to check the type code
inline void comQueSend::push_dbr_type ( unsigned type, const void *pVal, unsigned nElem )
{
    ( this->*dbrCopyVector [type] ) ( pVal, nElem );
}

inline unsigned comQueSend::occupiedBytes () const
{
    return this->nBytesPending;
}

inline bool comQueSend::flushBlockThreshold ( unsigned nBytesThisMsg ) const
{
    return ( this->nBytesPending + nBytesThisMsg > 16 * comBuf::capacityBytes () );
}

inline bool comQueSend::flushEarlyThreshold ( unsigned nBytesThisMsg ) const
{
    return ( this->nBytesPending + nBytesThisMsg > 4 * comBuf::capacityBytes () );
}

inline comBuf * comQueSend::popNextComBufToSend ()
{
    comBuf *pBuf = this->bufs.get ();
    if ( pBuf ) {
        unsigned nBytesThisBuf = pBuf->occupiedBytes ();
        assert ( this->nBytesPending >= nBytesThisBuf );
        this->nBytesPending -= pBuf->occupiedBytes ();
    }
    else {
        assert ( this->nBytesPending == 0u );
    }
    return pBuf;
}

inline bool tcpiiu::fullyConstructed () const
{
    return this->fullyConstructedFlag;
}

inline void tcpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    if ( this->pHostNameCache ) {
        this->pHostNameCache->hostName ( pBuf, bufLength );
    }
    else {
        netiiu::hostName ( pBuf, bufLength );
    }
}

// deprecated - please dont use - this is _not_ thread safe
inline const char * tcpiiu::pHostName () const
{
    static char nameBuf [128];
    this->hostName ( nameBuf, sizeof ( nameBuf ) );
    return nameBuf; // ouch !!
}

inline void tcpiiu::flushRequest ()
{
    epicsEventSignal ( this->sendThreadFlushSignal );
}

inline bool tcpiiu::ca_v44_ok () const
{
    return CA_V44 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v41_ok () const
{
    return CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion );
}

inline bool tcpiiu::alive () const
{
    if ( this->state == iiu_connecting || 
        this->state == iiu_connected ) {
        return true;
    }
    else {
        return false;
    }
}

inline bhe * tcpiiu::getBHE () const
{
    return this->pBHE;
}

inline void tcpiiu::beaconAnomalyNotify ()
{
    this->recvDog.beaconAnomalyNotify ();
}

inline void tcpiiu::beaconArrivalNotify ()
{
    this->recvDog.beaconArrivalNotify ();
}

inline bool tcpiiu::trueOnceOnly ()
{
    if ( this->f_trueOnceOnly ) {
        this->f_trueOnceOnly = false;
        return true;
    }
    else {
        return false;
    }
}

inline SOCKET tcpiiu::getSock () const
{
    return this->sock;
}

#endif // ifdef virtualCircuith