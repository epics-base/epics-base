
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

#ifndef udpiiu_ILh
#define udpiiu_ILh

inline bool udpiiu::isCurrentThread () const
{
    return ( this->recvThreadId == epicsThreadGetIdSelf () );
}

inline unsigned udpiiu::getPort () const
{
    return this->localPort;
}

inline void udpiiu::fdCreateNotify ( CAFDHANDLER *func, void *pArg )
{
    ( *func ) ( pArg, this->sock, true );
}

inline void udpiiu::fdDestroyNotify ( CAFDHANDLER *func, void *pArg )
{
    ( *func ) ( pArg, this->sock, false );
}

#endif // udpiiu_ILh

