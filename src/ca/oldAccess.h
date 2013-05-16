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

struct oldChannelNotify : private cacChannelNotify {
public:
    oldChannelNotify ( 
        epicsGuard < epicsMutex > &, struct ca_client_context &, 
        const char * pName, caCh * pConnCallBackIn, 
        void * pPrivateIn, capri priority );
    void destructor ( 
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & mutexGuard );

    // legacy C API
    friend unsigned epicsShareAPI ca_get_host_name ( 
        chid pChan, char * pBuf, unsigned bufLength );
    friend const char * epicsShareAPI ca_host_name ( 
        chid pChan );
    friend const char * epicsShareAPI ca_name ( 
        chid pChan );
    friend void epicsShareAPI ca_set_puser ( 
        chid pChan, void * puser );
    friend void * epicsShareAPI ca_puser ( 
        chid pChan );
    friend int epicsShareAPI ca_change_connection_event ( 
        chid pChan, caCh * pfunc );
    friend int epicsShareAPI ca_replace_access_rights_event ( 
        chid pChan, caArh *pfunc );
    friend int epicsShareAPI ca_array_get ( chtype type, 
        arrayElementCount count, chid pChan, void * pValue );
    friend int epicsShareAPI ca_array_get_callback ( chtype type, 
        arrayElementCount count, chid pChan,
        caEventCallBackFunc *pfunc, void *arg );
    friend int epicsShareAPI ca_array_put ( 
        chtype type, arrayElementCount count, 
        chid pChan, const void * pValue );
    friend int epicsShareAPI ca_array_put_callback ( 
        chtype type, arrayElementCount count, 
        chid pChan, const void *pValue, 
        caEventCallBackFunc *pfunc, void *usrarg );
    friend double epicsShareAPI ca_beacon_period ( 
        chid pChan );
    friend unsigned epicsShareAPI ca_search_attempts ( 
        chid pChan );
    friend unsigned epicsShareAPI ca_write_access ( 
        chid pChan );
    friend unsigned epicsShareAPI ca_read_access ( 
        chid pChan );
    friend short epicsShareAPI ca_field_type ( 
        chid pChan );
    friend arrayElementCount epicsShareAPI ca_element_count ( 
        chid pChan );
    friend int epicsShareAPI ca_v42_ok ( 
        chid pChan );
    friend int epicsShareAPI ca_create_subscription ( 
        chtype type, arrayElementCount count, chid pChan, 
        long mask, caEventCallBackFunc * pCallBack, 
        void * pCallBackArg, evid * monixptr );
    friend enum channel_state epicsShareAPI ca_state ( 
        chid pChan ); 
    friend double epicsShareAPI ca_receive_watchdog_delay ( 
        chid pChan );

    unsigned getName (
        epicsGuard < epicsMutex > &,
        char * pBuf, unsigned bufLen ) const throw ();
    void show ( 
        epicsGuard < epicsMutex > &,
        unsigned level ) const;
    void initiateConnect (
        epicsGuard < epicsMutex > & );
    void read ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, 
        cacReadNotify &notify, cacChannel::ioid *pId = 0 );
    void write ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, const void *pValue, 
        cacWriteNotify &, cacChannel::ioid *pId = 0 );
    void ioCancel ( 
        CallbackGuard & callbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard, 
        const cacChannel::ioid & );
    void ioShow ( 
        epicsGuard < epicsMutex > & guard,
        const cacChannel::ioid &, unsigned level ) const;
    ca_client_context & getClientCtx ();
    void eliminateExcessiveSendBacklog ( 
        epicsGuard < epicsMutex > & );

    void * operator new ( size_t size, 
        tsFreeList < struct oldChannelNotify, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void * , 
        tsFreeList < struct oldChannelNotify, 1024, epicsMutexNOOP > & ))
protected:
    ~oldChannelNotify ();
private:
    ca_client_context & cacCtx;
    cacChannel & io;
    caCh * pConnCallBack;
    void * pPrivate;
    caArh * pAccessRightsFunc;
    unsigned ioSeqNo;
    bool currentlyConnected;
    bool prevConnected;
    void connectNotify ( epicsGuard < epicsMutex > & );
    void disconnectNotify ( epicsGuard < epicsMutex > & );
    void serviceShutdownNotify (
        epicsGuard < epicsMutex > & mutualExclusionGuard );
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
    void operator delete ( void * );
};

struct oldSubscription : private cacStateNotify {
public:
    oldSubscription (
        epicsGuard < epicsMutex > & guard,
        oldChannelNotify & chanIn, cacChannel & io, 
        unsigned type, arrayElementCount nElem, unsigned mask,
        caEventCallBackFunc * pFuncIn, void * pPrivateIn,
        evid * );
    ~oldSubscription ();
    oldChannelNotify & channel () const;
    // The primary mutex must be released when calling the user's
    // callback, and therefore a finite interval exists when we are
    // moving forward with the intent to call the users callback
    // but the users IO could be deleted during this interval.
    // To prevent the user's callback from being called after
    // destroying his IO we must past a guard for the callback
    // mutex here.
    void cancel ( 
        CallbackGuard & callbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void * operator new ( size_t size, 
        tsFreeList < struct oldSubscription, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < struct oldSubscription, 1024, epicsMutexNOOP > & ))
private:
    oldChannelNotify & chan;
    cacChannel::ioid id;
    caEventCallBackFunc * pFunc;
    void * pPrivate;
    void current (
        epicsGuard < epicsMutex > &, unsigned type, 
        arrayElementCount count, const void *pData );
    void exception ( 
        epicsGuard < epicsMutex > &, int status, 
        const char *pContext, unsigned type, arrayElementCount count );
	oldSubscription ( const oldSubscription & );
	oldSubscription & operator = ( const oldSubscription & );
    void operator delete ( void * );
};

extern "C" void cacOnceFunc ( void * );
extern "C" void cacExitHandler ( void *);

struct ca_client_context : public cacContextNotify
{
public:
    ca_client_context ( bool enablePreemptiveCallback = false );
    virtual ~ca_client_context ();
    void changeExceptionEvent ( 
        caExceptionHandler * pfunc, void * arg );
    void registerForFileDescriptorCallBack ( 
        CAFDHANDLER * pFunc, void * pArg );
    void replaceErrLogHandler ( caPrintfFunc * ca_printf_func );
    cacChannel & createChannel ( 
        epicsGuard < epicsMutex > &, const char * pChannelName, 
        cacChannelNotify &, cacChannel::priLev pri );
    void flush ( epicsGuard < epicsMutex > & );
    void eliminateExcessiveSendBacklog ( 
        epicsGuard < epicsMutex > &, cacChannel & );
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
    static void installDefaultService ( cacService & );
    void installCASG ( epicsGuard < epicsMutex > &, CASG & );
    void uninstallCASG ( epicsGuard < epicsMutex > &, CASG & );
    void selfTest () const;
// perhaps these should be eliminated in deference to the exception mechanism
    int printFormated ( const char * pformat, ... ) const;
    int varArgsPrintFormated ( const char * pformat, va_list args ) const;
    void signal ( int ca_status, const char * pfilenm, 
                     int lineno, const char * pFormat, ... );
    void vSignal ( int ca_status, const char * pfilenm, 
                     int lineno, const char  *pFormat, va_list args );
    bool preemptiveCallbakIsEnabled () const;
    void destroyGetCopy ( epicsGuard < epicsMutex > &, getCopy & );
    void destroyGetCallback ( epicsGuard < epicsMutex > &, getCallback & );
    void destroyPutCallback ( epicsGuard < epicsMutex > &, putCallback & );
    void destroySubscription ( epicsGuard < epicsMutex > &, oldSubscription & );
    epicsMutex & mutexRef () const;

    template < class T >
    void whenThereIsAnExceptionDestroySyncGroupIO ( epicsGuard < epicsMutex > &, T & );

    // legacy C API    
    friend int epicsShareAPI ca_create_channel (
        const char * name_str, caCh * conn_func, void * puser,
        capri priority, chid * chanptr );
    friend int epicsShareAPI ca_clear_channel ( chid pChan );
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
    friend int epicsShareAPI ca_flush_io ();
    friend int epicsShareAPI ca_clear_subscription ( evid pMon );
    friend int epicsShareAPI ca_sg_create ( CA_SYNC_GID * pgid );
    friend int epicsShareAPI ca_sg_delete ( const CA_SYNC_GID gid );
    friend int epicsShareAPI ca_sg_block ( const CA_SYNC_GID gid, ca_real timeout );
    friend int epicsShareAPI ca_sg_reset ( const CA_SYNC_GID gid );
    friend int epicsShareAPI ca_sg_test ( const CA_SYNC_GID gid );
    friend int epicsShareAPI ca_sg_array_get ( const CA_SYNC_GID gid,
                              chtype type, arrayElementCount count,
                              chid pChan, void *pValue );
    friend int epicsShareAPI ca_sg_array_put ( const CA_SYNC_GID gid,
                              chtype type, arrayElementCount count,
                              chid pChan, const void *pValue );
    friend int ca_sync_group_destroy ( CallbackGuard & cbGuard,
                                 epicsGuard < epicsMutex > & guard,
                                ca_client_context & cac, const CA_SYNC_GID gid );
    friend void sync_group_reset ( ca_client_context & client, 
                                                  CASG & sg );

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
    epicsThreadId createdByThread;
    epics_auto_ptr < CallbackGuard > pCallbackGuard;
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
    void _sendWakeupMsg ();

	ca_client_context ( const ca_client_context & );
	ca_client_context & operator = ( const ca_client_context & );
	
	friend void cacOnceFunc ( void * );
	friend void cacExitHandler ( void *);
    static cacService * pDefaultService;
    static epicsMutex * pDefaultServiceInstallMutex;
    static const unsigned flushBlockThreshold;
};

int fetchClientContext ( ca_client_context * * ppcac );

inline ca_client_context & oldChannelNotify::getClientCtx ()
{
    return this->cacCtx;
}

inline unsigned oldChannelNotify::getName (
    epicsGuard < epicsMutex > & guard,
    char * pBuf, unsigned bufLen ) const throw ()
{
    return this->io.getName ( guard, pBuf, bufLen );
}

inline void oldChannelNotify::show ( 
    epicsGuard < epicsMutex > & guard,
    unsigned level ) const
{
    this->io.show ( guard, level );
}

inline void oldChannelNotify::initiateConnect (
    epicsGuard < epicsMutex > & guard )
{
    this->io.initiateConnect ( guard );
}

inline void oldChannelNotify::ioCancel (    
    CallbackGuard & callbackGuard,
    epicsGuard < epicsMutex > & mutualExclusionGuard,
    const cacChannel::ioid & id )
{
    this->io.ioCancel ( callbackGuard, mutualExclusionGuard, id );
}

inline void oldChannelNotify::ioShow ( 
    epicsGuard < epicsMutex > & guard,
    const cacChannel::ioid & id, unsigned level ) const
{
    this->io.ioShow ( guard, id, level );
}

inline void oldChannelNotify::eliminateExcessiveSendBacklog ( 
    epicsGuard < epicsMutex > & guard )
{
    this->cacCtx.eliminateExcessiveSendBacklog ( guard, this->io );
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

inline void oldSubscription::cancel ( 
    CallbackGuard & callbackGuard,
    epicsGuard < epicsMutex > & mutualExclusionGuard )
{
    this->chan.ioCancel ( callbackGuard, mutualExclusionGuard, this->id );
}

inline oldChannelNotify & oldSubscription::channel () const
{
    return this->chan;
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
    return this->pCallbackGuard.get () == 0;
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

template < class T >
void ca_client_context :: whenThereIsAnExceptionDestroySyncGroupIO ( 
                            epicsGuard < epicsMutex > & guard, T & io )
{
    if ( this->pCallbackGuard.get() &&
        this->createdByThread == epicsThreadGetIdSelf () ) {
        io.destroy ( *this->pCallbackGuard.get(), guard );
    }
    else {
        // dont reverse the lock hierarchy
        epicsGuardRelease < epicsMutex > guardRelease ();
        {
            //
            // we will definately stall out here if all of the
            // following are true
            //
            // o user creates non-preemtive mode client library context
            // o user doesnt periodically call a ca function
            // o user calls this function from an auxiillary thread
            //
            CallbackGuard cbGuard ( this->cbMutex );
            epicsGuard < epicsMutex > guard ( this->mutex );
            io.destroy ( cbGuard, guard );
        }
    }
}
        
#endif // ifndef oldAccessh
