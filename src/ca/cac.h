
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

#ifdef epicsExportSharedSymbols
#define cach_restore_epicsExportSharedSymbols
#undef epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "ipAddrToAsciiAsynchronous.h"
#include "epicsTimer.h"
#include "epicsEvent.h"
#include "freeList.h"

#ifdef cach_restore_epicsExportSharedSymbols
#define epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "nciu.h"
#include "bhe.h"
#include "cacIO.h"
#include "netIO.h"

class netWriteNotifyIO;
class netReadNotifyIO;
class netSubscription;

// used to control access to cac's recycle routines which
// should only be indirectly invoked by CAC when its lock
// is applied
class cacRecycle {              // X aCC 655
public:
    virtual void recycleReadNotifyIO ( netReadNotifyIO &io ) = 0;
    virtual void recycleWriteNotifyIO ( netWriteNotifyIO &io ) = 0;
    virtual void recycleSubscription ( netSubscription &io ) = 0;
};

struct CASG;
class inetAddrID;
class caServerID;
struct caHdrLargeArray;

extern epicsThreadPrivateId caClientCallbackThreadId;

class callbackMutex {
public:
    callbackMutex ( bool threadsMayBeBlockingForRecvThreadsToFinish );
    ~callbackMutex ();
    void lock ();
    void unlock ();
    void waitUntilNoRecvThreadsPending ();
private:
    epicsMutex countMutex;
    epicsMutex primaryMutex; 
    epicsEvent noRecvThreadsPending;
    unsigned recvThreadsPendingCount;
    bool threadsMayBeBlockingForRecvThreadsToFinish;
    callbackMutex ( callbackMutex & );
    callbackMutex & operator = ( callbackMutex & );
};

class cacMutex {
public:
    void lock ();
    void unlock ();
    void show ( unsigned level ) const;
private:
    epicsMutex mutex;
};

class cacDisconnectChannelPrivate {
public:
    virtual void disconnectChannel ( 
        epicsGuard < callbackMutex > &, 
        epicsGuard < cacMutex > &, nciu & chan ) = 0;
};

class cac : private cacRecycle, private cacDisconnectChannelPrivate
{
public:
    cac ( cacNotify &, bool enablePreemptiveCallback = false );
    virtual ~cac ();

    // beacon management
    void beaconNotify ( const inetAddrID & addr, const epicsTime & currentTime, 
        unsigned beaconNumber, unsigned protocolRevision );
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
    bool executeResponse ( epicsGuard < callbackMutex > &, tcpiiu &, 
        caHdrLargeArray &, char *pMsgBody );
    void ioCancel ( nciu &chan, const cacChannel::ioid &id );
    void ioShow ( const cacChannel::ioid &id, unsigned level ) const;

    // channel routines
    bool lookupChannelAndTransferToTCP ( 
            epicsGuard < callbackMutex > &,
            unsigned cid, unsigned sid, 
            ca_uint16_t typeCode, arrayElementCount count, 
            unsigned minorVersionNumber, const osiSockAddr &, 
            const epicsTime & currentTime );
    void uninstallChannel ( nciu & );
    cacChannel & createChannel ( const char *name_str, 
        cacChannelNotify &chan, cacChannel::priLev pri );
    void registerService ( cacService &service );
    void initiateConnect ( nciu & );

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
    void exception ( epicsGuard < callbackMutex > &, int status, const char * pContext,
        const char * pFileName, unsigned lineNo );

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
    cacMutex & mutexRef ();
    void attachToClientCtx ();
    void selfTest () const;
    void notifyNewFD ( epicsGuard < callbackMutex > &, SOCKET ) const;
    void notifyDestroyFD ( epicsGuard < callbackMutex > &, SOCKET ) const;
    void uninstallIIU ( tcpiiu &iiu ); 
    bool preemptiveCallbakIsEnabled () const;
    double beaconPeriod ( const nciu & chan ) const;
    static unsigned lowestPriorityLevelAbove ( unsigned priority );
    static unsigned highestPriorityLevelBelow ( unsigned priority );
    void tcpCircuitShutdown ( tcpiiu &, bool discardMessages );

private:
    ipAddrToAsciiEngine         ipToAEngine;
    cacServiceList              services;
    chronIntIdResTable
        < nciu >                chanTable;
    chronIntIdResTable 
        < baseNMIU >            ioTable;
    chronIntIdResTable
        < CASG >                sgTable;
    resTable 
        < bhe, inetAddrID >     beaconTable;
    resTable 
        < tcpiiu, caServerID >  serverTable;
    tsFreeList 
        < class netReadNotifyIO, 1024, epicsMutexNOOP > 
                                freeListReadNotifyIO;
    tsFreeList 
        < class netWriteNotifyIO, 1024, epicsMutexNOOP > 
                                freeListWriteNotifyIO;
    tsFreeList 
        < class netSubscription, 1024, epicsMutexNOOP > 
                                freeListSubscription;
    epicsTime                   programBeginTime;
    double                      connTMO;
    // **** lock hierarchy ****
    // callback lock must always be acquired before
    // the primary mutex if both locks are needed
    callbackMutex               cbMutex;
    mutable cacMutex            mutex; 
    epicsEvent                  ioDone;
    epicsEvent                  iiuUninstall;
    epicsTimerQueueActive       & timerQueue;
    char                        * pUserName;
    class udpiiu                * pudpiiu;
    void                        * tcpSmallRecvBufFreeList;
    void                        * tcpLargeRecvBufFreeList;
    epicsGuard <callbackMutex>  * pCallbackGuard;
    cacNotify                   & notify;
    epicsThreadId               initializingThreadsId;
    unsigned                    initializingThreadsPriority;
    unsigned                    maxRecvBytesTCP;
    unsigned                    pndRecvCnt;
    unsigned                    readSeq;

    void privateUninstallIIU ( epicsGuard < callbackMutex > &, tcpiiu &iiu ); 
    void flushRequestPrivate ();
    void run ();
    void connectAllIO ( epicsGuard < cacMutex > &, nciu &chan );
    void disconnectAllIO ( epicsGuard < cacMutex > & locker, nciu & chan, bool enableCallbacks );
    void flushIfRequired ( epicsGuard < cacMutex > &, netiiu & ); 
    void recycleReadNotifyIO ( netReadNotifyIO &io );
    void recycleWriteNotifyIO ( netWriteNotifyIO &io );
    void recycleSubscription ( netSubscription &io );

    void disconnectChannel ( 
        epicsGuard < callbackMutex > &, epicsGuard < cacMutex > &, nciu & chan );

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
    bool noopAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool echoRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool writeNotifyRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool readNotifyRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool eventRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool readRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool clearChannelRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool exceptionRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool accessRightsRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool claimCIURespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    bool verifyAndDisconnectChan ( epicsGuard < callbackMutex > &, tcpiiu &, 
                const caHdrLargeArray &, void *pMsgBdy );
    bool badTCPRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    typedef bool ( cac::*pProtoStubTCP ) ( 
        epicsGuard < callbackMutex > &, tcpiiu &, 
        const caHdrLargeArray &, void *pMsgBdy );
    static const pProtoStubTCP tcpJumpTableCAC [];

    bool defaultExcep ( epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool eventAddExcep ( epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool readExcep ( epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool writeExcep ( epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool clearChanExcep ( epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool readNotifyExcep ( epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool writeNotifyExcep ( epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    typedef bool ( cac::*pExcepProtoStubTCP ) ( 
                    epicsGuard < callbackMutex > &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    static const pExcepProtoStubTCP tcpExcepJumpTableCAC [];

	cac ( const cac & );
	cac & operator = ( const cac & );
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

inline cacMutex & cac::mutexRef ()
{
    return this->mutex;
}

inline void cac::exception ( epicsGuard < callbackMutex > &, int status, 
    const char *pContext, const char *pFileName, unsigned lineNo )
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

inline bool cac::preemptiveCallbakIsEnabled () const
{
    return ! this->pCallbackGuard;
}

inline void cacMutex::lock ()
{
    this->mutex.lock ();
}

inline void cacMutex::unlock ()
{
    this->mutex.unlock ();
}

inline void cacMutex::show ( unsigned level ) const
{
    this->mutex.show ( level );
}

#endif // ifdef cach

