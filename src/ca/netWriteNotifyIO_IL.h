
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

inline netWriteNotifyIO * netWriteNotifyIO::factory ( 
    tsFreeList < class netWriteNotifyIO, 1024 > &freeList, 
    nciu &chan, cacNotify &notify )
{
    return new ( freeList ) netWriteNotifyIO ( chan, notify );
}

inline void * netWriteNotifyIO::operator new ( size_t size, 
        tsFreeList < class netWriteNotifyIO, 1024 > &freeList )
{ 
    return freeList.allocate ( size );
}

// NOTE: The constructor for netWriteNotifyIO::netWriteNotifyIO() currently does
// not throw an exception, but we should eventually have placement delete
// defined for class netWriteNotifyIO when compilers support this so that 
// there is no possibility of a leak if there was an exception in
// a future version of netWriteNotifyIO::netWriteNotifyIO()
#if ! defined ( NO_PLACEMENT_DELETE )
inline void netWriteNotifyIO::operator delete ( void *pCadaver, size_t size, 
    tsFreeList < class netWriteNotifyIO, 1024 > &freeList )
{
    freeList.release ( pCadaver, size );
}
#endif
