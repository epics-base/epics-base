
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


inline void * netSubscription::operator new (size_t size)
{ 
    return netSubscription::freeList.allocate (size);
}

inline void netSubscription::operator delete (void *pCadaver, size_t size)
{ 
    netSubscription::freeList.release (pCadaver,size);
}

//
// we need to be careful about exporting a raw IO
// pointer because the IO object may be deleted 
// at any time when the channel disconnects or the
// IO completes
//
inline bool netSubscription::factory ( nciu &chan, chtype type, unsigned long count, 
    unsigned short mask, cacNotify &notify, unsigned &id )
{
    netSubscription *pIO = new netSubscription ( chan, type, count, mask, notify );
    if ( pIO ) {
        id = pIO->getId ();
        return true;
    }
    else {
        return false;
    }
}

#endif // netSubscription_ILh


