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
extern "C" int epicsShareAPI ca_sg_create ( CA_SYNC_GID * pgid ) // X aCC 361
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

/*
 * ca_sg_delete()
 */
extern "C" int epicsShareAPI ca_sg_delete ( const CA_SYNC_GID gid )
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext ( & pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( pcac->pCallbackGuard.get() ) {
        epicsGuard < epicsMutex > guard ( pcac->mutexRef() );

        CASG * pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            return ECA_BADSYNCGRP;
        }

        pcasg->destructor ( *pcac->pCallbackGuard, guard );
        pcac->casgFreeList.release ( pcasg );
    }
    else {
        epicsGuard < epicsMutex > cbGuard ( pcac->cbMutex );
        epicsGuard < epicsMutex > guard ( pcac->mutexRef() );

        CASG * pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            return ECA_BADSYNCGRP;
        }

        pcasg->destructor ( cbGuard, guard );
        pcac->casgFreeList.release ( pcasg );
    }

    return ECA_NORMAL;
}

/*
 * ca_sg_block ()
 */
extern "C" int epicsShareAPI ca_sg_block ( const CA_SYNC_GID gid, ca_real timeout )
{
    ca_client_context *pcac;
    CASG *pcasg;
    int status;

    status = fetchClientContext ( &pcac );
    if ( status != ECA_NORMAL ) {
        return status;
    }

    if ( pcac->pCallbackGuard.get() ) {
        epicsGuard < epicsMutex > guard ( pcac->mutex );

        pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            status = ECA_BADSYNCGRP;
        }
        else {
            status = pcasg->block ( *pcac->pCallbackGuard, guard, timeout );
        }
    }
    else {
        epicsGuard < epicsMutex > cbGuard ( pcac->cbMutex);
        epicsGuard < epicsMutex > guard ( pcac->mutex );

        pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            status = ECA_BADSYNCGRP;
        }
        else {
            status = pcasg->block ( cbGuard, guard, timeout );
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
    CASG *pcasg;
    int caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( pcac->pCallbackGuard.get() ) {
        epicsGuard < epicsMutex > guard ( pcac->mutex );

        pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            caStatus = ECA_BADSYNCGRP;
        }
        else {
            caStatus = ECA_NORMAL;
            pcasg->reset ( *pcac->pCallbackGuard, guard );
        }
    }
    else {
        epicsGuard < epicsMutex > cbGuard ( pcac->cbMutex );
        epicsGuard < epicsMutex > guard ( pcac->mutex );

        pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            caStatus = ECA_BADSYNCGRP;
        }
        else {
            caStatus = ECA_NORMAL;
            pcasg->reset ( cbGuard, guard );
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
extern "C" int epicsShareAPI ca_sg_test ( const CA_SYNC_GID gid ) // X aCC 361
{
    ca_client_context *pcac;
    CASG *pcasg;
    int caStatus;

    caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( pcac->pCallbackGuard.get() ) {
        epicsGuard < epicsMutex > guard ( pcac->mutexRef() );

        pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            return ECA_BADSYNCGRP;
        }
        if ( pcasg->ioComplete ( *pcac->pCallbackGuard, guard ) ) {
            return ECA_IODONE;
        }
        else{
            return ECA_IOINPROGRESS;
        }
    }
    else {
        epicsGuard < epicsMutex > cbGuard ( pcac->cbMutex );
        epicsGuard < epicsMutex > guard ( pcac->mutexRef() );

        pcasg = pcac->lookupCASG ( guard, gid );
        if ( ! pcasg ) {
            return ECA_BADSYNCGRP;
        }
        if ( pcasg->ioComplete ( cbGuard, guard ) ) {
            return ECA_IODONE;
        }
        else{
            return ECA_IOINPROGRESS;
        }
    }
}

/*
 * ca_sg_array_put()
 */
extern "C" int epicsShareAPI ca_sg_array_put ( const CA_SYNC_GID gid, chtype type,
    arrayElementCount count, chid pChan, const void *pValue )
{
    ca_client_context *pcac;
    CASG *pcasg;
    int caStatus;
    
    caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    epicsGuard < epicsMutex > guard ( pcac->mutexRef() );

    pcasg = pcac->lookupCASG ( guard, gid );
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
    CASG *pcasg;
    int caStatus;

    caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    epicsGuard < epicsMutex > guard ( pcac->mutexRef() );

    pcasg = pcac->lookupCASG ( guard, gid );
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
