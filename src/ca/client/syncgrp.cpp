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
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "syncGroup.h"

/*
 * ca_sg_create()
 */
extern "C" int epicsShareAPI ca_sg_create ( CA_SYNC_GID * pgid )
{
    ca_client_context * pcac;
    int caStatus;
    CASG * pcasg;

    caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    try {
        epicsGuard < epicsMutex > guard ( pcac->mutexRef() );
        pcasg = new ( pcac->casgFreeList ) CASG ( guard, *pcac );
        *pgid = pcasg->getId ();
        return ECA_NORMAL;
    }
    catch ( std::bad_alloc & ) {
        return ECA_ALLOCMEM;
    }
    catch ( ... ) {
        return ECA_INTERNAL;
    }
}

int ca_sync_group_destroy ( CallbackGuard & cbGuard, epicsGuard < epicsMutex > & guard,
                                ca_client_context & cac, const CA_SYNC_GID gid )
{
    int caStatus;
    CASG * pcasg = cac.lookupCASG ( guard, gid );
    if ( pcasg ) {
        pcasg->destructor ( cbGuard, guard );
        cac.casgFreeList.release ( pcasg );
        caStatus = ECA_NORMAL;
    }
    else {
        caStatus = ECA_BADSYNCGRP;
    }
    return caStatus;
}

/*
 * ca_sg_delete()
 */
extern "C" int epicsShareAPI ca_sg_delete ( const CA_SYNC_GID gid )
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext ( & pcac );
    if ( caStatus == ECA_NORMAL ) {
        if ( pcac->pCallbackGuard.get() &&
            pcac->createdByThread == epicsThreadGetIdSelf () ) {
          epicsGuard < epicsMutex > guard ( pcac->mutex );
          caStatus = ca_sync_group_destroy ( *pcac->pCallbackGuard.get(),
                                          guard, *pcac, gid );
        }
        else {
          //
          // we will definately stall out here if all of the
          // following are true
          //
          // o user creates non-preemtive mode client library context
          // o user doesnt periodically call a ca function
          // o user calls this function from an auxiillary thread
          //
          CallbackGuard cbGuard ( pcac->cbMutex );
          epicsGuard < epicsMutex > guard ( pcac->mutex );
          caStatus = ca_sync_group_destroy ( cbGuard, guard, *pcac, gid );
        }
    }
    return caStatus;
}

void sync_group_reset ( ca_client_context & client, CASG & sg )
{
    if ( client.pCallbackGuard.get() &&
        client.createdByThread == epicsThreadGetIdSelf () ) {
        epicsGuard < epicsMutex > guard ( client.mutex );
        sg.reset ( *client.pCallbackGuard.get(), guard );
    }
    else {  
        //
        // we will definately stall out here if all of the
        // following are true
        //
        // o user creates non-preemtive mode client library context
        // o user doesnt periodically call a ca function
        // o user calls this function from an auxiillary thread
        //
        CallbackGuard cbGuard ( client.cbMutex );
        epicsGuard < epicsMutex > guard ( client.mutex );
        sg.reset ( cbGuard, guard );
    }
}

//
// ca_sg_block ()
//
// !!!! This routine is only visible in the old interface - or in a new ST interface.
// !!!! In the old interface we restrict thread attach so that calls from threads
// !!!! other than the initializing thread are not allowed if preemptive callback
// !!!! is disabled. This prevents the preemptive callback lock from being released
// !!!! by other threads than the one that locked it.
//
extern "C" int epicsShareAPI ca_sg_block (
    const CA_SYNC_GID gid, ca_real timeout )
{
    ca_client_context *pcac;
    int status = fetchClientContext ( &pcac );
    if ( status == ECA_NORMAL ) {
        CASG * pcasg;
        {
            epicsGuard < epicsMutex > guard ( pcac->mutex );
            pcasg = pcac->lookupCASG ( guard, gid );
            if ( pcasg ) {
                status = pcasg->block (
                    pcac->pCallbackGuard.get (), guard, timeout );
            }
            else {
                status = ECA_BADSYNCGRP;
            }
        }
        if ( pcasg ) {
            sync_group_reset ( *pcac, *pcasg );
        }
    }
    return status;
}

/*
 * ca_sg_reset
 */
extern "C" int epicsShareAPI ca_sg_reset ( const CA_SYNC_GID gid )
{
    ca_client_context *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus == ECA_NORMAL ) {
        CASG * pcasg;
        {
            epicsGuard < epicsMutex > guard ( pcac->mutex );
            pcasg = pcac->lookupCASG ( guard, gid );
        }
        if ( pcasg ) {
            sync_group_reset ( *pcac, *pcasg );
            caStatus = ECA_NORMAL;
        }
        else {
            caStatus = ECA_BADSYNCGRP;
        }
    }
    return caStatus;
}

/*
 * ca_sg_stat
 */
extern "C" int epicsShareAPI ca_sg_stat ( const CA_SYNC_GID gid )
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    epicsGuard < epicsMutex > guard ( pcac->mutexRef() );

    CASG * pcasg = pcac->lookupCASG ( guard, gid );
    if ( ! pcasg ) {
        ::printf ( "Bad Sync Group Id\n");
        return ECA_BADSYNCGRP;
    }
    pcasg->show ( guard, 1000u );

    return ECA_NORMAL;
}

/*
 * ca_sg_test
 */
extern "C" int epicsShareAPI ca_sg_test ( const CA_SYNC_GID gid )
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus == ECA_NORMAL ) {
        epicsGuard < epicsMutex > guard ( pcac->mutexRef() );
        CASG * pcasg = pcac->lookupCASG ( guard, gid );
        if ( pcasg ) {
            bool isComplete;
            if ( pcac->pCallbackGuard.get() &&
                pcac->createdByThread == epicsThreadGetIdSelf () ) {
              epicsGuard < epicsMutex > guard ( pcac->mutex );
              isComplete = pcasg->ioComplete ( *pcac->pCallbackGuard.get(), guard );
            }
            else {
              //
              // we will definately stall out here if all of the
              // following are true
              //
              // o user creates non-preemtive mode client library context
              // o user doesnt periodically call a ca function
              // o user calls this function from an auxiillary thread
              //
              CallbackGuard cbGuard ( pcac->cbMutex );
              epicsGuard < epicsMutex > guard ( pcac->mutex );
              isComplete = pcasg->ioComplete ( cbGuard, guard );
            }
            if ( isComplete ) {
                caStatus = ECA_IODONE;
            }
            else{
                caStatus = ECA_IOINPROGRESS;
            }
        }
        else {
            caStatus = ECA_BADSYNCGRP;
        }
    }
    return caStatus;
}

/*
 * ca_sg_array_put()
 */
extern "C" int epicsShareAPI ca_sg_array_put ( const CA_SYNC_GID gid, chtype type,
    arrayElementCount count, chid pChan, const void *pValue )
{
    ca_client_context *pcac;

    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    epicsGuard < epicsMutex > guard ( pcac->mutexRef() );
    CASG * const pcasg = pcac->lookupCASG ( guard, gid );
    if ( ! pcasg ) {
        return ECA_BADSYNCGRP;
    }

    try {
        pcasg->put ( guard, pChan, type, 
            static_cast < unsigned > ( count ), pValue );
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noWriteAccess & )
    {
        return ECA_NOWTACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_UNAVAILINSERV;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        return ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        return ECA_INTERNAL;
    }
}

/*
 * ca_sg_array_get()
 */
extern "C" int epicsShareAPI ca_sg_array_get ( const CA_SYNC_GID gid, chtype type,
    arrayElementCount count, chid pChan, void *pValue )
{
    ca_client_context *pcac;

    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    epicsGuard < epicsMutex > guard ( pcac->mutexRef() );
    CASG * const pcasg = pcac->lookupCASG ( guard, gid );
    if ( ! pcasg ) {
        return ECA_BADSYNCGRP;
    }

    try {
        pcasg->get ( guard, pChan, type, 
            static_cast < unsigned > ( count ), pValue );
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noReadAccess & )
    {
        return ECA_NORDACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_UNAVAILINSERV;
    }
    catch ( std::bad_alloc & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        return ECA_INTERNAL;
    }
}
