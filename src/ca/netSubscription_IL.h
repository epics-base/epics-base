
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

inline void * netSubscription::operator new ( size_t size )
{ 
    return netSubscription::freeList.allocate ( size );
}

inline void netSubscription::operator delete ( void *pCadaver, size_t size )
{ 
    netSubscription::freeList.release ( pCadaver, size );
}

inline unsigned long netSubscription::getCount () const
{
    if ( this->chan.connected () ) {
        unsigned long nativeCount = chan.nativeElementCount ();
        if ( this->count == 0u ) {
            return nativeCount;
        }
        else if ( this->count > nativeCount ) {
            return nativeCount;
        }
        else {
            return this->count;
        }
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


