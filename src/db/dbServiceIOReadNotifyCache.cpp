
#include "epicsMutex.h"
#include "tsFreeList.h"

#include "cacIO.h"
#include "cadef.h" // this can be eliminated when the callbacks use the new interface
#include "db_access.h" // should be eliminated here in the future

#define epicsExportSharedSymbols

#include "db_access_routines.h"
#include "dbCAC.h"

dbServiceIOReadNotifyCache::dbServiceIOReadNotifyCache () :
	readNotifyCacheSize ( 0 ), pReadNotifyCache ( 0 )
{
}

dbServiceIOReadNotifyCache::~dbServiceIOReadNotifyCache ()
{
	delete this->pReadNotifyCache;
}

void dbServiceIOReadNotifyCache::show ( unsigned level ) const
{
    if ( level > 0 ) {
        printf ( "\tget call back cache location %p, and its size %lu\n", 
            static_cast <void *> ( this->pReadNotifyCache ), 
            this->readNotifyCacheSize );
    }
}

// extra effort taken here to not hold the lock when caslling the callback
void dbServiceIOReadNotifyCache::callReadNotify ( struct dbAddr &addr, 
	unsigned type, unsigned long count, cacReadNotify &notify )
{
    epicsGuard < epicsMutex > locker ( this->mutex );

    unsigned long size = dbr_size_n ( type, count );

    if ( type > INT_MAX ) {
        notify.exception ( ECA_BADTYPE, 
            "type code out of range (high side)", 
            type, count );
        return;
    }

    if ( count > static_cast<unsigned>(INT_MAX) || 
		count > static_cast<unsigned>(addr.no_elements) ) {
        notify.exception ( ECA_BADCOUNT, 
            "element count out of range (high side)",
            type, count);
        return;
    }

    if ( this->readNotifyCacheSize < size) {
        char * pTmp = new char [size];
        if ( ! pTmp ) {
            notify.exception ( ECA_ALLOCMEM, 
                "unable to allocate callback cache",
                type, count );
            return;
        }
        delete [] this->pReadNotifyCache;
        this->pReadNotifyCache = pTmp;
        this->readNotifyCacheSize = size;
    }
    int status = db_get_field ( &addr, static_cast <int> ( type ), 
                    this->pReadNotifyCache, static_cast <int> ( count ), 0 );
    if ( status ) {
        notify.exception ( ECA_GETFAIL, 
            "db_get_field() completed unsuccessfuly",
            type, count);
    }
    else { 
        notify.completion ( type, count, this->pReadNotifyCache );
    }
}
