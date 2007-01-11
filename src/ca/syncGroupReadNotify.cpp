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

#include <string>
#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"

syncGroupReadNotify::syncGroupReadNotify ( 
    CASG & sgIn, chid pChan, void * pValueIn ) :
    chan ( pChan ), sg ( sgIn ), pValue ( pValueIn ), 
    magic ( CASG_MAGIC ), id ( 0u ), 
    idIsValid ( false ), ioComplete ( false )
{
}

void syncGroupReadNotify::begin ( 
    epicsGuard < epicsMutex > & guard, 
    unsigned type, arrayElementCount count )
{
    this->chan->eliminateExcessiveSendBacklog ( guard );
    this->ioComplete = false;
    boolFlagManager mgr ( this->idIsValid );
    this->chan->read ( guard, type, count, *this, &this->id );
    mgr.release ();
}

void syncGroupReadNotify::cancel ( 
    epicsGuard < epicsMutex > & guard )
{
    if ( this->idIsValid ) {
        this->chan->ioCancel ( guard, this->id );
        this->idIsValid = false;
    }
}

syncGroupReadNotify * syncGroupReadNotify::factory ( 
    tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > & freeList, 
    struct CASG & sg, chid chan, void * pValueIn )
{
    return new ( freeList ) // X aCC 930
        syncGroupReadNotify ( sg, chan, pValueIn );
}

void syncGroupReadNotify::destroy ( 
    epicsGuard < epicsMutex > & guard, casgRecycle & recycle )
{
    this->~syncGroupReadNotify ();
    recycle.recycleSyncGroupReadNotify ( guard, *this );
}

syncGroupReadNotify::~syncGroupReadNotify ()
{
    assert ( ! this->idIsValid );
}

void syncGroupReadNotify::completion (
    epicsGuard < epicsMutex > & guard, unsigned type, 
    arrayElementCount count, const void * pData )
{
    if ( this->magic != CASG_MAGIC ) {
        this->sg.printf ( 
            "cac: sync group io_complete(): bad sync grp op magic number?\n" );
        return;
    }

    if ( this->pValue ) {
        size_t size = dbr_size_n ( type, count );
        memcpy ( this->pValue, pData, size );
    }
    this->sg.completionNotify ( guard, *this );
    this->idIsValid = false;
    this->ioComplete = true;
}

void syncGroupReadNotify::exception (
    epicsGuard < epicsMutex > & guard, int status, const char * pContext, 
    unsigned type, arrayElementCount count )
{
    if ( this->magic != CASG_MAGIC ) {
        this->sg.printf ( 
            "cac: sync group io_complete(): bad sync grp op magic number?\n" );
        return;
    }
    this->idIsValid = false;
    this->sg.exception ( guard, status, pContext, 
        __FILE__, __LINE__, *this->chan, type, count, CA_OP_GET );
    //
    // This notify is left installed at this point as a place holder indicating that
    // all requests have not been completed. This notify is not uninstalled until
    // CASG::block() times out or unit they call CASG::reset().
    //
}

void syncGroupReadNotify::show ( 
    epicsGuard < epicsMutex > &, unsigned level ) const
{
    ::printf ( "pending sg read op: pVal=%p\n", this->pValue );
    if ( level > 0u ) {
        ::printf ( "pending sg op: magic=%u sg=%p\n",
            this->magic, static_cast < void * > ( & this->sg ) );
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
