
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

#ifndef netSubscription_ILh
#define netSubscription_ILh

inline netSubscription * netSubscription::factory ( 
    tsFreeList < class netSubscription, 1024 > &freeList, 
    nciu &chan, unsigned type, unsigned long count, 
    unsigned mask, cacDataNotify &notify )
{
    return new ( freeList ) netSubscription ( chan, type, 
                                count, mask, notify );
}

inline void * netSubscription::operator new ( size_t size, 
    tsFreeList < class netSubscription, 1024 > &freeList )
{
    return freeList.allocate ( size );
}

// NOTE: The constructor for netSubscription::netSubscription() currently does
// not throw an exception, but we should eventually have placement delete
// defined for class netSubscription when compilers support this so that 
// there is no possibility of a leak if there was an exception in
// a future version of netSubscription::netSubscription()
#if ! defined ( NO_PLACEMENT_DELETE )
inline void netSubscription::operator delete ( void *pCadaver, size_t size, 
    tsFreeList < class netSubscription, 1024 > &freeList )
{
    freeList.release ( pCadaver, size );
}
#endif

inline unsigned long netSubscription::getCount () const
{
    unsigned long nativeCount = this->chan.nativeElementCount ();
    if ( this->count == 0u || this->count > nativeCount ) {
        return nativeCount;
    }
    else {
        return this->count;
    }
}

inline unsigned netSubscription::getType () const
{
    return this->type;
}

inline unsigned netSubscription::getMask () const
{
    return this->mask;
}

#endif // netSubscription_ILh


