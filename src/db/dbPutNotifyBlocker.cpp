
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

#include <limits.h>
#include <string.h>

#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "tsFreeList.h"
#include "epicsSingleton.h"
#include "errMdef.h"

#include "cacIO.h"
#include "caerr.h" // this needs to be eliminated
#include "db_access.h" // this needs to be eliminated

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "dbPutNotifyBlocker.h"

template class tsFreeList < dbPutNotifyBlocker, 1024 >;
template class epicsSingleton < tsFreeList < dbPutNotifyBlocker, 1024 > >;

epicsSingleton < tsFreeList < dbPutNotifyBlocker, 1024 > > dbPutNotifyBlocker::pFreeList;

dbPutNotifyBlocker::dbPutNotifyBlocker ( dbChannelIO &chanIn ) :
    pNotify ( 0 )
{
    memset ( &this->pn, '\0', sizeof ( this->pn ) );
}

dbPutNotifyBlocker::~dbPutNotifyBlocker () 
{
    this->cancel ();
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
    memset ( &this->pn, '\0', sizeof ( this->pn ) );
    this->pNotify = 0;
    this->block.signal ();
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
    memset ( &pBlocker->pn, '\0', sizeof ( pBlocker->pn ) );
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
        memset ( &this->pn, '\0', sizeof ( this->pn ) );
        this->pNotify = 0;
        throw cacChannel::badType();
    }

    this->pn.pbuffer = const_cast < void * > ( pValue );
    this->pn.nRequest = static_cast < unsigned > ( count );
    this->pn.paddr = &addr;
    this->pn.userCallback = putNotifyCompletion;
    this->pn.usrPvt = this;

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

