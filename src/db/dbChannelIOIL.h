

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


inline unsigned long dbChannelIO::nativeElementCount () const 
{
    if ( this->addr.no_elements >= 0u ) {
        return static_cast < unsigned long > ( this->addr.no_elements );
    }
    else {
        return 0u;
    }
}

inline void dbChannelIO::destroy () 
{
    delete this;
}

inline void * dbChannelIO::operator new ( size_t size )
{
    return dbChannelIO::freeList.allocate ( size );
}

inline void dbChannelIO::operator delete ( void *pCadaver, size_t size )
{
    dbChannelIO::freeList.release ( pCadaver, size );
}

inline const char *dbChannelIO::pName () const 
{
    return addr.precord->name;
}

inline short dbChannelIO::nativeType () const 
{
    return dbDBRnewToDBRold[this->addr.field_type];
}

inline void dbChannelIO::subscriptionUpdate ( unsigned type, unsigned long count, 
        const struct db_field_log *pfl, cacNotifyIO &notify )
{
    this->serviceIO.subscriptionUpdate ( this->addr, type, count, pfl, notify );
}

inline void dbChannelIO::lock () const
{
    dbScanLock ( this->addr.precord );
}

inline void dbChannelIO::unlock () const
{
    dbScanUnlock ( this->addr.precord );
}

inline dbEventSubscription dbChannelIO::subscribe ( dbSubscriptionIO &subscr, unsigned mask )
{
    return this->serviceIO.subscribe ( this->addr, subscr, mask );
}
