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
 *  Copyright, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef oldAccessh
#define oldAccessh

#ifdef epicsExportSharedSymbols
#   define oldAccessh_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "tsFreeList.h"
#include "epicsMemory.h"
#include "compilerDependencies.h"
#include "osiSock.h"

#ifdef oldAccessh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "caProto.h"
#include "cacIO.h"
#include "cadef.h"
#include "syncGroup.h"

struct oldChannelNotify : public cacChannelNotify {
public:
    oldChannelNotify ( 
        epicsGuard < epicsMutex > &, struct ca_client_context &, 
        const char * pName, caCh * pConnCallBackIn, 
        void * pPrivateIn, capri priority );
    void destructor ( 
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void setPrivatePointer ( void * );
    void * privatePointer () const;
    int changeConnCallBack ( caCh *pfunc );
    int replaceAccessRightsEvent ( caArh *pfunc );
    const char *pName () const;
    void show ( unsigned level ) const;
    void initiateConnect (
        epicsGuard < epicsMutex > & );
    void read ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, 
        cacReadNotify &notify, cacChannel::ioid *pId = 0 );
    void read ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, 
        void *pValue );
    void write ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, 
        const void *pValue );
    void write ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, const void *pValue, 
        cacWriteNotify &, cacChannel::ioid *pId = 0 );
    void subscribe ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, unsigned mask, 
        cacStateNotify &, cacChannel::ioid & );
    void ioCancel ( 
        epicsGuard < epicsMutex > & callbackControl, 
        epicsGuard < epicsMutex > & mutualExclusionGuard, 
        const cacChannel::ioid & );
    void ioShow ( const cacChannel::ioid &, unsigned level ) const;
    short nativeType () const;
    arrayElementCount nativeElementCount () const;
    caAccessRights accessRights () const; // defaults to unrestricted access
    unsigned searchAttempts () const; // defaults to zero
    double beaconPeriod () const; // defaults to negative DBL_MAX
    double receiveWatchdogDelay () const; // defaults to negative DBL_MAX
    bool ca_v42_ok () const; 
    bool connected () const; 
    bool previouslyConnected () const;
    void hostName ( char *pBuf, unsigned bufLength ) const; // defaults to local host name
    const char * pHostName () const; // deprecated - please do not use
    ca_client_context & getClientCtx ();
    void * operator new ( size_t size, 
        tsFreeList < struct oldChannelNotify, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void * , 
        tsFreeList < struct oldChannelNotify, 1024, epicsMutexNOOP > & ))
private:
    ca_client_context & cacCtx;
    cacChannel & io;
    caCh * pConnCallBack;
    void * pPrivate;
    caArh * pAccessRightsFunc;
    unsigned ioSeqNo;
    bool currentlyConnected;
    bool prevConnected;
    ~oldChannelNotify ();
    void connectNotify ( epicsGuard < epicsMutex > & );
    void disconnectNotify ( epicsGuard < epicsMutex > & );
    void serviceShutdownNotify ();
    void accessRightsNotify (  
        epicsGuard < epicsMutex > &, const caAccessRights & );
    void exception ( epicsGuard < epicsMutex > &,
        int status, const char * pContext );
    void readException ( epicsGuard < epicsMutex > &,
        int status, const char * pContext,
        unsigned type, arrayElementCount count, void *pValue );
    void writeException ( epicsGuard < epicsMutex > &,
        int status, const char * pContext,
        unsigned type, arrayElementCount count );
	oldChannelNotify ( const oldChannelNotify & );
	oldChannelNotify & operator = ( const oldChannelNotify & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

class getCopy : public cacReadNotify {
public:
    getCopy ( 
        epicsGuard < epicsMutex > & guard, 
        ca_client_context & cacCtx, 
        oldChannelNotify &, unsigned type, 
        arrayElementCount count, void *pValue );
    ~getCopy ();
    void show ( unsigned level ) const;
    void cancel ();
    void * operator new ( size_t size, 
        tsFreeList < class getCopy, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < class getCopy, 1024, epicsMutexNOOP > & ))
private:
    arrayElementCount count;
    ca_client_context & cacCtx;
    oldChannelNotify & chan;
    void * pValue;
    unsigned ioSeqNo;
    unsigned type;
    void completion (
        epicsGuard < epicsMutex > &, unsigned type, 
        arrayElementCount count, const void *pData );
    void exception ( 
        epicsGuard < epicsMutex > &, int status, 
        const char *pContext, unsigned type, arrayElementCount count );
	getCopy ( const getCopy & );
	getCopy & operator = ( const getCopy & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

class getCallback : public cacReadNotify {
public:
    getCallback ( 
        oldChannelNotify & chanIn, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    ~getCallback (); 
    void * operator new ( size_t size, 
        tsFreeList < class getCallback, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < class getCallback, 1024, epicsMutexNOOP > & ))
private:
    oldChannelNotify & chan;
    caEventCallBackFunc * pFunc;
    void * pPrivate;
    void completion (
        epicsGuard < epicsMutex > &, unsigned type, 
        arrayElementCount count, const void *pData);
    void exception ( 
        epicsGuard < epicsMutex > &, int status, 
        const char * pContext, unsigned type, arrayElementCount count );
	getCallback ( const getCallback & );
	getCallback & operator = ( const getCallback & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

class putCallback : public cacWriteNotify {
public:
    putCallback ( 
        oldChannelNotify &, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    ~putCallback ();
    void * operator new ( size_t size, 
        tsFreeList < class putCallback, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < class putCallback, 1024, epicsMutexNOOP > & ))
private:
    oldChannelNotify & chan;
    caEventCallBackFunc * pFunc;
    void *pPrivate;
    void completion ( epicsGuard < epicsMutex > & );
    void exception ( 
        epicsGuard < epicsMutex > &, int status, const char *pContext, 
        unsigned type, arrayElementCount count );
	putCallback ( const putCallback & );
	putCallback & operator = ( const putCallback & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

struct oldSubscription : public cacStateNotify {
public:
    oldSubscription ( 
        oldChannelNotify &, caEventCallBackFunc *pFunc, void *pPrivate );
    ~oldSubscription ();
    void begin ( epicsGuard < epicsMutex > & guard, unsigned type, 
        arrayElementCount nElem, unsigned mask );
    oldChannelNotify & channel () const;
    void * operator new ( size_t size, 
        tsFreeList < struct oldSubscription, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < struct oldSubscription, 1024, epicsMutexNOOP > & ))
    void ioCancel ( epicsGuard < epicsMutex > & cbGuard,
                    epicsGuard < epicsMutex > & guard );
private:
    oldChannelNotify & chan;
    cacChannel::ioid id;
    caEventCallBackFunc * pFunc;
    void * pPrivate;
    bool subscribed;
    void current (
        epicsGuard < epicsMutex > &, unsigned type, 
        arrayElementCount count, const void *pData );
    void exception ( 
        epicsGuard < epicsMutex > &, int status, 
        const char *pContext, unsigned type, arrayElementCount count );
	oldSubscription ( const oldSubscription & );
	oldSubscription & operator = ( const oldSubscription & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

struct ca_client_context : public cacContextNotify
{
public:
    ca_client_context ( bool enablePreemptiveCallback = false );
    virtual ~ca_client_context ();
    void changeExceptionEvent ( caExceptionHandler * pfunc, void * arg );
    void registerForFileDescriptorCallBack ( CAFDHANDLER * pFunc, void * pArg );
    void replaceErrLogHandler ( caPrintfFunc * ca_printf_func );
    cacChannel & createChannel ( 
        epicsGuard < epicsMutex > &, const char * pChannelName, 
        oldChannelNotify &, cacChannel::priLev pri );
    void flush ( epicsGuard < epicsMutex > & );
    int pendIO ( const double & timeout );
    int pendEvent ( const double & timeout );
    bool ioComplete () const;
    void show ( unsigned level ) const;
    unsigned circuitCount () const;
    unsigned sequenceNumberOfOutstandingIO (
        epicsGuard < epicsMutex > & ) const;
    unsigned beaconAnomaliesSinceProgramStart () const;
    void incrementOutstandingIO ( 
        epicsGuard < epicsMutex > &, unsigned ioSeqNo );
    void decrementOutstandingIO ( 
        epicsGuard < epicsMutex > &, unsigned ioSeqNo );
    void exception ( 
        epicsGuard < epicsMutex > &, int status, const char * pContext, 
        const char * pFileName, unsigned lineNo );
    void exception ( 
        epicsGuard < epicsMutex > &, int status, const char * pContext,
        const char * pFileName, unsigned lineNo, oldChannelNotify & chan, 
        unsigned type, arrayElementCount count, unsigned op );
    void blockForEventAndEnableCallbacks ( 
        epicsEvent & event, const double & timeout );
    CASG * lookupCASG ( epicsGuard < epicsMutex > &, unsigned id );
    void installCASG ( epicsGuard < epicsMutex > &, CASG & );
    void uninstallCASG ( epicsGuard < epicsMutex > &, CASG & );
    void selfTest () const;
// perhaps these should be eliminated in deference to the exception mechanism
    int printf ( const char * pformat, ... ) const;
    int vPrintf ( const char * pformat, va_list args ) const;
    void signal ( int ca_status, const char * pfilenm, 
                     int lineno, const char * pFormat, ... );
    void vSignal ( int ca_status, const char * pfilenm, 
                     int lineno, const char  *pFormat, va_list args );
    bool preemptiveCallbakIsEnabled () const;
    void destroyChannel ( oldChannelNotify & chan );
    void destroyGetCopy ( epicsGuard < epicsMutex > &, getCopy & );
    void destroyGetCallback ( epicsGuard < epicsMutex > &, getCallback & );
    void destroyPutCallback ( epicsGuard < epicsMutex > &, putCallback & );
    void destroySubscription ( epicsGuard < epicsMutex > &, oldSubscription & );
    void changeConnCallBack ( caCh * pfunc, caCh * & pConnCallBack, 
        const bool & currentlyConnected );
    epicsMutex & mutexRef () const;

    // exceptions
    class noSocket {};
private:
    chronIntIdResTable < CASG > sgTable;
    tsFreeList < struct oldChannelNotify, 1024, epicsMutexNOOP > oldChannelNotifyFreeList;
    tsFreeList < class getCopy, 1024, epicsMutexNOOP > getCopyFreeList;
    tsFreeList < class getCallback, 1024, epicsMutexNOOP > getCallbackFreeList;
    tsFreeList < class putCallback, 1024, epicsMutexNOOP > putCallbackFreeList;
    tsFreeList < struct oldSubscription, 1024, epicsMutexNOOP > subscriptionFreeList;
    tsFreeList < struct CASG, 128, epicsMutexNOOP > casgFreeList;
    mutable epicsMutex mutex;
    mutable epicsMutex cbMutex; 
    epicsEvent ioDone;
    epicsEvent callbackThreadActivityComplete;
    epics_auto_ptr < epicsGuard < epicsMutex > > pCallbackGuard;
    epics_auto_ptr < cacContext > pServiceContext;
    caExceptionHandler * ca_exception_func;
    void * ca_exception_arg;
    caPrintfFunc * pVPrintfFunc;
    CAFDHANDLER * fdRegFunc;
    void * fdRegArg;
    SOCKET sock;
    unsigned pndRecvCnt;
    unsigned ioSeqNo;
    unsigned callbackThreadsPending;
    ca_uint16_t localPort;
    bool fdRegFuncNeedsToBeCalled;
    bool noWakeupSincePend;

    void attachToClientCtx ();
    void callbackProcessingInitiateNotify ();
    void callbackProcessingCompleteNotify ();
    cacContext & createNetworkContext ( 
        epicsMutex & mutualExclusion, epicsMutex & callbackControl );
	ca_client_context ( const ca_client_context & );
	ca_client_context & operator = ( const ca_client_context & );

    static cacService * pDefaultService;
    static epicsMutex defaultServiceInstallMutex;

    friend int epicsShareAPI ca_create_channel (
        const char * name_str, caCh * conn_func, void * puser,
        capri priority, chid * chanptr );
    friend int epicsShareAPI ca_array_get ( chtype type, 
        arrayElementCount count, chid pChan, void * pValue );
    friend int epicsShareAPI ca_array_get_callback ( chtype type, 
        arrayElementCount count, chid pChan,
        caEventCallBackFunc *pfunc, void *arg );
    friend int epicsShareAPI ca_array_put ( chtype type, 
        arrayElementCount count, chid pChan, const void * pValue );
    friend int epicsShareAPI ca_array_put_callback ( chtype type, 
        arrayElementCount count, chid pChan, const void * pValue, 
        caEventCallBackFunc *pfunc, void *usrarg );
    friend int epicsShareAPI ca_create_subscription ( 
        chtype type, arrayElementCount count, chid pChan, 
        long mask, caEventCallBackFunc * pCallBack, void * pCallBackArg, 
        evid *monixptr );
    friend int epicsShareAPI ca_clear_subscription ( evid pMon );
    friend int epicsShareAPI ca_flush_io ();
    friend int epicsShareAPI ca_sg_create ( CA_SYNC_GID * pgid );
    friend int epicsShareAPI ca_sg_delete ( const CA_SYNC_GID gid );
    friend int epicsShareAPI ca_sg_block ( const CA_SYNC_GID gid, ca_real timeout );
    friend int epicsShareAPI ca_sg_reset ( const CA_SYNC_GID gid );
    friend int epicsShareAPI ca_sg_test ( const CA_SYNC_GID gid );
    friend void epicsShareAPI caInstallDefaultService ( cacService & );
};

int fetchClientContext ( ca_client_context * * ppcac );

inline ca_client_context & oldChannelNotify::getClientCtx ()
{
    return this->cacCtx;
}

inline const char * oldChannelNotify::pName () const 
{
    return this->io.pName ();
}

inline void oldChannelNotify::show ( unsigned level ) const
{
    this->io.show ( level );
}

inline void oldChannelNotify::initiateConnect (
    epicsGuard < epicsMutex > & guard )
{
    this->io.initiateConnect ( guard );
}

inline void oldChannelNotify::ioCancel (    
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard, 
    const cacChannel::ioid & id )
{
    this->io.ioCancel ( cbGuard, guard, id );
}

inline void oldChannelNotify::ioShow ( 
    const cacChannel::ioid & id, unsigned level ) const
{
    this->io.ioShow ( id, level );
}

inline short oldChannelNotify::nativeType () const
{
    return this->io.nativeType ();
}

inline arrayElementCount oldChannelNotify::nativeElementCount () const
{
    return this->io.nativeElementCount ();
}

inline caAccessRights oldChannelNotify::accessRights () const
{
    return this->io.accessRights ();
}

inline unsigned oldChannelNotify::searchAttempts () const
{
    return this->io.searchAttempts ();
}

inline double oldChannelNotify::beaconPeriod () const
{
    return this->io.beaconPeriod ();
}

inline double oldChannelNotify::receiveWatchdogDelay () const
{
    return this->io.receiveWatchdogDelay ();
}

inline bool oldChannelNotify::ca_v42_ok () const
{
    return this->io.ca_v42_ok ();
}

inline bool oldChannelNotify::connected () const
{
    return this->currentlyConnected;
}

inline bool oldChannelNotify::previouslyConnected () const
{
    return this->prevConnected;
}

inline void oldChannelNotify::hostName ( char *pBuf, unsigned bufLength ) const
{
    this->io.hostName ( pBuf, bufLength );
}

inline const char * oldChannelNotify::pHostName () const
{
    return this->io.pHostName ();
}

inline void * oldChannelNotify::operator new ( size_t size, 
    tsFreeList < struct oldChannelNotify, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void oldChannelNotify::operator delete ( void *pCadaver, 
    tsFreeList < struct oldChannelNotify, 1024, epicsMutexNOOP > & freeList ) 
{
    freeList.release ( pCadaver );
}
#endif

inline oldSubscription::oldSubscription  (
    oldChannelNotify & chanIn, caEventCallBackFunc * pFuncIn, 
        void * pPrivateIn ) :
    chan ( chanIn ), id ( UINT_MAX ), pFunc ( pFuncIn ), 
        pPrivate ( pPrivateIn ), subscribed ( false )
{
}

inline void oldSubscription::begin (
    epicsGuard < epicsMutex > & guard, unsigned type, 
    arrayElementCount nElem, unsigned mask )
{
    this->subscribed = true;
    this->chan.subscribe ( guard, type, nElem, mask, *this, this->id );
    // dont touch this pointer after this point because the
    // 1st update callback might cancel the subscription
}

inline void * oldSubscription::operator new ( size_t size, 
    tsFreeList < struct oldSubscription, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void oldSubscription::operator delete ( void *pCadaver, 
    tsFreeList < struct oldSubscription, 1024, epicsMutexNOOP > & freeList ) 
{
    freeList.release ( pCadaver );
}
#endif

inline oldChannelNotify & oldSubscription::channel () const
{
    return this->chan;
}

inline int oldChannelNotify::changeConnCallBack ( caCh * pfunc )
{
    this->cacCtx.changeConnCallBack ( pfunc, 
        this->pConnCallBack, this->currentlyConnected );
    return ECA_NORMAL;
}

inline void * getCopy::operator new ( size_t size, 
    tsFreeList < class getCopy, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void getCopy::operator delete ( void *pCadaver, 
    tsFreeList < class getCopy, 1024, epicsMutexNOOP > & freeList ) 
{
    freeList.release ( pCadaver );
}
#endif

inline void * putCallback::operator new ( size_t size, 
    tsFreeList < class putCallback, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void putCallback::operator delete ( void * pCadaver, 
    tsFreeList < class putCallback, 1024, epicsMutexNOOP > & freeList ) 
{
    freeList.release ( pCadaver );
}
#endif

inline void * getCallback::operator new ( size_t size, 
    tsFreeList < class getCallback, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void getCallback::operator delete ( void * pCadaver, 
    tsFreeList < class getCallback, 1024, epicsMutexNOOP > & freeList ) 
{
    freeList.release ( pCadaver );
}
#endif

inline bool ca_client_context::preemptiveCallbakIsEnabled () const
{
    return ! this->pCallbackGuard.get ();
}

inline bool ca_client_context::ioComplete () const
{
    return ( this->pndRecvCnt == 0u );
}

inline unsigned ca_client_context::sequenceNumberOfOutstandingIO (
    epicsGuard < epicsMutex > & ) const
{
    // perhaps on SMP systems THERE should be lock/unlock around this
    return this->ioSeqNo;
}

#endif // ifndef oldAccessh
