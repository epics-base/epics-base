/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *
 *  Author: Jeff Hill
 *
 */

#ifndef cach
#define cach

#ifdef epicsExportSharedSymbols
#   define cach_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "compilerDependencies.h"
#include "ipAddrToAsciiAsynchronous.h"
#include "msgForMultiplyDefinedPV.h"
#include "epicsTimer.h"
#include "epicsEvent.h"
#include "freeList.h"
#include "localHostName.h"

#ifdef cach_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "nciu.h"
#include "comBuf.h"
#include "bhe.h"
#include "cacIO.h"
#include "netIO.h"
#include "localHostName.h"
#include "virtualCircuit.h"

class netWriteNotifyIO;
class netReadNotifyIO;
class netSubscription;

// used to control access to cac's recycle routines which
// should only be indirectly invoked by CAC when its lock
// is applied
class cacRecycle {
public:
    virtual void recycleReadNotifyIO ( 
        epicsGuard < epicsMutex > &, netReadNotifyIO &io ) = 0;
    virtual void recycleWriteNotifyIO ( 
        epicsGuard < epicsMutex > &, netWriteNotifyIO &io ) = 0;
    virtual void recycleSubscription ( 
        epicsGuard < epicsMutex > &, netSubscription &io ) = 0;
protected:
    virtual ~cacRecycle() {}
};

struct CASG;
class inetAddrID;
class caServerID;
struct caHdrLargeArray;

class cacComBufMemoryManager : public comBufMemoryManager
{
public:
    cacComBufMemoryManager () {}
    void * allocate ( size_t );
    void release ( void * );
private:
    tsFreeList < comBuf, 0x20 > freeList;
	cacComBufMemoryManager ( const cacComBufMemoryManager & );
	cacComBufMemoryManager & operator = ( const cacComBufMemoryManager & );
};

class notifyGuard {
public:
    notifyGuard ( cacContextNotify & );
    ~notifyGuard ();
private:
    cacContextNotify & notify;
    notifyGuard ( const notifyGuard & );
    notifyGuard & operator = ( const notifyGuard & );
};

class callbackManager : public notifyGuard {
public:
    callbackManager ( 
        cacContextNotify &, 
        epicsMutex & callbackControl );
    epicsGuard < epicsMutex > cbGuard;
};

class cac : 
    public cacContext,
    private cacRecycle, 
    private callbackForMultiplyDefinedPV
{
public:
    cac ( 
        epicsMutex & mutualExclusion, 
        epicsMutex & callbackControl, 
        cacContextNotify & );
    virtual ~cac ();

    // beacon management
    void beaconNotify ( const inetAddrID & addr, const epicsTime & currentTime, 
        ca_uint32_t beaconNumber, unsigned protocolRevision );
    unsigned beaconAnomaliesSinceProgramStart (
        epicsGuard < epicsMutex > & ) const;

    // IO management
    void flush ( epicsGuard < epicsMutex > & guard );
    bool executeResponse ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, caHdrLargeArray &, char *pMsgBody );

    // channel routines
    void transferChanToVirtCircuit ( 
        unsigned cid, unsigned sid, 
        ca_uint16_t typeCode, arrayElementCount count, 
        unsigned minorVersionNumber, const osiSockAddr &,
        const epicsTime & currentTime );
    cacChannel & createChannel ( 
        epicsGuard < epicsMutex > & guard, const char * pChannelName, 
        cacChannelNotify &, cacChannel::priLev );
    void destroyChannel ( 
        epicsGuard < epicsMutex > &, nciu & );
    void initiateConnect ( 
        epicsGuard < epicsMutex > &, nciu &, netiiu * & );
    nciu * lookupChannel (
        epicsGuard < epicsMutex > &, const cacChannel::ioid & );

    // IO requests
    netWriteNotifyIO & writeNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, privateInterfaceForIO &,
        unsigned type, arrayElementCount nElem, const void * pValue, 
        cacWriteNotify & );
    netReadNotifyIO & readNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, privateInterfaceForIO &,
        unsigned type, arrayElementCount nElem, 
        cacReadNotify & );
    netSubscription & subscriptionRequest ( 
        epicsGuard < epicsMutex > &, nciu &, privateInterfaceForIO &,
        unsigned type, arrayElementCount nElem, unsigned mask, 
        cacStateNotify &, bool channelIsInstalled );
    bool destroyIO (
        CallbackGuard & callbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard,
        const cacChannel::ioid & idIn, 
        nciu & chan );
    void disconnectAllIO ( 
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard, 
        nciu &, tsDLList < baseNMIU > & ioList );

    void ioShow ( 
        epicsGuard < epicsMutex > & guard,
        const cacChannel::ioid &id, unsigned level ) const;

    // exception generation
    void exception ( 
        epicsGuard < epicsMutex > & cbGuard, 
        epicsGuard < epicsMutex > & guard,
        int status, const char * pContext,
        const char * pFileName, unsigned lineNo );

    // search destination management
    void registerSearchDest (
        epicsGuard < epicsMutex > &, SearchDest & req );
    bool findOrCreateVirtCircuit (
        epicsGuard < epicsMutex > &, const osiSockAddr &,
        unsigned, tcpiiu *&, unsigned, SearchDestTCP * pSearchDest = NULL );

    // diagnostics
    unsigned circuitCount ( epicsGuard < epicsMutex > & ) const;
    void show ( epicsGuard < epicsMutex > &, unsigned level ) const;
    int printFormated ( 
        epicsGuard < epicsMutex > & callbackControl, 
        const char *pformat, ... ) const;
    int varArgsPrintFormated ( 
        epicsGuard < epicsMutex > & callbackControl, 
        const char *pformat, va_list args ) const;
    double connectionTimeout ( epicsGuard < epicsMutex > & );

    // buffer management
    char * allocateSmallBufferTCP ();
    void releaseSmallBufferTCP ( char * );
    unsigned largeBufferSizeTCP () const;
    char * allocateLargeBufferTCP ();
    void releaseLargeBufferTCP ( char * );
    unsigned maxContiguousFrames ( epicsGuard < epicsMutex > & ) const;

    // misc
    const char * userNamePointer () const;
    unsigned getInitializingThreadsPriority () const;
    epicsMutex & mutexRef ();
    void attachToClientCtx ();
    void selfTest ( 
        epicsGuard < epicsMutex > & ) const;
    double beaconPeriod ( 
        epicsGuard < epicsMutex > &,
        const nciu & chan ) const;
    static unsigned lowestPriorityLevelAbove ( unsigned priority );
    static unsigned highestPriorityLevelBelow ( unsigned priority );
    void destroyIIU ( tcpiiu & iiu ); 

    const char * pLocalHostName ();
    
private:
    epicsSingleton < localHostName > :: reference _refLocalHostName;
    chronIntIdResTable < nciu > chanTable;
    //
    // !!!! There is at this point no good reason
    // !!!! to maintain one IO table for all types of
    // !!!! IO. It would probably be better to maintain
    // !!!! an independent table for each IO type. The
    // !!!! new adaptive sized hash table will not 
    // !!!! waste memory. Making this change will 
    // !!!! avoid virtual function overhead when 
    // !!!! accessing the different types of IO. This
    // !!!! approach would also probably be safer in 
    // !!!! terms of detecting damaged protocol.
    //
    chronIntIdResTable < baseNMIU > ioTable;
    resTable < bhe, inetAddrID > beaconTable;
    resTable < tcpiiu, caServerID > serverTable;
    tsDLList < tcpiiu > circuitList;
    tsDLList < SearchDest > searchDestList;
    tsFreeList 
        < class tcpiiu, 32, epicsMutexNOOP > 
            freeListVirtualCircuit;
    tsFreeList 
        < class netReadNotifyIO, 1024, epicsMutexNOOP > 
            freeListReadNotifyIO;
    tsFreeList 
        < class netWriteNotifyIO, 1024, epicsMutexNOOP > 
            freeListWriteNotifyIO;
    tsFreeList 
        < class netSubscription, 1024, epicsMutexNOOP > 
            freeListSubscription;
    tsFreeList 
        < class nciu, 1024, epicsMutexNOOP > 
            channelFreeList;
    tsFreeList 
        < class msgForMultiplyDefinedPV, 16 > 
            mdpvFreeList;
    cacComBufMemoryManager comBufMemMgr;
    bheFreeStore bheFreeList;
    epicsTime programBeginTime;
    double connTMO;
    // **** lock hierarchy ****
    // 1) callback lock must always be acquired before
    // the primary mutex if both locks are needed
    epicsMutex & mutex;
    epicsMutex & cbMutex;
    epicsEvent iiuUninstall;
    ipAddrToAsciiEngine & ipToAEngine;
    epicsTimerQueueActive & timerQueue;
    char * pUserName;
    class udpiiu * pudpiiu;
    void * tcpSmallRecvBufFreeList;
    void * tcpLargeRecvBufFreeList;
    cacContextNotify & notify;
    epicsThreadId initializingThreadsId;
    unsigned initializingThreadsPriority;
    unsigned maxRecvBytesTCP;
    unsigned maxContigFrames;
    unsigned beaconAnomalyCount;
    unsigned short _serverPort;
    unsigned iiuExistenceCount;
    bool cacShutdownInProgress;

    void recycleReadNotifyIO ( 
        epicsGuard < epicsMutex > &, netReadNotifyIO &io );
    void recycleWriteNotifyIO ( 
        epicsGuard < epicsMutex > &, netWriteNotifyIO &io );
    void recycleSubscription ( 
        epicsGuard < epicsMutex > &, netSubscription &io );

    void disconnectChannel ( 
        epicsGuard < epicsMutex > & cbGuard, 
        epicsGuard < epicsMutex > & guard, nciu & chan );

    void ioExceptionNotify ( unsigned id, int status, 
        const char * pContext, unsigned type, arrayElementCount count );
    void ioExceptionNotifyAndUninstall ( unsigned id, int status, 
        const char * pContext, unsigned type, arrayElementCount count );

    void pvMultiplyDefinedNotify ( msgForMultiplyDefinedPV & mfmdpv, 
        const char * pChannelName, const char * pAcc, const char * pRej );

    // recv protocol stubs
    bool versionAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool echoRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool writeNotifyRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool searchRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool readNotifyRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool eventRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool readRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool clearChannelRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool exceptionRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool accessRightsRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool createChannelRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool verifyAndDisconnectChan ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    bool badTCPRespAction ( callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );

    typedef bool ( cac::*pProtoStubTCP ) ( 
        callbackManager &, tcpiiu &, 
        const epicsTime & currentTime, const caHdrLargeArray &, void *pMsgBdy );
    static const pProtoStubTCP tcpJumpTableCAC [];

    bool defaultExcep ( callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool eventAddExcep ( callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool readExcep ( callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool writeExcep ( callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool clearChanExcep ( callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool readNotifyExcep ( callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    bool writeNotifyExcep ( callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    typedef bool ( cac::*pExcepProtoStubTCP ) ( 
                    callbackManager &, tcpiiu &iiu, const caHdrLargeArray &hdr, 
                    const char *pCtx, unsigned status );
    static const pExcepProtoStubTCP tcpExcepJumpTableCAC [];

	cac ( const cac & );
	cac & operator = ( const cac & );
};

inline const char * cac::userNamePointer () const
{
    return this->pUserName;
}

inline unsigned cac::getInitializingThreadsPriority () const
{
    return this->initializingThreadsPriority;
}

inline epicsMutex & cac::mutexRef ()
{
    return this->mutex;
}

inline int cac :: varArgsPrintFormated ( 
    epicsGuard < epicsMutex > & callbackControl, 
    const char *pformat, va_list args ) const
{
    callbackControl.assertIdenticalMutex ( this->cbMutex );
    return this->notify.varArgsPrintFormated ( pformat, args );
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

inline unsigned cac::beaconAnomaliesSinceProgramStart (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->beaconAnomalyCount;
}

inline notifyGuard::notifyGuard ( cacContextNotify & notifyIn ) :
    notify ( notifyIn )
{
    this->notify.callbackProcessingInitiateNotify ();
}

inline notifyGuard::~notifyGuard ()
{
    this->notify.callbackProcessingCompleteNotify ();
}

inline callbackManager::callbackManager ( 
    cacContextNotify & notify, epicsMutex & callbackControl ) :
    notifyGuard ( notify ), cbGuard ( callbackControl )
{
}

inline nciu * cac::lookupChannel (
    epicsGuard < epicsMutex > & guard, 
    const cacChannel::ioid & idIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->chanTable.lookup ( idIn );
}

inline const char * cac :: pLocalHostName ()
{
    return _refLocalHostName->pointer ();
}

inline unsigned cac ::
    maxContiguousFrames ( epicsGuard < epicsMutex > & ) const
{
    return maxContigFrames;
}    

inline double cac ::
    connectionTimeout ( epicsGuard < epicsMutex > & guard )
{    
    guard.assertIdenticalMutex ( this->mutex );
    return this->connTMO;
}

#endif // ifdef cach
