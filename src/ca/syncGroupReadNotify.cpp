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
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 */

#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"

syncGroupReadNotify::syncGroupReadNotify ( CASG &sgIn, chid pChan, void *pValueIn ) :
    syncGroupNotify ( sgIn, pChan ), pValue ( pValueIn )
{
}

void syncGroupReadNotify::begin ( unsigned type, arrayElementCount count )
{
    this->chan->read ( type, count, *this, &this->id );
    this->idIsValid = true;
}

syncGroupReadNotify * syncGroupReadNotify::factory ( 
    tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > &freeList, 
    struct CASG &sg, chid chan, void *pValueIn )
{
    return new ( freeList ) // X aCC 930
        syncGroupReadNotify ( sg, chan, pValueIn );
}

void syncGroupReadNotify::destroy ( casgRecycle &recycle )
{
    this->~syncGroupReadNotify ();
    recycle.recycleSyncGroupReadNotify ( * this );
}

syncGroupReadNotify::~syncGroupReadNotify ()
{
    if ( this->idIsValid ) {
        this->chan->ioCancel ( this-> id );
    }
}

void syncGroupReadNotify::completion (
    unsigned type, arrayElementCount count, const void *pData )
{
    if ( this->magic != CASG_MAGIC ) {
        this->sg.printf ( "cac: sync group io_complete(): bad sync grp op magic number?\n" );
        return;
    }

    if ( this->pValue ) {
        size_t size = dbr_size_n ( type, count );
        memcpy ( this->pValue, pData, size );
    }
    this->idIsValid = false;
    this->sg.completionNotify ( *this );
}

void syncGroupReadNotify::exception (
    int status, const char *pContext, unsigned type, arrayElementCount count )
{
   this->sg.exception ( status, pContext, 
        __FILE__, __LINE__, *this->chan, type, count, CA_OP_GET );
    //
    // This notify is left installed at this point as a place holder indicating that
    // all requests have not been completed. This notify is not uninstalled until
    // CASG::block() times out or unit they call CASG::reset().
    //
}

void syncGroupReadNotify::show ( unsigned level ) const
{
    ::printf ( "pending sg read op: pVal=%p\n", this->pValue );
    if ( level > 0u ) {
        this->syncGroupNotify::show ( level - 1u );
    }
}

void * syncGroupReadNotify::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void syncGroupReadNotify::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void * syncGroupReadNotify::operator new ( size_t size, 
    tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#if defined ( CXX_PLACEMENT_DELETE )
void syncGroupReadNotify::operator delete ( void *pCadaver, 
    tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > &freeList ) 
{
    freeList.release ( pCadaver );
}
#endif
