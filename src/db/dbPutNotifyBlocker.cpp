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
 *  Author: 
 *  Jeffrey O. Hill
 *  johill@lanl.gov
 *  505 665 1831
 */

#include <stdexcept>

#include <limits.h>
#include <string.h>

#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "tsFreeList.h"
#include "errMdef.h"

#include "caerr.h" // this needs to be eliminated
#include "db_access.h" // this needs to be eliminated

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIO.h"
#include "dbPutNotifyBlocker.h"

dbPutNotifyBlocker::dbPutNotifyBlocker () :
    pNotify ( 0 ), maxValueSize ( sizeof ( this->dbrScalarValue ) )
{
    memset ( & this->pn, '\0', sizeof ( this->pn ) );
    memset ( & this->dbrScalarValue, '\0', sizeof ( this->dbrScalarValue ) );
    this->pn.pbuffer = & this->dbrScalarValue;
}

dbPutNotifyBlocker::~dbPutNotifyBlocker () 
{
    this->cancel ();
    if ( this->maxValueSize > sizeof ( this->dbrScalarValue ) ) {
        char * pBuf = static_cast < char * > ( this->pn.pbuffer );
        delete [] pBuf;
    }
}

void dbPutNotifyBlocker::destroy ()
{
    delete this;
}

void dbPutNotifyBlocker::cancel ()
{
    if ( this->pn.paddr ) {
        dbNotifyCancel ( &this->pn );
    }
    this->pNotify = 0;
    this->block.signal ();
}

void dbPutNotifyBlocker::expandValueBuf ( unsigned long newSize )
{
    if ( this->maxValueSize < newSize ) {
        if ( this->maxValueSize > sizeof ( this->dbrScalarValue ) ) {
            char * pBuf = static_cast < char * > ( this->pn.pbuffer );
            delete [] pBuf;
            this->maxValueSize = sizeof ( this->dbrScalarValue );
            this->pn.pbuffer = & this->dbrScalarValue;
        }
        this->pn.pbuffer = new char [newSize];
        this->maxValueSize = newSize;
    }
}

extern "C" void putNotifyCompletion ( putNotify *ppn )
{
    dbPutNotifyBlocker *pBlocker = static_cast < dbPutNotifyBlocker * > ( ppn->usrPvt );
    if ( pBlocker->pNotify ) {
        if ( pBlocker->pn.status != putNotifyOK) {
            if ( pBlocker->pn.status == putNotifyBlocked ) {
                pBlocker->pNotify->exception ( 
                    ECA_PUTCBINPROG, "put notify blocked",
                    static_cast <unsigned> (pBlocker->pn.dbrType), 
                    static_cast <unsigned> (pBlocker->pn.nRequest) );
            }
            else {
                pBlocker->pNotify->exception ( 
                    ECA_PUTFAIL,  "put notify unsuccessful",
                    static_cast <unsigned> (pBlocker->pn.dbrType), 
                    static_cast <unsigned> (pBlocker->pn.nRequest) );
            }
        }
        else {
            pBlocker->pNotify->completion ();
        }
    }
    else {
        errlogPrintf ( "put notify completion with nill pNotify?\n" );
    }
    // no need to lock here because only one put notify at a time is allowed to run
    pBlocker->pNotify = 0;
    pBlocker->block.signal ();
}

void dbPutNotifyBlocker::initiatePutNotify ( epicsGuard < epicsMutex > & locker, cacWriteNotify & notify, 
        struct dbAddr & addr, unsigned type, unsigned long count, const void * pValue )
{
    int status;

    epicsTime begin;
    bool beginTimeInit = false;
    while ( true ) {
        if ( this->pNotify == 0 ) {
            this->pNotify = & notify;
            break;
        }
        if ( beginTimeInit ) {
            if ( epicsTime::getCurrent () - begin > 30.0 ) {
                throw cacChannel::requestTimedOut ();
            }
        }
        else {
            begin = epicsTime::getCurrent ();
            beginTimeInit = true;
        }
        {
            epicsGuardRelease < epicsMutex > autoRelease ( locker );
            this->block.wait ( 1.0 );
        }
    }

    if ( count > LONG_MAX ) {
        throw cacChannel::outOfBounds();
    }

    if ( type > SHRT_MAX ) {
        throw cacChannel::badType();
    }

    status = dbPutNotifyMapType ( 
                &this->pn, static_cast <short> ( type ) );
    if ( status ) {
        this->pNotify = 0;
        throw cacChannel::badType();
    }

    this->pn.nRequest = static_cast < unsigned > ( count );
    this->pn.paddr = &addr;
    this->pn.userCallback = putNotifyCompletion;
    this->pn.usrPvt = this;

    unsigned long size = dbr_size_n ( type, count );
    this->expandValueBuf ( size );
    memcpy ( this->pn.pbuffer, pValue, size );

    ::dbPutNotify ( &this->pn );
}

void dbPutNotifyBlocker::show ( unsigned level ) const
{
    printf ( "put notify blocker at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        this->block.show ( level - 1u );
    }
}

dbSubscriptionIO * dbPutNotifyBlocker::isSubscription () 
{
    return 0;
}

void dbPutNotifyBlocker::operator delete ( void * pCadaver )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}
