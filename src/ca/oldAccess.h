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
#include "cxxCompilerDependencies.h"

#ifdef oldAccessh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "cac.h"
#include "cacIO.h"
#include "cadef.h"
#include "syncGroup.h"

struct oldChannelNotify : public cacChannelNotify {
public:
    oldChannelNotify ( struct ca_client_context &, const char * pName, 
        caCh * pConnCallBackIn, void * pPrivateIn, capri priority );
    ~oldChannelNotify ();
    void setPrivatePointer ( void * );
    void * privatePointer () const;
    int changeConnCallBack ( caCh *pfunc );
    int replaceAccessRightsEvent ( caArh *pfunc );
    const char *pName () const;
    void show ( unsigned level ) const;
    void initiateConnect ();
    void read ( 
        unsigned type, arrayElementCount count, 
        cacReadNotify &notify, cacChannel::ioid *pId = 0 );
    void read ( 
        unsigned type, arrayElementCount count, 
        void *pValue );
    void write ( 
        unsigned type, arrayElementCount count, 
        const void *pValue );
    void write ( 
        unsigned type, arrayElementCount count, const void *pValue, 
        cacWriteNotify &, cacChannel::ioid *pId = 0 );
    void subscribe ( 
        unsigned type, arrayElementCount count, unsigned mask, 
        cacStateNotify &, cacChannel::ioid & );
    void ioCancel ( const cacChannel::ioid & );
    void ioShow ( const cacChannel::ioid &, unsigned level ) const;
    short nativeType () const;
    arrayElementCount nativeElementCount () const;
    caAccessRights accessRights () const; // defaults to unrestricted access
    unsigned searchAttempts () const; // defaults to zero
    double beaconPeriod () const; // defaults to negative DBL_MAX
    bool ca_v42_ok () const; 
    bool connected () const; 
    bool previouslyConnected () const;
    void hostName ( char *pBuf, unsigned bufLength ) const; // defaults to local host name
    const char * pHostName () const; // deprecated - please do not use
    ca_client_context & getClientCtx ();
    void * operator new ( size_t size, 
        tsFreeList < struct oldChannelNotify, 1024 > & );
#   ifdef  CXX_PLACEMENT_DELETE
    void operator delete ( void * , 
        tsFreeList < struct oldChannelNotify, 1024 > & );
#   endif
private:
    ca_client_context & cacCtx;
    cacChannel & io;
    caCh * pConnCallBack;
    void * pPrivate;
    caArh * pAccessRightsFunc;
    unsigned ioSeqNo;
    bool currentlyConnected;
    bool prevConnected;
    void connectNotify ();
    void disconnectNotify ();
    void serviceShutdownNotify ();
    void accessRightsNotify ( const caAccessRights & );
    void exception ( int status, const char *pContext );
    void readException ( int status, const char *pContext,
        unsigned type, arrayElementCount count, void *pValue );
    void writeException ( int status, const char *pContext,
        unsigned type, arrayElementCount count );
	oldChannelNotify ( const oldChannelNotify & );
	oldChannelNotify & operator = ( const oldChannelNotify & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

class getCopy : public cacReadNotify {
public:
    getCopy ( ca_client_context &cacCtx, oldChannelNotify &, unsigned type, 
        arrayElementCount count, void *pValue );
    ~getCopy ();
    void show ( unsigned level ) const;
    void cancel ();
    void * operator new ( size_t size, 
        tsFreeList < class getCopy, 1024 > & );
#   ifdef  CXX_PLACEMENT_DELETE
    void operator delete ( void *, 
        tsFreeList < class getCopy, 1024 > & );
#   endif
private:
    arrayElementCount count;
    ca_client_context &cacCtx;
    oldChannelNotify &chan;
    void *pValue;
    unsigned ioSeqNo;
    unsigned type;
    void completion (
        unsigned type, arrayElementCount count, const void *pData);
    void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count );
	getCopy ( const getCopy & );
	getCopy & operator = ( const getCopy & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

class getCallback : public cacReadNotify {
public:
    getCallback ( oldChannelNotify &chanIn, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    ~getCallback (); 
    void destroy ();
    void * operator new ( size_t size, 
        tsFreeList < class getCallback, 1024 > & );
#   ifdef  CXX_PLACEMENT_DELETE
    void operator delete ( void *, 
        tsFreeList < class getCallback, 1024 > & );
#   endif
private:
    oldChannelNotify & chan;
    caEventCallBackFunc * pFunc;
    void * pPrivate;
    void completion (
        unsigned type, arrayElementCount count, const void *pData);
    void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count );
	getCallback ( const getCallback & );
	getCallback & operator = ( const getCallback & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

class putCallback : public cacWriteNotify {
public:
    putCallback ( oldChannelNotify &, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    ~putCallback ();
    void * operator new ( size_t size, 
        tsFreeList < class putCallback, 1024 > & );
#   ifdef  CXX_PLACEMENT_DELETE
    void operator delete ( void *, 
        tsFreeList < class putCallback, 1024 > & );
#   endif
private:
    oldChannelNotify & chan;
    caEventCallBackFunc * pFunc;
    void *pPrivate;
    void completion ();
    void exception ( int status, const char *pContext, 
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
    void begin ( unsigned type, arrayElementCount nElem, unsigned mask );
    oldChannelNotify & channel () const;
    void * operator new ( size_t size, 
        tsFreeList < struct oldSubscription, 1024 > & );
#   ifdef  CXX_PLACEMENT_DELETE
    void operator delete ( void *, 
        tsFreeList < struct oldSubscription, 1024 > & );
#   endif
    void ioCancel ();
private:
    oldChannelNotify & chan;
    cacChannel::ioid id;
    caEventCallBackFunc * pFunc;
    void * pPrivate;
    bool subscribed;
    void current (
        unsigned type, arrayElementCount count, const void *pData );
    void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count );
	oldSubscription ( const oldSubscription & );
	oldSubscription & operator = ( const oldSubscription & );
    void * operator new ( size_t size );
    void operator delete ( void * );
};

class ca_client_context_mutex {
public:
    void lock ();
    void unlock ();
    void show ( unsigned level ) const;
private:
    epicsMutex mutex;
};

struct ca_client_context : public cacNotify
{
public:
    ca_client_context ( bool enablePreemptiveCallback = false );
    virtual ~ca_client_context ();
    void changeExceptionEvent ( caExceptionHandler * pfunc, void * arg );
    void registerForFileDescriptorCallBack ( CAFDHANDLER * pFunc, void * pArg );
    void replaceErrLogHandler ( caPrintfFunc * ca_printf_func );
    void registerService ( cacService & service );
    cacChannel & createChannel ( const char * name_str, 
        oldChannelNotify & chan, cacChannel::priLev pri );
    void flushRequest ();
    int pendIO ( const double & timeout );
    int pendEvent ( const double & timeout );
    bool ioComplete () const;
    void show ( unsigned level ) const;
    unsigned connectionCount () const;
    unsigned sequenceNumberOfOutstandingIO () const;
    void incrementOutstandingIO ( unsigned ioSeqNo );
    void decrementOutstandingIO ( unsigned ioSeqNo );
    void exception ( int status, const char *pContext, 
        const char *pFileName, unsigned lineNo );
    void exception ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo, oldChannelNotify &chan, 
        unsigned type, arrayElementCount count, unsigned op );
    CASG * lookupCASG ( unsigned id );
    void installCASG ( CASG & );
    void uninstallCASG ( CASG & );
    void blockForEventAndEnableCallbacks ( epicsEvent & event, double timeout );
    void selfTest ();
// perhaps these should be eliminated in deference to the exception mechanism
    int printf ( const char *pformat, ... ) const;
    int vPrintf ( const char *pformat, va_list args ) const;
    void vSignal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, va_list args );
    bool preemptiveCallbakIsEnabled () const;
    epicsGuard < callbackMutex > callbackGuardFactory ();
    void destroyChannel ( oldChannelNotify & chan );
    void destroyGetCopy ( getCopy & );
    void destroyGetCallback ( getCallback & );
    void destroyPutCallback ( putCallback & );
    void destroySubscription ( oldSubscription & );
private:
    tsFreeList < struct oldChannelNotify, 1024 > oldChannelNotifyFreeList;
    tsFreeList < class getCopy, 1024 > getCopyFreeList;
    tsFreeList < class getCallback, 1024 > getCallbackFreeList;
    tsFreeList < class putCallback, 1024 > putCallbackFreeList;
    tsFreeList < struct oldSubscription, 1024 > subscriptionFreeList;
    tsFreeList < struct CASG, 128 > casgFreeList;
    mutable ca_client_context_mutex mutex; 
    epicsEvent ioDone;
    cac & clientCtx;
    epicsGuard < callbackMutex > * pCallbackGuard;
    caExceptionHandler * ca_exception_func;
    void * ca_exception_arg;
    caPrintfFunc * pVPrintfFunc;
    CAFDHANDLER * fdRegFunc;
    void  * fdRegArg;
    unsigned pndRecvCnt;
    unsigned ioSeqNo;
// this should probably be phased out (its not OS independent)
    void fdWasCreated ( int fd );
    void fdWasDestroyed ( int fd );
    void attachToClientCtx ();
	ca_client_context ( const ca_client_context & );
	ca_client_context & operator = ( const ca_client_context & );

    friend int epicsShareAPI ca_create_channel (
        const char * name_str, caCh * conn_func, void * puser,
        capri priority, chid * chanptr );
    friend int epicsShareAPI ca_array_get ( chtype type, 
            arrayElementCount count, chid pChan, void *pValue );
    friend int epicsShareAPI ca_array_get_callback ( chtype type, 
            arrayElementCount count, chid pChan,
            caEventCallBackFunc *pfunc, void *arg );
    friend int epicsShareAPI ca_array_put_callback ( chtype type, 
            arrayElementCount count, chid pChan, const void *pValue, 
            caEventCallBackFunc *pfunc, void *usrarg );
    friend int epicsShareAPI ca_add_masked_array_event ( 
            chtype type, arrayElementCount count, chid pChan, 
            caEventCallBackFunc *pCallBack, void *pCallBackArg, 
            ca_real, ca_real, ca_real, 
            evid *monixptr, long mask );
    friend int epicsShareAPI ca_sg_create ( CA_SYNC_GID * pgid );
    friend int epicsShareAPI ca_sg_delete ( const CA_SYNC_GID gid );
};

int fetchClientContext ( ca_client_context **ppcac );

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

inline void oldChannelNotify::initiateConnect ()
{
    this->io.initiateConnect ();
}

inline void oldChannelNotify::read ( unsigned type, arrayElementCount count, 
                        cacReadNotify &notify, cacChannel::ioid * pId )
{
    this->io.read ( type, count, notify, pId );
}

inline void oldChannelNotify::write ( unsigned type, 
    arrayElementCount count, const void * pValue )
{
    this->io.write ( type, count, pValue );
}

inline void oldChannelNotify::write ( unsigned type, arrayElementCount count, 
                 const void * pValue, cacWriteNotify & notify, cacChannel::ioid * pId )
{
    this->io.write ( type, count, pValue, notify, pId );
}

inline void oldChannelNotify::subscribe ( unsigned type, 
    arrayElementCount count, unsigned mask, cacStateNotify & notify,
    cacChannel::ioid & idOut)
{
    this->io.subscribe ( type, count, mask, notify, &idOut );
}

inline void oldChannelNotify::ioCancel ( const cacChannel::ioid &id )
{
    this->io.ioCancel ( id );
}

inline void oldChannelNotify::ioShow ( const cacChannel::ioid &id, unsigned level ) const
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
    tsFreeList < struct oldChannelNotify, 1024 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void oldChannelNotify::operator delete ( void *pCadaver, 
    tsFreeList < struct oldChannelNotify, 1024 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline oldSubscription::oldSubscription  (
        oldChannelNotify & chanIn, 
        caEventCallBackFunc * pFuncIn, void * pPrivateIn ) :
    chan ( chanIn ), id ( UINT_MAX ), pFunc ( pFuncIn ), 
        pPrivate ( pPrivateIn ), subscribed ( false )
{
}

inline void oldSubscription::begin  ( unsigned type, 
              arrayElementCount nElem, unsigned mask )
{
    this->subscribed = true;
    this->chan.subscribe ( type, nElem, mask, *this, this->id );
    // dont touch this pointer after this point because the
    // 1st update callback might cancel the subscription
}

inline void * oldSubscription::operator new ( size_t size, 
    tsFreeList < struct oldSubscription, 1024 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void oldSubscription::operator delete ( void *pCadaver, 
    tsFreeList < struct oldSubscription, 1024 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline oldChannelNotify & oldSubscription::channel () const
{
    return this->chan;
}

inline void * getCopy::operator new ( size_t size, 
    tsFreeList < class getCopy, 1024 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void getCopy::operator delete ( void *pCadaver, 
    tsFreeList < class getCopy, 1024 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline void * putCallback::operator new ( size_t size, 
    tsFreeList < class putCallback, 1024 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void putCallback::operator delete ( void * pCadaver, 
    tsFreeList < class putCallback, 1024 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline void getCallback::destroy ()
{
    delete this;
}

inline void * getCallback::operator new ( size_t size, 
    tsFreeList < class getCallback, 1024 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void getCallback::operator delete ( void *pCadaver, 
    tsFreeList < class getCallback, 1024 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline void ca_client_context::registerService ( cacService &service )
{
    this->clientCtx.registerService ( service );
}

inline cacChannel & ca_client_context::createChannel ( const char * name_str, 
             oldChannelNotify & chan, cacChannel::priLev pri )
{
    return this->clientCtx.createChannel ( name_str, chan, pri );
}

inline void ca_client_context::flushRequest ()
{
    this->clientCtx.flushRequest ();
}

inline unsigned ca_client_context::connectionCount () const
{
    return this->clientCtx.connectionCount ();
}

inline CASG * ca_client_context::lookupCASG ( unsigned id )
{
    return this->clientCtx.lookupCASG ( id );
}

inline void ca_client_context::installCASG ( CASG &sg )
{
    this->clientCtx.installCASG ( sg );
}

inline void ca_client_context::uninstallCASG ( CASG &sg )
{
    this->clientCtx.uninstallCASG ( sg );
}

inline void ca_client_context::vSignal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, va_list args )
{
    this->clientCtx.vSignal ( ca_status, pfilenm, 
                     lineno, pFormat, args );
}

inline void ca_client_context::selfTest ()
{
    this->clientCtx.selfTest ();
}

inline bool ca_client_context::preemptiveCallbakIsEnabled () const
{
    return this->clientCtx.preemptiveCallbakIsEnabled ();
}

inline bool ca_client_context::ioComplete () const
{
    return ( this->pndRecvCnt == 0u );
}

inline unsigned ca_client_context::sequenceNumberOfOutstandingIO () const
{
    // perhaps on SMP systems THERE should be lock/unlock around this
    return this->ioSeqNo;
}

inline epicsGuard < callbackMutex > ca_client_context::callbackGuardFactory ()
{
    return this->clientCtx.callbackGuardFactory ();
}

inline void ca_client_context_mutex::lock ()
{
    this->mutex.lock ();
}

inline void ca_client_context_mutex::unlock ()
{
    this->mutex.unlock ();
}

inline void ca_client_context_mutex::show ( unsigned level ) const
{
    this->mutex.show ( level );
}

#endif // ifndef oldAccessh
