
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
 *	Author:
 *  Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include "limits.h"
#include "string.h"

#include "epicsMutex.h"
#include "tsFreeList.h"
#include "errMdef.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbPutNotifyIOIL.h"

#define S_db_Blocked 	(M_dbAccess|39)
#define S_db_Pending 	(M_dbAccess|37)

tsFreeList <dbPutNotifyIO> dbPutNotifyIO::freeList;

dbPutNotifyIO::dbPutNotifyIO ( cacNotify &notifyIn, dbPutNotifyBlocker &blockerIn ) :
    cacNotifyIO ( notifyIn ), blocker ( blockerIn )
{
    memset ( &this->pn, '\0', sizeof ( this->pn ) );
    this->pn.userCallback = putNotifyCompletion;
    this->pn.usrPvt = &blockerIn;
}

dbPutNotifyIO::~dbPutNotifyIO ()
{
    if ( this->pn.paddr ) {
        dbNotifyCancel ( &this->pn );
    }
    this->blocker.putNotifyDestroyNotify ();
}

int dbPutNotifyIO::initiate ( struct dbAddr &addr, unsigned type, 
                             unsigned long count, const void *pValue)
{
    int status;

    if ( count > LONG_MAX ) {
        return ECA_BADCOUNT;
    }
    if ( type > SHRT_MAX ) {
        return ECA_BADTYPE;
    }
    this->pn.pbuffer = const_cast <void *> ( pValue );
    this->pn.nRequest = static_cast <unsigned> ( count );
    this->pn.paddr = &addr;
    status = this->pn.dbrType = dbPutNotifyMapType ( &this->pn, static_cast <short> ( type ) );
    if (status) {
        this->pn.paddr = 0;
        return ECA_BADTYPE;
    }

    status = ::dbPutNotify ( &this->pn );
    if ( status && status != S_db_Pending ) {
        this->pn.paddr = 0;
        this->pn.status = status;
        this->cacNotifyIO::exceptionNotify ( ECA_PUTFAIL,  "dbPutNotify() returned failure");
    }
    return ECA_NORMAL;
}

void dbPutNotifyIO::completion () 
{
    if ( ! this->pn.paddr ) {
        errlogPrintf ( "completion pn=%p\n", this );
    }
    this->pn.paddr = 0;
    if ( this->pn.status ) {
        if ( this->pn.status == S_db_Blocked ) {
            this->cacNotifyIO::exceptionNotify ( ECA_PUTCBINPROG, "put notify blocked" );
        }
        else {
            this->cacNotifyIO::exceptionNotify ( ECA_PUTFAIL,  "put notify unsuccessful");
        }
    }
    else {
        this->cacNotifyIO::completionNotify ();
    }
}

