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
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 *
 * NOTES:
 * 1) This interface is preliminary and will change in the future
 */

#ifndef dbCACh
#define dbCACh

#ifdef epicsExportSharedSymbols
#define dbCACh_restore_epicsExportSharedSymbols
#undef epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "tsDLList.h"
#include "epicsSingleton.h"
#include "tsFreeList.h"
#include "resourceLib.h"

#include "cacIO.h"

#ifdef dbCACh_restore_epicsExportSharedSymbols
#define epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "db_access.h"
#include "dbNotify.h"
#include "dbEvent.h"
#include "dbAddr.h"
#include "dbLock.h"
#include "dbCommon.h"
#include "db_convert.h"
#include "resourceLib.h"

extern "C" void putNotifyCompletion ( putNotify *ppn );

class dbServiceIO;
class dbChannelIO;
class dbPutNotifyBlocker;
class dbSubscriptionIO;

class dbBaseIO                  //  X aCC 655
    : public chronIntIdRes < dbBaseIO > {
public:
    virtual dbSubscriptionIO * isSubscription () = 0;
    virtual void destroy () = 0;
    virtual void show ( unsigned level ) const = 0;
	dbBaseIO ();
	dbBaseIO ( const dbBaseIO & );
	dbBaseIO & operator = ( const dbBaseIO & );
};

extern "C" void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl );

class dbSubscriptionIO : public tsDLNode <dbSubscriptionIO>, public dbBaseIO {
public:
    dbSubscriptionIO ( dbServiceIO &, dbChannelIO &, struct dbAddr &, cacStateNotify &, 
        unsigned type, unsigned long count, unsigned mask, cacChannel::ioid * );
    void destroy ();
    void unsubscribe ();
    void channelDeleteException ();
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~dbSubscriptionIO ();
private:
    cacStateNotify & notify;
    dbChannelIO & chan;
    dbEventSubscription es;
    unsigned type;
    unsigned long count;
    unsigned id;
    dbSubscriptionIO * isSubscription ();
    static epicsSingleton < tsFreeList < dbSubscriptionIO > > pFreeList;
    friend void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	    int eventsRemaining, struct db_field_log *pfl );
	dbSubscriptionIO ( const dbSubscriptionIO & );
	dbSubscriptionIO & operator = ( const dbSubscriptionIO & );
};

class dbServiceIO;

class dbServicePrivateListOfIO {
public:
    dbServicePrivateListOfIO ();
private:
    tsDLList < dbSubscriptionIO > eventq;
    dbPutNotifyBlocker *pBlocker;
    friend class dbServiceIO;
	dbServicePrivateListOfIO ( const dbServicePrivateListOfIO & );
	dbServicePrivateListOfIO & operator = ( const dbServicePrivateListOfIO & );
};

// allow only one thread at a time to use the cache, but do not hold
// lock when calling the callback
class dbServiceIOReadNotifyCache  {
public:
    dbServiceIOReadNotifyCache ();
    ~dbServiceIOReadNotifyCache ();
    void callReadNotify ( struct dbAddr &addr, unsigned type, unsigned long count, 
            cacReadNotify &notify );
    void show ( unsigned level ) const;
private:
    epicsMutex mutex;
    unsigned long readNotifyCacheSize;
    char *pReadNotifyCache;
	dbServiceIOReadNotifyCache ( const dbServiceIOReadNotifyCache & );
	dbServiceIOReadNotifyCache & operator = ( const dbServiceIOReadNotifyCache & );
};

class dbServiceIO : public cacService {
public:
    dbServiceIO ();
    virtual ~dbServiceIO ();
    cacChannel * createChannel ( const char *pName, 
                    cacChannelNotify &, cacChannel::priLev );
    void callReadNotify ( struct dbAddr &addr, unsigned type, unsigned long count, 
            cacReadNotify &notify );
    void callStateNotify ( struct dbAddr &addr, unsigned type, unsigned long count, 
            const struct db_field_log *pfl, cacStateNotify &notify );
    dbEventSubscription subscribe ( struct dbAddr & addr, dbChannelIO & chan,
        dbSubscriptionIO & subscr, unsigned mask );
    void initiatePutNotify ( dbChannelIO &, struct dbAddr &, unsigned type, 
        unsigned long count, const void *pValue, cacWriteNotify &notify, 
        cacChannel::ioid *pId ); 
    void show ( unsigned level ) const;
    void showAllIO ( const dbChannelIO &chan, unsigned level ) const;
    void destroyAllIO ( dbChannelIO & chan );
    void ioCancel ( dbChannelIO & chan, const cacChannel::ioid &id );
    void ioShow ( const cacChannel::ioid &id, unsigned level ) const;
private:
    mutable epicsMutex mutex;
    chronIntIdResTable < dbBaseIO > ioTable;
    dbServiceIOReadNotifyCache readNotifyCache;
    dbEventCtx ctx;
    unsigned long stateNotifyCacheSize;
    char *pStateNotifyCache;
	dbServiceIO ( const dbServiceIO & );
	dbServiceIO & operator = ( const dbServiceIO & );
};

inline dbServicePrivateListOfIO::dbServicePrivateListOfIO () :
    pBlocker ( 0 )
{
}

inline void dbServiceIO::callReadNotify ( struct dbAddr &addr, 
        unsigned type, unsigned long count, 
        cacReadNotify &notify )
{
    this->readNotifyCache.callReadNotify ( addr, type, count, notify );
}

#endif // dbCACh

