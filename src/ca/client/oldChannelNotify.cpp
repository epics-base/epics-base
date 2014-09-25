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
 *  Copyright, 1986, The Regents of the University of California.
 *
 *
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <string>
#include <stdexcept>

#ifdef _MSC_VER
#   pragma warning(disable:4355)
#endif

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"
#include "autoPtrFreeList.h"

extern "C" void cacNoopAccesRightsHandler ( struct access_rights_handler_args )
{
}

oldChannelNotify::oldChannelNotify (
        epicsGuard < epicsMutex > & guard, ca_client_context & cacIn,
        const char *pName, caCh * pConnCallBackIn,
        void * pPrivateIn, capri priority ) :
    cacCtx ( cacIn ),
    io ( cacIn.createChannel ( guard, pName, *this, priority ) ),
    pConnCallBack ( pConnCallBackIn ),
    pPrivate ( pPrivateIn ), pAccessRightsFunc ( cacNoopAccesRightsHandler ),
    ioSeqNo ( 0 ), currentlyConnected ( false ), prevConnected ( false )
{
    guard.assertIdenticalMutex ( cacIn.mutexRef () );
    this->ioSeqNo = cacIn.sequenceNumberOfOutstandingIO ( guard );
    if ( pConnCallBackIn == 0 ) {
        cacIn.incrementOutstandingIO ( guard, this->ioSeqNo );
    }
}

oldChannelNotify::~oldChannelNotify ()
{
}

void  oldChannelNotify::destructor (
    CallbackGuard & cbGuard,
    epicsGuard < epicsMutex > & mutexGuard )
{
    mutexGuard.assertIdenticalMutex ( this->cacCtx.mutexRef () );
    this->io.destroy ( cbGuard, mutexGuard );
    // no need to worry about a connect preempting here because
    // the io (the nciu) has been destroyed above
    if ( this->pConnCallBack == 0 && ! this->currentlyConnected ) {
        this->cacCtx.decrementOutstandingIO ( mutexGuard, this->ioSeqNo );
    }
    this->~oldChannelNotify ();
}

void oldChannelNotify::connectNotify (
    epicsGuard < epicsMutex > & guard )
{
    this->currentlyConnected = true;
    this->prevConnected = true;
    if ( this->pConnCallBack ) {
        struct connection_handler_args  args;
        args.chid = this;
        args.op = CA_OP_CONN_UP;
        caCh * pFunc = this->pConnCallBack;
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            ( *pFunc ) ( args );
        }
    }
    else {
        this->cacCtx.decrementOutstandingIO ( guard, this->ioSeqNo );
    }
}

void oldChannelNotify::disconnectNotify (
    epicsGuard < epicsMutex > & guard )
{
    this->currentlyConnected = false;
    if ( this->pConnCallBack ) {
        struct connection_handler_args args;
        args.chid = this;
        args.op = CA_OP_CONN_DOWN;
        caCh * pFunc = this->pConnCallBack;
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            ( *pFunc ) ( args );
        }
    }
    else {
        this->cacCtx.incrementOutstandingIO (
            guard, this->ioSeqNo );
    }
}

void oldChannelNotify::serviceShutdownNotify (
    epicsGuard < epicsMutex > & guard )
{
    this->disconnectNotify ( guard );
}

void oldChannelNotify::accessRightsNotify (
    epicsGuard < epicsMutex > & guard, const caAccessRights & ar )
{
    struct access_rights_handler_args args;
    args.chid = this;
    args.ar.read_access = ar.readPermit();
    args.ar.write_access = ar.writePermit();
    caArh * pFunc = this->pAccessRightsFunc;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        ( *pFunc ) ( args );
    }
}

void oldChannelNotify::exception (
    epicsGuard < epicsMutex > & guard, int status, const char * pContext )
{
    this->cacCtx.exception ( guard, status, pContext, __FILE__, __LINE__ );
}

void oldChannelNotify::readException (
    epicsGuard < epicsMutex > & guard, int status, const char *pContext,
    unsigned type, arrayElementCount count, void * /* pValue */ )
{
    this->cacCtx.exception ( guard, status, pContext,
        __FILE__, __LINE__, *this, type, count, CA_OP_GET );
}

void oldChannelNotify::writeException (
    epicsGuard < epicsMutex > & guard, int status, const char *pContext,
    unsigned type, arrayElementCount count )
{
    this->cacCtx.exception ( guard, status, pContext,
        __FILE__, __LINE__, *this, type, count, CA_OP_PUT );
}

void oldChannelNotify::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

/*
 * ca_get_host_name ()
 */
unsigned epicsShareAPI ca_get_host_name (
    chid pChan, char * pBuf, unsigned bufLength )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef() );
    return pChan->io.getHostName ( guard, pBuf, bufLength );
}

/*
 * ca_host_name ()
 *
 * !!!! not thread safe !!!!
 *
 */
const char * epicsShareAPI ca_host_name (
    chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.pHostName ( guard );
}

/*
 * ca_set_puser ()
 */
void epicsShareAPI ca_set_puser (
    chid pChan, void * puser )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    pChan->pPrivate = puser;
}

/*
 * ca_get_puser ()
 */
void * epicsShareAPI ca_puser (
    chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->pPrivate;
}

/*
 *  Specify an event subroutine to be run for connection events
 */
int epicsShareAPI ca_change_connection_event ( chid pChan, caCh * pfunc )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    if ( ! pChan->currentlyConnected ) {
         if ( pfunc ) {
            if ( ! pChan->pConnCallBack ) {
                pChan->cacCtx.decrementOutstandingIO ( guard, pChan->ioSeqNo );
            }
        }
        else {
            if ( pChan->pConnCallBack ) {
                pChan->cacCtx.incrementOutstandingIO ( guard, pChan->ioSeqNo );
            }
        }
    }
    pChan->pConnCallBack = pfunc;
    return ECA_NORMAL;
}

/*
 * ca_replace_access_rights_event
 */
int epicsShareAPI ca_replace_access_rights_event (
    chid pChan, caArh *pfunc )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );

    // The order of the following is significant to guarantee that the
    // access rights handler is always gets called even if the channel connects
    // while this is running. There is some very small chance that the
    // handler could be called twice here with the same access rights state, but
    // that will not upset the application.
    pChan->pAccessRightsFunc = pfunc ? pfunc : cacNoopAccesRightsHandler;
    caAccessRights tmp = pChan->io.accessRights ( guard );

    if ( pChan->currentlyConnected ) {
        struct access_rights_handler_args args;
        args.chid = pChan;
        args.ar.read_access = tmp.readPermit ();
        args.ar.write_access = tmp.writePermit ();
        epicsGuardRelease < epicsMutex > unguard ( guard );
        ( *pChan->pAccessRightsFunc ) ( args );
    }
    return ECA_NORMAL;
}

/*
 * ca_array_get ()
 */
int epicsShareAPI ca_array_get ( chtype type,
            arrayElementCount count, chid pChan, void *pValue )
{
    int caStatus;
    try {
        if ( type < 0 ) {
            return ECA_BADTYPE;
        }
        if ( count == 0 )
            return ECA_BADCOUNT;

        unsigned tmpType = static_cast < unsigned > ( type );
        epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
        pChan->eliminateExcessiveSendBacklog ( guard );
        autoPtrFreeList < getCopy, 0x400, epicsMutexNOOP > pNotify
            ( pChan->getClientCtx().getCopyFreeList,
                new ( pChan->getClientCtx().getCopyFreeList )
                    getCopy ( guard, pChan->getClientCtx(), *pChan,
                                tmpType, count, pValue ) );
        pChan->io.read ( guard, type, count, *pNotify, 0 );
        pNotify.release ();
        caStatus = ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        caStatus = ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        caStatus = ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        caStatus = ECA_BADCOUNT;
    }
    catch ( cacChannel::noReadAccess & )
    {
        caStatus = ECA_NORDACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        caStatus = ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        caStatus = ECA_UNAVAILINSERV;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        caStatus = ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        caStatus = ECA_ALLOCMEM;
    }
    catch ( cacChannel::msgBodyCacheTooSmall & ) {
        caStatus = ECA_TOLARGE;
    }
    catch ( ... )
    {
        caStatus = ECA_GETFAIL;
    }
    return caStatus;
}

/*
 * ca_array_get_callback ()
 */
int epicsShareAPI ca_array_get_callback ( chtype type,
            arrayElementCount count, chid pChan,
            caEventCallBackFunc *pfunc, void *arg )
{
    int caStatus;
    try {
        if ( type < 0 ) {
            return ECA_BADTYPE;
        }
        if ( pfunc == NULL ) {
            return ECA_BADFUNCPTR;
        }
        unsigned tmpType = static_cast < unsigned > ( type );

        epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
        pChan->eliminateExcessiveSendBacklog ( guard );
        autoPtrFreeList < getCallback, 0x400, epicsMutexNOOP > pNotify
            ( pChan->getClientCtx().getCallbackFreeList,
            new ( pChan->getClientCtx().getCallbackFreeList )
                getCallback ( *pChan, pfunc, arg ) );
        pChan->io.read ( guard, tmpType, count, *pNotify, 0 );
        pNotify.release ();
        caStatus = ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        caStatus = ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        caStatus = ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        caStatus = ECA_BADCOUNT;
    }
    catch ( cacChannel::noReadAccess & )
    {
        caStatus = ECA_NORDACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        caStatus = ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        caStatus = ECA_UNAVAILINSERV;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        caStatus = ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        caStatus = ECA_ALLOCMEM;
    }
    catch ( cacChannel::msgBodyCacheTooSmall ) {
        caStatus = ECA_TOLARGE;
    }
    catch ( ... )
    {
        caStatus = ECA_GETFAIL;
    }
    return caStatus;
}

void oldChannelNotify::read (
    epicsGuard < epicsMutex > & guard,
    unsigned type, arrayElementCount count,
    cacReadNotify & notify, cacChannel::ioid * pId )
{
    this->io.read ( guard, type, count, notify, pId );
}

/*
 *  ca_array_put_callback ()
 */
int epicsShareAPI ca_array_put_callback ( chtype type, arrayElementCount count,
    chid pChan, const void *pValue, caEventCallBackFunc *pfunc, void *usrarg )
{
    int caStatus;
    try {
        if ( type < 0 ) {
            return ECA_BADTYPE;
        }
        if ( pfunc == NULL ) {
            return ECA_BADFUNCPTR;
        }
        epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
        pChan->eliminateExcessiveSendBacklog ( guard );
        unsigned tmpType = static_cast < unsigned > ( type );
        autoPtrFreeList < putCallback, 0x400, epicsMutexNOOP > pNotify
                ( pChan->getClientCtx().putCallbackFreeList,
                    new ( pChan->getClientCtx().putCallbackFreeList )
                        putCallback ( *pChan, pfunc, usrarg ) );
        pChan->io.write ( guard, tmpType, count, pValue, *pNotify, 0 );
        pNotify.release ();
        caStatus = ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        caStatus = ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        caStatus = ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        caStatus = ECA_BADCOUNT;
    }
    catch ( cacChannel::noWriteAccess & )
    {
        caStatus = ECA_NOWTACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        caStatus = ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        caStatus = ECA_UNAVAILINSERV;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        caStatus = ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        caStatus = ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        caStatus = ECA_PUTFAIL;
    }
    return caStatus;
}

/*
 *  ca_array_put ()
 */
int epicsShareAPI ca_array_put ( chtype type, arrayElementCount count,
                                chid pChan, const void * pValue )
{
    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    int caStatus;
    try {
        epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
        pChan->eliminateExcessiveSendBacklog ( guard );
        pChan->io.write ( guard, tmpType, count, pValue );
        caStatus = ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        caStatus = ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        caStatus = ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        caStatus = ECA_BADCOUNT;
    }
    catch ( cacChannel::noWriteAccess & )
    {
        caStatus = ECA_NOWTACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        caStatus = ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        caStatus = ECA_UNAVAILINSERV;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        caStatus = ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        caStatus = ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        caStatus = ECA_PUTFAIL;
    }
    return caStatus;
}

int epicsShareAPI ca_create_subscription (
        chtype type, arrayElementCount count, chid pChan,
        long mask, caEventCallBackFunc * pCallBack, void * pCallBackArg,
        evid * monixptr )
{
    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    if ( INVALID_DB_REQ (type) ) {
        return ECA_BADTYPE;
    }

    if ( pCallBack == NULL ) {
        return ECA_BADFUNCPTR;
    }

    static const long maskMask = 0xffff;
    if ( ( mask & maskMask ) == 0) {
        return ECA_BADMASK;
    }

    if ( mask & ~maskMask ) {
        return ECA_BADMASK;
    }

    try {
        epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
        try {
            // if this stalls out on a live circuit then an exception
            // can be forthcoming which we must ignore (this is a
            // special case preserving legacy ca_create_subscription
            // behavior)
            pChan->eliminateExcessiveSendBacklog ( guard );
        }
        catch ( cacChannel::notConnected & ) {
            // intentionally ignored (its ok to subscribe when not connected)
        }
        new ( pChan->getClientCtx().subscriptionFreeList )
            oldSubscription  (
                guard, *pChan, pChan->io, tmpType, count, mask,
                pCallBack, pCallBackArg, monixptr );
        // dont touch object created after above new because
        // the first callback might have canceled, and therefore
        // destroyed, it
        return ECA_NORMAL;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::badEventSelection & )
    {
        return ECA_BADMASK;
    }
    catch ( cacChannel::noReadAccess & )
    {
        return ECA_NORDACCESS;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_UNAVAILINSERV;
    }
    catch ( std::bad_alloc & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( cacChannel::msgBodyCacheTooSmall & ) {
        return ECA_TOLARGE;
    }
    catch ( ... )
    {
        return ECA_INTERNAL;
    }
}

void oldChannelNotify::write (
    epicsGuard < epicsMutex > & guard, unsigned type, arrayElementCount count,
    const void * pValue, cacWriteNotify & notify, cacChannel::ioid * pId )
{
    this->io.write ( guard, type, count, pValue, notify, pId );
}

/*
 * ca_field_type()
 */
short epicsShareAPI ca_field_type ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.nativeType ( guard );
}

/*
 * ca_element_count ()
 */
arrayElementCount epicsShareAPI ca_element_count ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.nativeElementCount ( guard );
}

/*
 * ca_state ()
 */
enum channel_state epicsShareAPI ca_state ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    if ( pChan->io.connected ( guard ) ) {
        return cs_conn;
    }
    else if ( pChan->prevConnected ){
        return cs_prev_conn;
    }
    else {
        return cs_never_conn;
    }
}

/*
 * ca_read_access ()
 */
unsigned epicsShareAPI ca_read_access ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.accessRights(guard).readPermit();
}

/*
 * ca_write_access ()
 */
unsigned epicsShareAPI ca_write_access ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.accessRights(guard).writePermit();
}

/*
 * ca_name ()
 */
const char * epicsShareAPI ca_name ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.pName ( guard );
}

unsigned epicsShareAPI ca_search_attempts ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.searchAttempts ( guard );
}

double epicsShareAPI ca_beacon_period ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.beaconPeriod ( guard );
}

double epicsShareAPI ca_receive_watchdog_delay ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.receiveWatchdogDelay ( guard );
}

/*
 * ca_v42_ok(chid chan)
 */
int epicsShareAPI ca_v42_ok ( chid pChan )
{
    epicsGuard < epicsMutex > guard ( pChan->cacCtx.mutexRef () );
    return pChan->io.ca_v42_ok ( guard );
}


