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

syncGroupWriteNotify::syncGroupWriteNotify ( CASG & sgIn, chid pChan ) :
    syncGroupNotify ( sgIn, pChan )
{
}

void syncGroupWriteNotify::begin ( unsigned type, 
                      arrayElementCount count, const void * pValueIn )
{
    this->chan->write ( type, count, pValueIn, *this, &this->id );
    this->idIsValid = true;
}

syncGroupWriteNotify * syncGroupWriteNotify::factory ( 
    tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > &freeList, 
    struct CASG &sg, chid chan )
{
    return new ( freeList ) syncGroupWriteNotify ( sg, chan );
}

void syncGroupWriteNotify::destroy ( casgRecycle & recycle )
{
    this->~syncGroupWriteNotify ();
    recycle.recycleSyncGroupWriteNotify ( * this );
}

syncGroupWriteNotify::~syncGroupWriteNotify ()
{
    if ( this->idIsValid ) {
        this->chan->ioCancel ( this->id );
    }
}

void syncGroupWriteNotify::completion ()
{
    if ( this->magic != CASG_MAGIC ) {
        this->sg.printf ( "cac: sync group io_complete(): bad sync grp op magic number?\n" );
        return;
    }
    this->idIsValid = false;
    this->sg.completionNotify ( *this );
}

void syncGroupWriteNotify::exception (
    int status, const char *pContext, unsigned type, arrayElementCount count )
{
   this->sg.exception ( status, pContext, 
        __FILE__, __LINE__, *this->chan, type, count, CA_OP_PUT );
    //
    // This notify is left installed at this point as a place holder indicating that
    // all requests have not been completed. This notify is not uninstalled until
    // CASG::block() times out or unit they call CASG::reset().
    //
}

void syncGroupWriteNotify::show ( unsigned level ) const
{
    ::printf ( "pending write sg op\n" );
    if ( level > 0u ) {
        this->syncGroupNotify::show ( level - 1u );
    }
}

void * syncGroupWriteNotify::operator new ( size_t )
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void syncGroupWriteNotify::operator delete ( void * p )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void * syncGroupWriteNotify::operator new ( size_t size, 
    tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#if defined ( CXX_PLACEMENT_DELETE )
void syncGroupWriteNotify::operator delete ( void *pCadaver, 
    tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > &freeList ) epicsThrows(())
{
    freeList.release ( pCadaver );
}
#endif

