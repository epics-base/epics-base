
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
#include "epicsMemory.h"
#include "ipAddrToAsciiAsynchronous.h"
#include "caServerID.h"
#include "comBuf.h"
#include "netiiu.h"

enum iiu_conn_state { iiu_connecting, iiu_connected, iiu_disconnected };

class nciu;
class tcpiiu;

class comQueSend {
public:
    comQueSend ( wireSendAdapter & );
    ~comQueSend ();
    void clear ();
    unsigned occupiedBytes () const;
    bool flushEarlyThreshold ( unsigned nBytesThisMsg ) const;
    bool flushBlockThreshold ( unsigned nBytesThisMsg ) const;
    bool dbr_type_ok ( unsigned type );
    void pushUInt16 ( const ca_uint16_t value );
    void pushUInt32 ( const ca_uint32_t value );
    void pushFloat32 ( const ca_float32_t value );
    void pushString ( const char *pVal, unsigned nChar );
    void push_dbr_type ( unsigned type, const void *pVal, unsigned nElem );
    comBuf * popNextComBufToSend ();
private:
    wireSendAdapter & wire;
    tsDLList < comBuf > bufs;
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

    //
    // visual C++ version 6.0 does not allow out of 
    // class member template function definition
    //
    template < class T >
    inline void copyIn ( const T *pVal, unsigned nElem )
    {
        unsigned nCopied;
        comBuf *pComBuf = this->bufs.last ();
        if ( pComBuf ) {
            nCopied = pComBuf->copyIn ( pVal, nElem );
            this->nBytesPending += nCopied * sizeof ( T );
        }
        else {
            nCopied = 0u;
        }
        while ( nElem > nCopied ) {
            pComBuf = new ( std::nothrow ) comBuf;
            if ( ! pComBuf ) {
                this->wire.forcedShutdown ();
                throw std::bad_alloc ();
            }
            unsigned nNew = pComBuf->copyIn ( &pVal[nCopied], nElem - nCopied );
            nCopied +=  nNew;
            this->nBytesPending += nNew * sizeof ( T );
            this->bufs.add ( *pComBuf );
        }
    }

    //
    // visual C++ version 6.0 does not allow out of 
    // class member template function definition
    //
    template < class T >
    inline void copyIn ( const T &val )
    {
        comBuf *pComBuf = this->bufs.last ();
        if ( pComBuf ) {
            if ( pComBuf->copyIn ( &val, 1u ) >= 1u ) {
                this->nBytesPending += sizeof ( T );
                return;
            }
        }
        pComBuf = new ( std::nothrow ) comBuf;
        if ( ! pComBuf ) {
            this->wire.forcedShutdown ();
            throw std::bad_alloc ();
        }
        if ( pComBuf->copyIn ( &val, 1u ) == 0u ) {
            this->wire.forcedShutdown ();
            throw -1;
        }
        this->bufs.add ( *pComBuf );
        this->nBytesPending += sizeof ( T );
        return;
    }
};

static const unsigned maxBytesPendingTCP = 0x4000;

class comQueRecv {
public:
    comQueRecv ();
    ~comQueRecv ();
    unsigned occupiedBytes () const;
    unsigned copyOutBytes ( epicsInt8 *pBuf, unsigned nBytes );
    unsigned removeBytes ( unsigned nBytes );
    void pushLastComBufReceived ( comBuf & );
    void clear ();
    epicsInt8 popInt8 ();
    epicsUInt8 popUInt8 ();
    epicsInt16 popInt16 ();
    epicsUInt16 popUInt16 ();
    epicsInt32 popInt32 ();
    epicsUInt32 popUInt32 ();
    epicsFloat32 popFloat32 ();
    epicsFloat64 popFloat64 ();
    void popString ( epicsOldString * );
    class insufficentBytesAvailable {};
private:
    tsDLList < comBuf > bufs;
    unsigned nBytesPending;
};

class tcpRecvWatchdog : private epicsTimerNotify {
public:
    tcpRecvWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & );
    virtual ~tcpRecvWatchdog ();
    void rescheduleRecvTimer ();
    void sendBacklogProgressNotify ();
    void messageArrivalNotify ();
    void beaconArrivalNotify ();
    void beaconAnomalyNotify ();
    void connectNotify ();
    void cancel ();
    void show ( unsigned level ) const;
private:
    const double period;
    epicsTimer & timer;
    tcpiiu &iiu;
    bool responsePending;
    bool beaconAnomaly;
    expireStatus expire ( const epicsTime & currentTime );
};

class tcpSendWatchdog : private epicsTimerNotify {
public:
    tcpSendWatchdog ( tcpiiu &, double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpSendWatchdog ();
    void start ();
    void cancel ();
private:
    const double period;
    epicsTimer & timer;
    tcpiiu & iiu;
    expireStatus expire ( const epicsTime & currentTime );
};

class hostNameCache : public ipAddrToAsciiAsynchronous {
public:
    hostNameCache ( const osiSockAddr &addr, ipAddrToAsciiEngine &engine );
    ~hostNameCache ();
    void destroy ();
    void ioCompletionNotify ( const char *pHostName );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    bool ioComplete;
    char hostNameBuf [128];
    static tsFreeList < class hostNameCache, 16 > freeList;
    static epicsMutex freeListMutex;
};

extern "C" void cacSendThreadTCP ( void *pParam );
extern "C" void cacRecvThreadTCP ( void *pParam );

// a modified ca header with capacity for large arrays
struct  caHdrLargeArray {
    ca_uint32_t m_postsize;     // size of message extension 
    ca_uint32_t m_count;        // operation data count 
    ca_uint32_t m_cid;          // channel identifier 
    ca_uint32_t m_available;    // protocol stub dependent
    ca_uint16_t m_dataType;     // operation data type 
    ca_uint16_t m_cmmd;         // operation to be performed 
};

class tcpiiu : 
        public netiiu, public tsDLNode < tcpiiu >,
        public tsSLNode < tcpiiu >, public caServerID, 
        private wireSendAdapter, private wireRecvAdapter {
public:
    tcpiiu ( cac &cac, double connectionTimeout, 
        epicsTimerQueue &timerQueue, const osiSockAddr &addrIn, 
        unsigned minorVersion, ipAddrToAsciiEngine & engineIn,
        const cacChannel::priLev & priorityIn );
    ~tcpiiu ();
    void connect ();
    void destroy ();
    void forcedShutdown ();
    void cleanShutdown ();
    void beaconAnomalyNotify ();
    void beaconArrivalNotify ();

    void flushRequest ();
    bool flushBlockThreshold () const;
    void flushRequestIfAboveEarlyThreshold ();
    void blockUntilSendBacklogIsReasonable 
        ( epicsMutex * pCallBack, epicsMutex & primary );
    virtual void show ( unsigned level ) const;
    bool setEchoRequestPending ();
    void requestRecvProcessPostponedFlush ();

    bool ca_v41_ok () const;
    bool ca_v42_ok () const;
    bool ca_v44_ok () const;
    bool ca_v49_ok () const;

    void hostName ( char *pBuf, unsigned bufLength ) const;
    const char * pHostName () const; // deprecated - please do not use
    bool isVirtaulCircuit ( const char *pChannelName, const osiSockAddr &addr ) const;
    bool alive () const;
    osiSockAddr getNetworkAddress () const;

private:
    tcpRecvWatchdog recvDog;
    tcpSendWatchdog sendDog;
    comQueSend sendQue;
    comQueRecv recvQue;
    caHdrLargeArray curMsg;
    arrayElementCount curDataMax;
    arrayElementCount curDataBytes;
    epics_auto_ptr < hostNameCache > pHostNameCache;
    char * pCurData;
    unsigned minorProtocolVersion;
    iiu_conn_state state;
    epicsEvent sendThreadFlushEvent;
    epicsEvent sendThreadExitEvent;
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

    void processIncoming ();

    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );

    friend void cacSendThreadTCP ( void *pParam );
    friend void cacRecvThreadTCP ( void *pParam );

    void lastChannelDetachNotify ();

    // send protocol stubs
    void echoRequest ();
    void versionMessage ( const cacChannel::priLev & priority );
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
    void flushIfRecvProcessRequested ();
    bool flush (); // only to be called by the send thread
};

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

inline void comQueSend::pushUInt16 ( const ca_uint16_t value )
{
    this->copyIn ( value );
}

inline void comQueSend::pushUInt32 ( const ca_uint32_t value )
{
    this->copyIn ( value );
}

inline void comQueSend::pushFloat32 ( const ca_float32_t value )
{
    this->copyIn ( value );
}

inline void comQueSend::pushString ( const char *pVal, unsigned nChar )
{
    this->copyIn ( pVal, nChar );
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

inline unsigned comQueRecv::occupiedBytes () const
{
    return this->nBytesPending;
}

inline epicsInt8 comQueRecv::popInt8 ()
{
    return static_cast < epicsInt8 > ( this->popUInt8() );
}

inline epicsInt16 comQueRecv::popInt16 ()
{
    epicsInt16 tmp;
    tmp  = this->popInt8() << 8u;
    tmp |= this->popInt8() << 0u;
    return tmp;
}

inline epicsInt32 comQueRecv::popInt32 ()
{
    epicsInt32 tmp ;
    tmp |= this->popInt8() << 24u;
    tmp |= this->popInt8() << 16u;
    tmp |= this->popInt8() << 8u;
    tmp |= this->popInt8() << 0u;
    return tmp;
}

inline epicsFloat32 comQueRecv::popFloat32 ()
{
    epicsFloat32 tmp;
    epicsUInt8 wire[ sizeof ( tmp ) ];
    for ( unsigned i = 0u; i < sizeof ( tmp ); i++ ) {
        wire[i] = this->popUInt8 ();
    }
    osiConvertFromWireFormat ( tmp, wire );
    return tmp;
}

inline epicsFloat64 comQueRecv::popFloat64 ()
{
    epicsFloat64 tmp;
    epicsUInt8 wire[ sizeof ( tmp ) ];
    for ( unsigned i = 0u; i < sizeof ( tmp ); i++ ) {
        wire[i] = this->popUInt8 ();
    }
    osiConvertFromWireFormat ( tmp, wire );
    return tmp;
}

inline void comQueRecv::popString ( epicsOldString *pStr )
{
    for ( unsigned i = 0u; i < sizeof ( *pStr ); i++ ) {
        pStr[0][i] = this->popInt8 ();
    }
}

inline void tcpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    this->pHostNameCache->hostName ( pBuf, bufLength );
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
    this->sendThreadFlushEvent.signal ();
}

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

#endif // ifdef virtualCircuith

