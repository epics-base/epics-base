

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
    return 0u;
}

inline void dbChannelIO::destroy () 
{
    delete this;
}

inline void * dbChannelIO::operator new ( size_t size )
{
    return dbChannelIO::pFreeList->allocate ( size );
}

inline void dbChannelIO::operator delete ( void *pCadaver, size_t size )
{
    dbChannelIO::pFreeList->release ( pCadaver, size );
}

inline const char *dbChannelIO::pName () const 
{
    return addr.precord->name;
}

inline short dbChannelIO::nativeType () const 
{
    return this->addr.dbr_field_type;
}

inline void dbChannelIO::callReadNotify ( unsigned type, unsigned long count, 
        cacReadNotify &notify )
{
    this->serviceIO.callReadNotify ( this->addr, type, count, notify );
}

inline void dbChannelIO::callStateNotify ( unsigned type, unsigned long count, 
        const struct db_field_log *pfl, cacStateNotify &notify )
{
    this->serviceIO.callStateNotify ( this->addr, type, count, pfl, notify );
}

