
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


#include "dbNotify.h"
#include "dbEvent.h"
#include "dbAddr.h"
#include "dbLock.h"
#include "dbCommon.h"
#include "db_convert.h"

extern "C" void putNotifyCompletion ( putNotify *ppn );

class dbChannelIO;
class dbPutNotifyBlocker;

class dbPutNotifyIO : public cacNotifyIO {
public:
    dbPutNotifyIO ( cacNotify &, dbPutNotifyBlocker & );
    int initiate ( struct dbAddr &addr, unsigned type, 
        unsigned long count, const void *pValue);
    void completion ();
    void destroy ();
    cacChannelIO & channelIO () const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~dbPutNotifyIO (); // must allocate out of pool
private:
    putNotify pn;
    dbPutNotifyBlocker &blocker;
    static tsFreeList < dbPutNotifyIO > freeList;
};

extern "C" void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl );

class dbSubscriptionIO : public cacNotifyIO, public tsDLNode <dbSubscriptionIO> {
public:
    dbSubscriptionIO ( dbChannelIO &chanIO, cacNotify &, unsigned type, unsigned long count );
    int begin ( struct dbAddr &addr, unsigned mask );
    void destroy ();
    void show ( unsigned level ) const;
    cacChannelIO & channelIO () const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~dbSubscriptionIO (); // must be allocated from pool
private:
    dbChannelIO &chan;
    dbEventSubscription es;
    unsigned type;
    unsigned long count;
    static tsFreeList < dbSubscriptionIO > freeList;
    friend void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	    int eventsRemaining, struct db_field_log *pfl );
};

class dbServiceIO;

class dbPutNotifyBlocker {
public:
    dbPutNotifyBlocker ( dbChannelIO &chanIn );
    void destroy ();
    int initiatePutNotify ( cacNotify &notify, struct dbAddr &addr, 
            unsigned type, unsigned long count, const void *pValue );
    void putNotifyDestroyNotify ();
    dbChannelIO & channel () const;
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~dbPutNotifyBlocker (); // must allocate out of pool
private:
    epicsEvent block;
    dbPutNotifyIO *pPN;
    dbChannelIO &chan;
    void lock () const;
    void unlock () const;
    static tsFreeList < dbPutNotifyBlocker > freeList;
    friend void putNotifyCompletion ( putNotify *ppn );
};

class dbChannelIO : public cacChannelIO {
public:
    dbChannelIO ( cacChannelNotify &notify, 
        const dbAddr &addr, dbServiceIO &serviceIO );
    void destroy ();
    void subscriptionUpdate ( unsigned type, unsigned long count, 
            const struct db_field_log *pfl, dbSubscriptionIO &notify );
    dbEventSubscription subscribe ( dbSubscriptionIO &subscr, unsigned mask );
    void show ( unsigned level ) const;
    void * operator new ( size_t size);
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~dbChannelIO (); // allocate only from pool
private:
    dbServiceIO &serviceIO;
    char *pGetCallbackCache;
    dbPutNotifyBlocker *pBlocker;
    unsigned long getCallbackCacheSize;
    tsDLList < dbSubscriptionIO > eventq;
    dbAddr addr;
    const char *pName () const;
    void initiateConnect ();
    int read ( unsigned type, unsigned long count, void *pValue );
    int read ( unsigned type, unsigned long count, cacNotify & );
    int write ( unsigned type, unsigned long count, const void *pvalue );
    int write ( unsigned type, unsigned long count, const void *pvalue, cacNotify & );
    int subscribe ( unsigned type, unsigned long count, 
        unsigned mask, cacNotify &notify, cacNotifyIO *& );
    short nativeType () const;
    unsigned long nativeElementCount () const;
    void lock () const;
    void unlock () const;
    void lockOutstandingIO () const;
    void unlockOutstandingIO () const;
    static tsFreeList < dbChannelIO > freeList;
    friend dbSubscriptionIO::dbSubscriptionIO ( dbChannelIO &chanIO, 
        cacNotify &, unsigned type, unsigned long count );
    friend dbSubscriptionIO::~dbSubscriptionIO ();
    friend void dbPutNotifyBlocker::lock () const;
    friend void dbPutNotifyBlocker::unlock () const;
};

class dbServiceIO : public cacServiceIO {
public:
    dbServiceIO ();
    virtual ~dbServiceIO ();
    cacChannelIO *createChannelIO ( const char *pName, cac &, cacChannelNotify & );
    void subscriptionUpdate ( struct dbAddr &addr, unsigned type, unsigned long count, 
            const struct db_field_log *pfl, dbSubscriptionIO &notify );
    dbEventSubscription subscribe ( struct dbAddr &addr, dbSubscriptionIO &subscr, unsigned mask );
    void show ( unsigned level ) const;
private:
    dbEventCtx ctx;
    char *pEventCallbackCache;
    unsigned long eventCallbackCacheSize;
    epicsMutex mutex;
};

