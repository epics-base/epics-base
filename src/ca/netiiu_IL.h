
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

inline netiiu::netiiu ( cac &cacIn ) : 
    cacRef ( cacIn )
{
}

inline netiiu::~netiiu ()
{
}

inline cac & netiiu::clientCtx () const
{
    return this->cacRef;
}

#endif // netiiu_ILh