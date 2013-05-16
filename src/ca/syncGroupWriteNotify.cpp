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

syncGroupWriteNotify::syncGroupWriteNotify ( CASG & sgIn, 
                      PRecycleFunc pRecycleFuncIn, chid pChan ) :
    chan ( pChan ), pRecycleFunc ( pRecycleFuncIn ),
    sg ( sgIn ), magic ( CASG_MAGIC ),
        id ( 0u ), idIsValid ( false ), ioComplete ( false )
{
}

void syncGroupWriteNotify::begin ( 
    epicsGuard < epicsMutex > & guard, unsigned type, 
    arrayElementCount count, const void * pValueIn )
{
    this->chan->eliminateExcessiveSendBacklog ( guard );
    this->ioComplete = false;
    boolFlagManager mgr ( this->idIsValid );
    this->chan->write ( guard, type, count, 
        pValueIn, *this, &this->id );
    mgr.release ();
}

void syncGroupWriteNotify::cancel ( 
    CallbackGuard & callbackGuard,
    epicsGuard < epicsMutex > & mutualExcusionGuard )
{
    if ( this->idIsValid ) {
        this->chan->ioCancel ( callbackGuard, mutualExcusionGuard, this->id );
        this->idIsValid = false;
    }
}

syncGroupWriteNotify * syncGroupWriteNotify::factory ( 
    tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > &freeList, 
    struct CASG & sg, PRecycleFunc pRecycleFunc, chid chan )
{
    return new ( freeList ) syncGroupWriteNotify ( sg, pRecycleFunc, chan );
}

void syncGroupWriteNotify::destroy ( 
    CallbackGuard &,
    epicsGuard < epicsMutex > & guard )
{
    CASG & sgRef ( this->sg );
    this->~syncGroupWriteNotify ();
    ( sgRef.*pRecycleFunc ) ( guard, *this );
}

syncGroupWriteNotify::~syncGroupWriteNotify ()
{
    assert ( ! this->idIsValid );
}

void syncGroupWriteNotify::completion ( 
    epicsGuard < epicsMutex > & guard )
{
    if ( this->magic != CASG_MAGIC ) {
        this->sg.printFormated ( "cac: sync group io_complete(): bad sync grp op magic number?\n" );
        return;
    }
    this->sg.completionNotify ( guard, *this );
    this->idIsValid = false;
    this->ioComplete = true;
}

void syncGroupWriteNotify::exception (
    epicsGuard < epicsMutex > & guard, 
    int status, const char *pContext, unsigned type, arrayElementCount count )
{
    if ( this->magic != CASG_MAGIC ) {
        this->sg.printFormated ( "cac: sync group io_complete(): bad sync grp op magic number?\n" );
        return;
    }
    this->sg.exception ( guard, status, pContext, 
            __FILE__, __LINE__, *this->chan, type, count, CA_OP_PUT );
    this->idIsValid = false;
    //
    // This notify is left installed at this point as a place holder indicating that
    // all requests have not been completed. This notify is not uninstalled until
    // CASG::block() times out or unit they call CASG::reset().
    //
}

void syncGroupWriteNotify::show ( 
    epicsGuard < epicsMutex > &, unsigned level ) const
{
    ::printf ( "pending write sg op\n" );
    if ( level > 0u ) {
        ::printf ( "pending sg op: magic=%u sg=%p\n",
            this->magic, static_cast < void * > ( &this->sg ) );
    }
}

void syncGroupWriteNotify::operator delete ( void * )
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
    tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > &freeList ) 
{
    freeList.release ( pCadaver );
}
#endif

