
/*
 *  $Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"

syncGroupWriteNotify::syncGroupWriteNotify ( CASG &sgIn, chid pChan, unsigned type, 
                       unsigned long count, const void *pValueIn  ) :
    syncGroupNotify ( sgIn, pChan )
{
    pChan->write ( type, count, pValueIn, *this, &this->id );
    this->idIsValid = true;
}

syncGroupWriteNotify * syncGroupWriteNotify::factory ( 
    tsFreeList < class syncGroupWriteNotify, 128 > &freeList, 
    struct CASG &sg, chid chan, unsigned type, 
    unsigned long count, const void *pValueIn )
{
    return new ( freeList ) syncGroupWriteNotify ( sg, chan, type, 
            count, pValueIn);
}

void syncGroupWriteNotify::destroy ( casgRecycle & recycle )
{
    this->~syncGroupWriteNotify ();
    recycle.recycleSyncGroupWriteNotify ( * this );
}

syncGroupWriteNotify::~syncGroupWriteNotify ()
{
}

void syncGroupWriteNotify::completion ()
{
    if ( this->magic != CASG_MAGIC ) {
        this->sg.printf ( "cac: sync group io_complete(): bad sync grp op magic number?\n" );
        return;
    }

    this->idIsValid = false;
    this->sg.destroyIO ( *this );
}

void syncGroupWriteNotify::exception (
    int status, const char *pContext )
{
    ca_signal_formated ( status, __FILE__, __LINE__, 
            "CA sync group write request for channel \"%s\" failed because \"%s\"\n", 
            this->chan->pName(), pContext);
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

void * syncGroupWriteNotify::operator new ( size_t size, 
    tsFreeList < class syncGroupWriteNotify, 128 > & freeList )
{
    return freeList.allocate ( size );
}

#if ! defined ( NO_PLACEMENT_DELETE )
void syncGroupWriteNotify::operator delete ( void *pCadaver, size_t size, 
    tsFreeList < class syncGroupWriteNotify, 128 > &freeList )
{
    freeList.release ( pCadaver, size );
}
#endif
