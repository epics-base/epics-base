
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

extern "C" void putNotifyCompletion ( putNotify *ppn );

class dbPutNotifyIO : public cacNotifyIO {
public:
    dbPutNotifyIO ( cacNotify &notify );
    int initiate ( struct dbAddr &addr, unsigned type, 
        unsigned long count, const void *pValue);
    void destroy ();
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
private:
    putNotify pn;
    bool ioComplete;
    static tsFreeList < dbPutNotifyIO > freeList;
    ~dbPutNotifyIO (); // must allocate out of pool
    friend void putNotifyCompletion ( putNotify *ppn );
};

class dbChannelIO;

extern "C" void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl );

class dbSubscriptionIO : public cacNotifyIO {
public:
    dbSubscriptionIO ( dbChannelIO &chanIO, cacNotify &, unsigned type, unsigned long count );
    int dbSubscriptionIO::begin ( struct dbAddr &addr, unsigned mask );
    void destroy ();
    static void * operator new ( size_t size );
    static void operator delete ( void *pCadaver, size_t size );
private:
    dbChannelIO &chan;
    dbEventSubscription es;
    unsigned type;
    unsigned long count;
    static tsFreeList < dbSubscriptionIO > freeList;
    ~dbSubscriptionIO (); // must be allocated from pool
    friend void dbSubscriptionEventCallback ( void *user_arg, struct dbAddr *paddr,
	    int eventsRemaining, struct db_field_log *pfl );
};

class dbServiceIO;

class dbChannelIO : public cacChannelIO {
public:
    dbChannelIO (  cacChannel &chan, const dbAddr &addr, dbServiceIO &serviceIO );
    void destroy ();
    void subscriptionUpdate ( unsigned type, unsigned long count, 
            const struct db_field_log *pfl, cacNotifyIO &notify);
    dbEventSubscription subscribe ( dbSubscriptionIO &subscr, unsigned mask );

    static void * operator new ( size_t size);
    static void operator delete ( void *pCadaver, size_t size );

private:
    dbServiceIO &serviceIO;
    char *pGetCallbackCache;
    unsigned long getCallbackCacheSize;
    dbAddr addr;

    static tsFreeList < dbChannelIO > freeList;

    ~dbChannelIO (); // allocate only from pool

    const char *pName () const;
    int read ( unsigned type, unsigned long count, void *pValue );
    int read ( unsigned type, unsigned long count, cacNotify & );
    int write ( unsigned type, unsigned long count, const void *pvalue );
    int write ( unsigned type, unsigned long count, const void *pvalue, cacNotify & );
    int subscribe ( unsigned type, unsigned long count, unsigned mask, cacNotify &notify );
    short nativeType () const;
    unsigned long nativeElementCount () const;
};

class dbServiceIO : public cacServiceIO {
public:
    dbServiceIO ();
    ~dbServiceIO ();
    cacChannelIO *createChannelIO ( cacChannel &chan, const char *pName );
    void subscriptionUpdate ( struct dbAddr &addr, unsigned type, unsigned long count, 
            const struct db_field_log *pfl, cacNotifyIO &notify );
    dbEventSubscription subscribe ( struct dbAddr &addr, dbSubscriptionIO &subscr, unsigned mask );
private:
    dbEventCtx ctx;
    char *pEventCallbackCache;
    unsigned long eventCallbackCacheSize;
    osiMutex mutex;
};
