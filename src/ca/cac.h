
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

#ifndef cach
#define cach

#include "ipAddrToAsciiAsynchronous.h"
#include "epicsTimer.h"
#include "epicsEvent.h"
#include "freeList.h"

#include "nciu.h"

#define epicsExportSharedSymbols
#include "bhe.h"
#include "cacIO.h"
#undef epicsExportSharedSymbols

class ioCounterNet {
public:
    ioCounterNet ();
    void increment ();
    void decrement ();
    void decrement ( unsigned seqNumber );
    unsigned sequenceNumber () const;
    unsigned currentCount () const;
    void cleanUp ();
    void show ( unsigned level ) const;
    void waitForCompletion ( double delaySec );
private:
    unsigned pndrecvcnt;
    unsigned readSeq;
    epicsMutex mutex;
    epicsEvent ioDone;
};

class recvProcessThread : public epicsThreadRunable {
public:
    recvProcessThread ( class cac *pcacIn );
    virtual ~recvProcessThread ();
    void run ();
    void enable ();
    void disable ();
    void signalActivity ();
    bool isCurrentThread () const;
    void show ( unsigned level ) const;
private:
    //
    // The additional complexity associated with
    // "processingDone" event and the "processing" flag
    // avoid complex locking hierarchy constraints
    // and therefore reduces the chance of creating
    // a deadlock window during code maintenance.
    //
    epicsThread thread;
    epicsEvent recvActivity;
    class cac *pcac;
    epicsEvent exit;
    epicsEvent processingDone;
    mutable epicsMutex mutex;
    unsigned enableRefCount;
    unsigned blockingForCompletion;
    bool processing;
    bool shutDown;
};

class netWriteNotifyIO;
class netReadNotifyIO;
class netSubscription;

// used to control access to cac's recycle routines which
// should only be indirectly invoked by CAC when its lock
// is applied
class cacRecycle {
public:
    virtual void recycleReadNotifyIO ( netReadNotifyIO &io ) = 0;
    virtual void recycleWriteNotifyIO ( netWriteNotifyIO &io ) = 0;
    virtual void recycleSubscription ( netSubscription &io ) = 0;
};

struct CASG;
class inetAddrID;
struct caHdrLargeArray;

class cac : private cacRecycle
{
public:
    cac ( cacNotify &, bool enablePreemptiveCallback = false );
    virtual ~cac ();

    // beacon management
    void beaconNotify ( const inetAddrID &addr, 
        const epicsTime &currentTime );
    void repeaterSubscribeConfirmNotify ();

    // IIU routines
    void signalRecvActivity ();
    void processRecvBacklog ();

    // outstanding IO count management routines
    void incrementOutstandingIO ();
    void decrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned sequenceNo );
    unsigned sequenceNumberOfOutstandingIO () const;
    bool ioComplete () const;

    // IO management
    void flushRequest ();
    int pendIO ( const double &timeout );
    int pendEvent ( const double &timeout );
    void connectAllIO ( nciu &chan );
    void disconnectAllIO ( nciu &chan );
    void destroyAllIO ( nciu &chan );
    bool executeResponse ( tcpiiu &, caHdrLargeArray &, char *pMsgBody );
    void ioCancel ( nciu &chan, const cacChannel::ioid &id );
    void ioShow ( const cacChannel::ioid &id, unsigned level ) const;

    // channel routines
    bool connectChannel ( unsigned id );
    void installNetworkChannel ( nciu &, netiiu *&piiu );
    bool lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             ca_uint16_t typeCode, arrayElementCount count, unsigned minorVersionNumber,
             const osiSockAddr &, const epicsTime & currentTime );
    void uninstallChannel ( nciu & );
    cacChannel & createChannel ( const char *name_str, cacChannelNotify &chan );
    void registerService ( cacService &service );

    // IO request stubs
    void writeRequest ( nciu &, unsigned type, 
        unsigned nElem, const void *pValue );
    cacChannel::ioid writeNotifyRequest ( nciu &, unsigned type, 
        unsigned nElem, const void *pValue, cacWriteNotify & );
    cacChannel::ioid readNotifyRequest ( nciu &, unsigned type, 
        unsigned nElem, cacReadNotify & );
    cacChannel::ioid subscriptionRequest ( nciu &, unsigned type, 
        arrayElementCount nElem, unsigned mask, cacStateNotify & );

    // sync group routines
    CASG * lookupCASG ( unsigned id );
    void installCASG ( CASG & );
    void uninstallCASG ( CASG & );

    // exception generation
    void exception ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo );

    // callback preemption control
    void enableCallbackPreemption ();
    void disableCallbackPreemption ();

    // diagnostics
    unsigned connectionCount () const;
    void show ( unsigned level ) const;
    int printf ( const char *pformat, ... ) const;
    int vPrintf ( const char *pformat, va_list args ) const;
    void ipAddrToAsciiAsynchronousRequestInstall ( ipAddrToAsciiAsynchronous & request );
    void signal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, ... );
    void vSignal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, va_list args );

    // misc
    const char * userNamePointer () const;
    unsigned getInitializingThreadsPriority () const;
    epicsMutex & mutexRef ();
    void attachToClientCtx ();
    char * allocateSmallBufferTCP ();
    void releaseSmallBufferTCP ( char * );
    unsigned largeBufferSizeTCP () const;
    char * allocateLargeBufferTCP ();
    void releaseLargeBufferTCP ( char * );

private:
    ioCounterNet            ioCounter;
    ipAddrToAsciiEngine     ipToAEngine;
    cacServiceList          services;
    tsDLList < tcpiiu >     iiuList;
    chronIntIdResTable
        < nciu >            chanTable;
    chronIntIdResTable 
        < baseNMIU >        ioTable;
    chronIntIdResTable
        < CASG >            sgTable;
    resTable 
        < bhe, inetAddrID > beaconTable;
    tsFreeList 
        < class netReadNotifyIO, 1024 > 
                            freeListReadNotifyIO;
    tsFreeList 
        < class netWriteNotifyIO, 1024 > 
                            freeListWriteNotifyIO;
    tsFreeList 
        < class netSubscription, 1024 > 
                            freeListSubscription;
    epicsTime               programBeginTime;
    double                  connTMO;
    mutable epicsMutex      mutex; 
    epicsEvent              notifyCompletionEvent;
    epicsTimerQueueActive   *pTimerQueue;
    char                    *pUserName;
    recvProcessThread       *pRecvProcThread;
    class udpiiu            *pudpiiu;
    class searchTimer       *pSearchTmr;
    class repeaterSubscribeTimer  
                            *pRepeaterSubscribeTmr;
    void                    *tcpSmallRecvBufFreeList;
    void                    *tcpLargeRecvBufFreeList;
    cacNotify               &notify;
    unsigned                ioNotifyInProgressId;
    unsigned                initializingThreadsPriority;
    unsigned                threadsBlockingOnNotifyCompletion;
    unsigned                maxRecvBytesTCP;
    bool                    enablePreemptiveCallback;
    bool                    ioInProgress;
    bool setupUDP ();
    void flushIfRequired ( nciu & ); // lock must be applied
    void recycleReadNotifyIO ( netReadNotifyIO &io );
    void recycleWriteNotifyIO ( netWriteNotifyIO &io );
    void recycleSubscription ( netSubscription &io );

    void ioCompletionNotify ( unsigned id, unsigned type, 
        arrayElementCount count, const void *pData );
    void ioExceptionNotify ( unsigned id, 
        int status, const char *pContext );
    void ioExceptionNotify ( unsigned id, int status, 
        const char *pContext, unsigned type, arrayElementCount count );

    void ioCompletionNotifyAndDestroy ( unsigned id );
    void ioCompletionNotifyAndDestroy ( unsigned id, 
        unsigned type, arrayElementCount count, const void *pData );
    void ioExceptionNotifyAndDestroy ( unsigned id, 
       int status, const char *pContext );
    void ioExceptionNotifyAndDestroy ( unsigned id, 
       int status, const char *pContext, unsigned type, arrayElementCount count );
    bool blockForIOCallbackCompletion ( const cacChannel::ioid & id );

    // recv protocol stubs
    bool noopAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool echoRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool writeNotifyRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool readNotifyRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool eventRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool readRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool clearChannelRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool exceptionRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool accessRightsRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool claimCIURespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool verifyAndDisconnectChan ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    bool badTCPRespAction ( tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    typedef bool ( cac::*pProtoStubTCP ) ( 
        tcpiiu &, const caHdrLargeArray &, void *pMsgBdy );
    static const pProtoStubTCP tcpJumpTableCAC [];

    bool defaultExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool eventAddExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool readExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool writeExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool clearChanExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool readNotifyExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool writeNotifyExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    typedef bool ( cac::*pExcepProtoStubTCP ) ( 
                    tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    static const pExcepProtoStubTCP tcpExcepJumpTableCAC [];
};

inline const char * cac::userNamePointer () const
{
    return this->pUserName;
}

inline void cac::ipAddrToAsciiAsynchronousRequestInstall ( ipAddrToAsciiAsynchronous & request )
{
    request.ioInitiate ( this->ipToAEngine );
}

inline unsigned cac::getInitializingThreadsPriority () const
{
    return this->initializingThreadsPriority;
}

inline void cac::incrementOutstandingIO ()
{
    this->ioCounter.increment ();
}

inline void cac::decrementOutstandingIO ()
{
    this->ioCounter.decrement ();
}

inline void cac::decrementOutstandingIO ( unsigned sequenceNo )
{
    this->ioCounter.decrement ( sequenceNo );
}

inline unsigned cac::sequenceNumberOfOutstandingIO () const
{
    return this->ioCounter.sequenceNumber ();
}

inline epicsMutex & cac::mutexRef ()
{
    return this->mutex;
}

inline void cac::exception ( int status, const char *pContext,
    const char *pFileName, unsigned lineNo )
{
    this->notify.exception ( status, pContext, pFileName, lineNo );
}

inline int cac::vPrintf ( const char *pformat, va_list args ) const
{
    return this->notify.vPrintf ( pformat, args );
}

inline void cac::attachToClientCtx ()
{
    this->notify.attachToClientCtx ();
}

inline char * cac::allocateSmallBufferTCP ()
{
    return ( char * ) freeListMalloc ( this->tcpSmallRecvBufFreeList );
}

inline void cac::releaseSmallBufferTCP ( char *pBuf )
{
    freeListFree ( this->tcpSmallRecvBufFreeList, pBuf );
}

inline unsigned cac::largeBufferSizeTCP () const
{
    return this->maxRecvBytesTCP;
}

inline char * cac::allocateLargeBufferTCP ()
{
    return ( char * ) freeListMalloc ( this->tcpLargeRecvBufFreeList );
}

inline void cac::releaseLargeBufferTCP ( char *pBuf )
{
    freeListFree ( this->tcpLargeRecvBufFreeList, pBuf );
}

inline bool recvProcessThread::isCurrentThread () const
{
    return this->thread.isCurrentThread ();
}

#endif // ifdef cach

