
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

#include "tsFreeList.h"

#include "cac.h"

#define epicsExportSharedSymbols
#include "cacIO.h"
#include "cadef.h"
#undef epicsExportSharedSymbols

struct oldChannelNotify : public cacChannelNotify {
public:
    oldChannelNotify ( class oldCAC &, const char *pName, caCh *pConnCallBackIn, void *pPrivateIn );
    void destroy ();
    bool ioAttachOK ();
    void setPrivatePointer ( void * );
    void * privatePointer () const;
    int changeConnCallBack ( caCh *pfunc );
    int replaceAccessRightsEvent ( caArh *pfunc );

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );

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
protected:
    ~oldChannelNotify (); // must allocate from pool
private:
    cacChannel &io;
    oldCAC &cacCtx;
    caCh *pConnCallBack;
    void *pPrivate;
    caArh *pAccessRightsFunc;
    void connectNotify ();
    void disconnectNotify ();
    void accessRightsNotify ( const caAccessRights & );
    void exception ( int status, const char *pContext );
    void readException ( int status, const char *pContext,
        unsigned type, arrayElementCount count, void *pValue );
    void writeException ( int status, const char *pContext,
        unsigned type, arrayElementCount count );
    bool includeFirstConnectInCountOfOutstandingIO () const;
    static tsFreeList < struct oldChannelNotify, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class getCopy : public cacReadNotify {
public:
    getCopy ( oldCAC &cacCtx, oldChannelNotify &, unsigned type, 
        arrayElementCount count, void *pValue );
    void destroy ();
    void show ( unsigned level ) const;
    void cancel ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~getCopy (); // allocate only out of pool
private:
    arrayElementCount count;
    oldCAC &cacCtx;
    oldChannelNotify &chan;
    void *pValue;
    unsigned readSeq;
    unsigned type;
    void completion (
        unsigned type, arrayElementCount count, const void *pData);
    void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count );
    static tsFreeList < class getCopy, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class getCallback : public cacReadNotify {
public:
    getCallback ( oldChannelNotify &chanIn, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~getCallback (); // allocate only out of pool
private:
    oldChannelNotify &chan;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    void completion (
        unsigned type, arrayElementCount count, const void *pData);
    void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count );
    static tsFreeList < class getCallback, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class putCallback : public cacWriteNotify {
public:
    putCallback ( oldChannelNotify &, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~putCallback (); // allocate only out of pool
private:
    oldChannelNotify &chan;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    void completion ();
    void exception ( int status, const char *pContext, 
        unsigned type, arrayElementCount count );
    static tsFreeList < class putCallback, 1024 > freeList;
    static epicsMutex freeListMutex;
};

struct oldSubscription : public cacStateNotify {
public:
    oldSubscription ( 
        oldChannelNotify &,
        unsigned type, arrayElementCount nElem, unsigned mask, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    bool ioAttachOK ();
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    oldChannelNotify &channel () const;
protected:
    ~oldSubscription (); // must allocate from pool
private:
    oldChannelNotify &chan;
    cacChannel::ioid id;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    bool subscribed;
    void current (
        unsigned type, arrayElementCount count, const void *pData );
    void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count );
    static tsFreeList < struct oldSubscription, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class oldCAC : public cacNotify
{
public:
    oldCAC ( bool enablePreemptiveCallback = false, 
        unsigned maxNumberOfChannels = 32768 );
    virtual ~oldCAC ();
    void changeExceptionEvent ( caExceptionHandler *pfunc, void *arg );
    void registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg );
    void replaceErrLogHandler ( caPrintfFunc *ca_printf_func );
    void registerService ( cacService &service );
    cacChannel & createChannel ( const char *name_str, oldChannelNotify &chan );
    void flushRequest ();
    int pendIO ( const double &timeout );
    int pendEvent ( const double &timeout );
    bool ioComplete () const;
    void show ( unsigned level ) const;
    unsigned connectionCount () const;
    unsigned sequenceNumberOfOutstandingIO () const;
    void incrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned sequenceNo );
    void exception ( int status, const char *pContext, 
        const char *pFileName, unsigned lineNo );
    void exception ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo, oldChannelNotify &chan, 
        unsigned type, arrayElementCount count, unsigned op );
    CASG * lookupCASG ( unsigned id );
    void installCASG ( CASG & );
    void uninstallCASG ( CASG & );
    void enableCallbackPreemption ();
    void disableCallbackPreemption ();
// perhaps these should be eliminated in deference to the exception mechanism
    int printf ( const char *pformat, ... ) const;
    int vPrintf ( const char *pformat, va_list args ) const;
    void vSignal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, va_list args );
private:
    mutable epicsMutex mutex; 
    cac & clientCtx;
    caExceptionHandler *ca_exception_func;
    void *ca_exception_arg;
    caPrintfFunc *pVPrintfFunc;
    CAFDHANDLER *fdRegFunc;
    void *fdRegArg;
// this should probably be phased out (its not OS independent)
    void fdWasCreated ( int fd );
    void fdWasDestroyed ( int fd );
    void attachToClientCtx ();
};

int fetchClientContext ( oldCAC **ppcac );

inline bool oldChannelNotify::ioAttachOK ()
{
    return &this->io ? true : false;
}

inline void oldChannelNotify::destroy ()
{
    delete this;
}

inline const char *oldChannelNotify::pName () const 
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
                        cacReadNotify &notify, cacChannel::ioid *pId )
{
    this->io.read ( type, count, notify, pId );
}

inline void oldChannelNotify::write ( unsigned type, 
    arrayElementCount count, const void *pValue )
{
    this->io.write ( type, count, pValue );
}

inline void oldChannelNotify::write ( unsigned type, arrayElementCount count, 
                 const void *pValue, cacWriteNotify &notify, cacChannel::ioid *pId )
{
    this->io.write ( type, count, pValue, notify, pId );
}

inline void oldChannelNotify::subscribe ( unsigned type, 
    arrayElementCount count, unsigned mask, cacStateNotify &notify,
    cacChannel::ioid &idOut)
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
    return this->io.connected ();
}

inline bool oldChannelNotify::previouslyConnected () const
{
    return this->io.previouslyConnected ();
}

inline void oldChannelNotify::hostName ( char *pBuf, unsigned bufLength ) const
{
    this->io.hostName ( pBuf, bufLength );
}

inline const char * oldChannelNotify::pHostName () const
{
    return this->io.pHostName ();
}

inline oldSubscription::oldSubscription  (
        oldChannelNotify &chanIn,
        unsigned type, arrayElementCount nElem, unsigned mask, 
        caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    chan ( chanIn ), id ( 0 ), pFunc ( pFuncIn ), 
        pPrivate ( pPrivateIn ), subscribed ( false )
{
    this->chan.subscribe ( type, nElem, mask, *this, this->id );
    this->subscribed = true;
}

inline void oldSubscription::destroy ()
{
    if ( this->subscribed ) {
        this->chan.ioCancel ( this->id );
    }
    delete this;
}

inline void * oldSubscription::operator new ( size_t size )
{
    epicsAutoMutex locker ( oldSubscription::freeListMutex );
    return oldSubscription::freeList.allocate ( size );
}

inline void oldSubscription::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( oldSubscription::freeListMutex );
    oldSubscription::freeList.release ( pCadaver, size );
}

inline oldChannelNotify & oldSubscription::channel () const
{
    return this->chan;
}

inline void * getCopy::operator new ( size_t size )
{
    epicsAutoMutex locker ( getCopy::freeListMutex );
    return getCopy::freeList.allocate ( size );
}

inline void getCopy::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( getCopy::freeListMutex );
    getCopy::freeList.release ( pCadaver, size );
}

inline void putCallback::destroy ()
{
    delete this;
}

inline void * putCallback::operator new ( size_t size )
{
    epicsAutoMutex locker ( putCallback::freeListMutex );
    return putCallback::freeList.allocate ( size );
}

inline void putCallback::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( putCallback::freeListMutex );
    putCallback::freeList.release ( pCadaver, size );
}

inline void getCallback::destroy ()
{
    delete this;
}

inline void * getCallback::operator new ( size_t size )
{
    epicsAutoMutex locker ( getCallback::freeListMutex );
    return getCallback::freeList.allocate ( size );
}

inline void getCallback::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( getCallback::freeListMutex );
    getCallback::freeList.release ( pCadaver, size );
}

inline void oldCAC::registerService ( cacService &service )
{
    this->clientCtx.registerService ( service );
}

inline cacChannel & oldCAC::createChannel ( const char *name_str, oldChannelNotify &chan )
{
    return this->clientCtx.createChannel ( name_str, chan );
}

inline int oldCAC::pendIO ( const double &timeout )
{
    return this->clientCtx.pendIO ( timeout );
}

inline int oldCAC::pendEvent ( const double &timeout )
{
    return this->clientCtx.pendEvent ( timeout );
}

inline void oldCAC::flushRequest ()
{
    this->clientCtx.flushRequest ();
}

inline bool oldCAC::ioComplete () const
{
    return this->clientCtx.ioComplete ();
}

inline unsigned oldCAC::connectionCount () const
{
    return this->clientCtx.connectionCount ();
}

inline unsigned oldCAC::sequenceNumberOfOutstandingIO () const
{
    return this->clientCtx.sequenceNumberOfOutstandingIO ();
}

inline void oldCAC::incrementOutstandingIO ()
{
    this->clientCtx.incrementOutstandingIO ();
}

inline void oldCAC::decrementOutstandingIO ( unsigned sequenceNo )
{
    this->clientCtx.decrementOutstandingIO ( sequenceNo );
}

inline CASG * oldCAC::lookupCASG ( unsigned id )
{
    return this->clientCtx.lookupCASG ( id );
}

inline void oldCAC::installCASG ( CASG &sg )
{
    this->clientCtx.installCASG ( sg );
}

inline void oldCAC::uninstallCASG ( CASG &sg )
{
    this->clientCtx.uninstallCASG ( sg );
}

inline void oldCAC::enableCallbackPreemption ()
{
    this->clientCtx.enableCallbackPreemption ();
}

inline void oldCAC::disableCallbackPreemption ()
{
    this->clientCtx.disableCallbackPreemption ();
}

inline void oldCAC::vSignal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, va_list args )
{
    this->clientCtx.vSignal ( ca_status, pfilenm, 
                     lineno, pFormat, args );
}

#endif // ifndef oldAccessh
