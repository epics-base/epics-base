
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

#include "limits.h"
#include "string.h"

#include "osiMutex.h"
#include "tsFreeList.h"
#include "errMdef.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "dbCAC.h"

#define S_db_Blocked 	(M_dbAccess|39)
#define S_db_Pending 	(M_dbAccess|37)

tsFreeList <dbPutNotifyIO> dbPutNotifyIO::freeList;

extern "C" void putNotifyCompletion ( putNotify *ppn )
{
    dbPutNotifyIO *pNotify = static_cast < dbPutNotifyIO * > ( ppn->usrPvt );
    if ( pNotify->pn.status ) {
        if (pNotify->pn.status == S_db_Blocked) {
            pNotify->cacNotifyIO::exceptionNotify ( ECA_PUTCBINPROG, "put notify blocked" );
        }
        else {
            pNotify->cacNotifyIO::exceptionNotify ( ECA_PUTFAIL,  "put notify unsuccessful");
        }
    }
    else {
        pNotify->cacNotifyIO::completionNotify ();
    }
    pNotify->ioComplete = true;
    pNotify->destroy ();
}

dbPutNotifyIO::dbPutNotifyIO ( cacNotify &notifyIn ) :
    cacNotifyIO (notifyIn), ioComplete (false) 
{
    memset (&this->pn, '\0', sizeof (this->pn));
    this->pn.userCallback = putNotifyCompletion;
    this->pn.usrPvt = this;
}

dbPutNotifyIO::~dbPutNotifyIO ()
{
    if ( ! this->ioComplete ) {
        dbNotifyCancel ( &this->pn );
    }
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
        return ECA_BADTYPE;
    }

    status = ::dbPutNotify ( &this->pn );
    if ( status && status != S_db_Pending ) {
        this->pn.status = status;
        this->cacNotifyIO::exceptionNotify ( ECA_PUTFAIL,  "dbPutNotify() returned failure");
    }
    return ECA_NORMAL;
}

void dbPutNotifyIO::destroy () 
{
    delete this;
}

void * dbPutNotifyIO::operator new ( size_t size )
{
    return dbPutNotifyIO::freeList.allocate ( size );
}

void dbPutNotifyIO::operator delete ( void *pCadaver, size_t size )
{
    dbPutNotifyIO::freeList.release ( pCadaver, size );
}
