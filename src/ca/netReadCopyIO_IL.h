
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

#ifndef netReadCopyIO_ILh
#define netReadCopyIO_ILh

inline void * netReadCopyIO::operator new ( size_t size )
{
    return netReadCopyIO::freeList.allocate ( size );
}

inline void netReadCopyIO::operator delete ( void *pCadaver, size_t size )
{
    netReadCopyIO::freeList.release ( pCadaver, size );
}


#endif // netReadCopyIO_ILh
