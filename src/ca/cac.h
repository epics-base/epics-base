
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

extern epicsThreadPrivateId caClientCallbackThreadId;

class cac : private cacRecycle
{
public:
    cac ( cacNotify &, bool enablePreemptiveCallback = false );
    virtual ~cac ();

    // beacon management
    void beaconNotify ( const inetAddrID &addr, 
        const epicsTime &currentTime );
    void repeaterSubscribeConfirmNotify ();

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
    void destroyAllIO ( nciu &chan );
    bool executeResponse ( tcpiiu &, caHdrLargeArray &, char *pMsgBody );
    void ioCancel ( nciu &chan, const cacChannel::ioid &id );
    void ioShow ( const cacChannel::ioid &id, unsigned level ) const;

    // channel routines
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
    int blockForEventAndEnableCallbacks ( epicsEvent &event, double timeout );

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

    // buffer management
    char * allocateSmallBufferTCP ();
    void releaseSmallBufferTCP ( char * );
    unsigned largeBufferSizeTCP () const;
    char * allocateLargeBufferTCP ();
    void releaseLargeBufferTCP ( char * );

    // misc
    const char * userNamePointer () const;
    unsigned getInitializingThreadsPriority () const;
    epicsMutex & mutexRef ();
    void attachToClientCtx ();
    void selfTest () const;
    void notifyNewFD ( SOCKET ) const;
    void notifyDestroyFD ( SOCKET ) const;
    void uninstallIIU ( tcpiiu &iiu ); 

private:
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
    epicsMutex              callbackMutex; 
    epicsMutex              serializePendIO; 
    epicsMutex              serializePendEvent; 
    epicsEvent              ioDone;
    epicsEvent              noRecvThreadsPending;
    epicsEvent              iiuUninstal;
    epicsTimerQueueActive   *pTimerQueue;
    char                    *pUserName;
    class udpiiu            *pudpiiu;
    class searchTimer       *pSearchTmr;
    class repeaterSubscribeTimer  
                            *pRepeaterSubscribeTmr;
    void                    *tcpSmallRecvBufFreeList;
    void                    *tcpLargeRecvBufFreeList;
    cacNotify               &notify;
    unsigned                initializingThreadsPriority;
    unsigned                maxRecvBytesTCP;
    unsigned                pndRecvCnt;
    unsigned                readSeq;
    unsigned                recvThreadsPendingCount;
    bool                    enablePreemptiveCallback;

    void flushRequestPrivate ();
    void run ();
    bool setupUDP ();
    void connectAllIO ( nciu &chan );
    void disconnectAllIO ( nciu &chan, bool enableCallbacks );
    void privateDestroyAllIO ( nciu & chan );
    void ioCancelPrivate ( nciu &chan, const cacChannel::ioid &id );
    void flushIfRequired ( netiiu & ); 
    void recycleReadNotifyIO ( netReadNotifyIO &io );
    void recycleWriteNotifyIO ( netWriteNotifyIO &io );
    void recycleSubscription ( netSubscription &io );
    void preemptiveCallbackLock ();
    void preemptiveCallbackUnlock ();

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

    friend class callbackAutoMutex;
};

class callbackAutoMutex {
public:
    callbackAutoMutex ( cac & ctxIn ) : ctx ( ctxIn )
    {
        this->ctx.preemptiveCallbackLock ();
    }
    ~callbackAutoMutex ()
    {
        this->ctx.preemptiveCallbackUnlock ();
    }
private:
    cac & ctx;
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

inline unsigned cac::sequenceNumberOfOutstandingIO () const
{
    return this->readSeq;
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
    // this locks internally
    return ( char * ) freeListMalloc ( this->tcpSmallRecvBufFreeList );
}

inline void cac::releaseSmallBufferTCP ( char *pBuf )
{
    // this locks internally
    freeListFree ( this->tcpSmallRecvBufFreeList, pBuf );
}

inline unsigned cac::largeBufferSizeTCP () const
{
    return this->maxRecvBytesTCP;
}

inline char * cac::allocateLargeBufferTCP ()
{
    // this locks internally
    return ( char * ) freeListMalloc ( this->tcpLargeRecvBufFreeList );
}

inline void cac::releaseLargeBufferTCP ( char *pBuf )
{
    // this locks internally
    freeListFree ( this->tcpLargeRecvBufFreeList, pBuf );
}

inline bool cac::ioComplete () const
{
    return ( this->pndRecvCnt == 0u );
}

#endif // ifdef cach

