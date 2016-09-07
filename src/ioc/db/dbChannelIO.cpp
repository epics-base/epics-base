/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*  
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <string>
#include <stdexcept>

#include <limits.h>

#include "tsFreeList.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "db_access.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIO.h"
#include "dbPutNotifyBlocker.h"

dbChannelIO::dbChannelIO (
    epicsMutex & mutexIn, cacChannelNotify & notify,
    dbChannel * dbchIn, dbContext & serviceIO ) :
    cacChannel ( notify ), mutex ( mutexIn ), serviceIO ( serviceIO ),
    dbch ( dbchIn )
{
}

void dbChannelIO::initiateConnect ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->notify().connectNotify ( guard );
}

dbChannelIO::~dbChannelIO ()
{
}

void dbChannelIO::destructor ( CallbackGuard & cbGuard, 
                              epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.destroyAllIO ( cbGuard, guard, *this );
    dbChannelDelete ( this->dbch );
    this->~dbChannelIO ();
}

void dbChannelIO::destroy (
    CallbackGuard & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.destroyChannel ( cbGuard, guard, *this );
    // don't access this pointer after above call because
    // object no longer exists
}

cacChannel::ioStatus dbChannelIO::read (
     epicsGuard < epicsMutex > & guard, unsigned type,
     unsigned long count, cacReadNotify & notify, ioid * )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.callReadNotify ( guard, this->dbch,
        type, count, notify );
    return iosSynch;
}

void dbChannelIO::write (
    epicsGuard < epicsMutex > & guard, unsigned type,
    unsigned long count, const void *pValue )
{
    epicsGuardRelease < epicsMutex > unguard ( guard );
    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }
    int status = dbChannel_put ( this->dbch, type, pValue,
        static_cast <long> (count) );
    if ( status ) {
        throw std::logic_error (
           "db_put_field() completed unsuccessfully" );
    }
}

cacChannel::ioStatus dbChannelIO::write (
    epicsGuard < epicsMutex > & guard, unsigned type,
    unsigned long count, const void * pValue,
    cacWriteNotify & notify, ioid * pId )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }

    this->serviceIO.initiatePutNotify (
        guard, *this, this->dbch,
        type, count, pValue, notify, pId );

    return iosAsynch;
}

void dbChannelIO::subscribe (
    epicsGuard < epicsMutex > & guard, unsigned type, unsigned long count,
    unsigned mask, cacStateNotify & notify, ioid * pId )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.subscribe (
        guard, this->dbch, *this,
        type, count, mask, notify, pId );
}

void dbChannelIO::ioCancel (
    CallbackGuard & cbGuard,
    epicsGuard < epicsMutex > & mutualExclusionGuard,
    const ioid & id )
{
    mutualExclusionGuard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.ioCancel ( cbGuard, mutualExclusionGuard, *this, id );
}

void dbChannelIO::ioShow (
    epicsGuard < epicsMutex > & guard,
    const ioid & id, unsigned level ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.ioShow ( guard, id, level );
}

void dbChannelIO::show (
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    guard.assertIdenticalMutex ( this->mutex );

    printf ("channel at %p attached to local database record %s\n",
        static_cast <const void *> ( this ),
        dbChannelRecord ( this->dbch ) -> name );

    if ( level > 0u ) {
        printf ( "        type %s, element count %li, field at %p\n",
            dbf_type_to_text ( dbChannelExportCAType ( this->dbch ) ),
            dbChannelElements ( this->dbch ),
            dbChannelField ( this->dbch ) );
        if ( level > 1u ) {
            dbChannelFilterShow ( this->dbch, level - 2u, 8 );
            this->serviceIO.show ( level - 2u );
            this->serviceIO.showAllIO ( *this, level - 2u );
        }
    }
}

unsigned long dbChannelIO::nativeElementCount (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    long elements = dbChannelElements ( this->dbch );
    if ( elements >= 0u ) {
        return static_cast < unsigned long > ( elements );
    }
    return 0u;
}

// hopefully to be eventually phased out
const char * dbChannelIO::pName (
    epicsGuard < epicsMutex > & guard ) const throw ()
{
    guard.assertIdenticalMutex ( this->mutex );
    return dbChannelName ( this->dbch );
}

unsigned dbChannelIO::getName (
    epicsGuard < epicsMutex > &,
    char * pBuf, unsigned bufLen ) const throw ()
{
    const char *name = dbChannelName ( this->dbch );
    size_t len = strlen ( name );
    strncpy ( pBuf, name, bufLen );
    if (len < bufLen)
        return (unsigned) len;
    pBuf[--bufLen] = '\0';
    return bufLen;
}

short dbChannelIO::nativeType (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return dbChannelExportCAType( this->dbch );
}

void * dbChannelIO::operator new ( size_t size,
    tsFreeList < dbChannelIO, 256, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void dbChannelIO::operator delete ( void *pCadaver,
    tsFreeList < dbChannelIO, 256, epicsMutexNOOP > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

void dbChannelIO::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void dbChannelIO::flush (
    epicsGuard < epicsMutex > & )
{
}

unsigned dbChannelIO::requestMessageBytesPending (
    epicsGuard < epicsMutex > & )
{
    return 0u;
}

