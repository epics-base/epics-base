
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

#ifndef netiiu_ILh
#define netiiu_ILh

inline cac * netiiu::pCAC () const
{
    return this->pClientCtx;
}

inline unsigned netiiu::channelCount () const
{
    return this->channelList.count ();
}

// cac lock must also be applied when
// calling this
inline void netiiu::attachChannel ( nciu &chan )
{
    this->channelList.add ( chan );
}

// cac lock must also be applied when
// calling this
inline void netiiu::detachChannel ( nciu &chan )
{
    this->channelList.remove ( chan );
    if ( this->channelList.count () == 0u ) {
        this->lastChannelDetachNotify ();
    }
}

#endif // netiiu_ILh
