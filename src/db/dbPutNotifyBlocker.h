
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

#ifndef dbPutNotifyBlockerh
#define dbPutNotifyBlockerh

class dbPutNotifyBlocker : public dbBaseIO {
public:
    dbPutNotifyBlocker ( dbChannelIO & chanIn );
    void initiatePutNotify ( epicsGuard < epicsMutex > & locker, 
            cacWriteNotify & notify, struct dbAddr & addr, 
            unsigned type, unsigned long count, const void * pValue );
    void cancel ();
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void destroy ();
protected:
    virtual ~dbPutNotifyBlocker ();
private:
    putNotify pn;
    epicsEvent block;
    cacWriteNotify * pNotify;
    dbSubscriptionIO * isSubscription ();
    static epicsSingleton < tsFreeList < dbPutNotifyBlocker > > pFreeList;
    friend void putNotifyCompletion ( putNotify *ppn );
	dbPutNotifyBlocker ( const dbPutNotifyBlocker & );
	dbPutNotifyBlocker & operator = ( const dbPutNotifyBlocker & );
};

inline void * dbPutNotifyBlocker::operator new ( size_t size )
{
    return dbPutNotifyBlocker::pFreeList->allocate ( size );
}

inline void dbPutNotifyBlocker::operator delete ( void *pCadaver, size_t size )
{
    dbPutNotifyBlocker::pFreeList->release ( pCadaver, size );
}

#endif // ifndef dbPutNotifyBlockerh

