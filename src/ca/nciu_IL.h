
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


//
// nciu inline member functions
//

inline void * nciu::operator new ( size_t size )
{ 
    return nciu::freeList.allocate ( size );
}

inline void nciu::operator delete ( void *pCadaver, size_t size )
{ 
    nciu::freeList.release ( pCadaver, size );
}

inline bool nciu::fullyConstructed () const
{
    return this->f_fullyConstructed;
}
