
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

//
// we need to be careful about exporting a raw IO
// pointer because the IO object may be deleted 
// at any time when the channel disconnects or the
// IO completes
//
inline bool netReadCopyIO::factory ( nciu &chan, unsigned type, 
    unsigned long count, void *pValue, unsigned seqNumber, unsigned &id )
{
    netReadCopyIO *pIO = new netReadCopyIO ( chan, 
        type, count, pValue, seqNumber );
    if ( pIO ) {
        id = pIO->getId ();
        return true;
    }
    else {
        return false;
    }
}

inline void * netReadCopyIO::operator new ( size_t size )
{
    return netReadCopyIO::freeList.allocate ( size );
}

inline void netReadCopyIO::operator delete ( void *pCadaver, size_t size )
{
    netReadCopyIO::freeList.release ( pCadaver, size );
}


#endif // netReadCopyIO_ILh
