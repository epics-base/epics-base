
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

#ifndef netReadNotifyIO_ILh
#define netReadNotifyIO_ILh

inline void * netReadNotifyIO::operator new ( size_t size )
{ 
    return netReadNotifyIO::freeList.allocate ( size );
}

inline void netReadNotifyIO::operator delete ( void *pCadaver, size_t size )
{ 
    netReadNotifyIO::freeList.release ( pCadaver, size );
}

#endif // netReadNotifyIO_ILh
