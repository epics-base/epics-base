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
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <stdexcept>

#include <limits.h>

#include "tsFreeList.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "db_access.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIO.h"
#include "dbPutNotifyBlocker.h"

dbChannelIO::dbChannelIO ( cacChannelNotify & notify, 
    const dbAddr & addrIn, dbServiceIO & serviceIO ) :
    cacChannel ( notify ), serviceIO ( serviceIO ), 
    addr ( addrIn )
{
}

void dbChannelIO::initiateConnect ()
{
    this->notify ().connectNotify ();
}

dbChannelIO::~dbChannelIO ()
{
    this->serviceIO.destroyAllIO ( *this );
}

void dbChannelIO::destroy () 
{
    this->serviceIO.destroyChannel ( *this );
    // dont access this pointer after above call because
    // object nolonger exists
}


cacChannel::ioStatus dbChannelIO::read ( unsigned type, 
     unsigned long count, cacReadNotify &notify, ioid * ) 
{
    this->serviceIO.callReadNotify ( this->addr, 
        type, count, notify );
    return iosSynch;
}

void dbChannelIO::write ( unsigned type, unsigned long count, const void *pValue )
{
    int status;
    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }
    status = db_put_field ( &this->addr, type, pValue, static_cast <long> (count) );
    if ( status ) {
        throw -1; 
    }
}

cacChannel::ioStatus dbChannelIO::write ( unsigned type, unsigned long count, 
                        const void *pValue, cacWriteNotify &notify, ioid *pId ) 
{
    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }

    this->serviceIO.initiatePutNotify ( *this, this->addr, type, count, pValue, notify, pId );

    return iosAsynch;
}

void dbChannelIO::subscribe ( unsigned type, unsigned long count, 
    unsigned mask, cacStateNotify & notify, ioid * pId ) 
{   
    this->serviceIO.subscribe ( this->addr, *this,
        type, count, mask, notify, pId );
}

void dbChannelIO::ioCancel ( const ioid & id )
{
    this->serviceIO.ioCancel ( *this, id );
}

void dbChannelIO::ioShow ( const ioid &id, unsigned level ) const
{
    this->serviceIO.ioShow ( id, level );
}

void dbChannelIO::show ( unsigned level ) const
{
    printf ("channel at %p attached to local database record %s\n", 
        static_cast <const void *> ( this ), this->addr.precord->name );

    if ( level > 0u ) {
        printf ( "\ttype %s, element count %li, field at %p\n",
            dbf_type_to_text ( this->addr.dbr_field_type ), this->addr.no_elements,
            this->addr.pfield );
    }
    if ( level > 1u ) {
        this->serviceIO.show ( level - 2u );
        this->serviceIO.showAllIO ( *this, level - 2u );
    }
}

void * dbChannelIO::operator new ( size_t size, 
    tsFreeList < dbChannelIO > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void dbChannelIO::operator delete ( void *pCadaver, 
    tsFreeList < dbChannelIO > & freeList )
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


