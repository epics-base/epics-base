
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
// netWriteNotifyIO inline member functions
//
inline void * netWriteNotifyIO::operator new ( size_t size )
{ 
    return netWriteNotifyIO::freeList.allocate ( size );
}

inline void netWriteNotifyIO::operator delete ( void *pCadaver, size_t size )
{ 
    netWriteNotifyIO::freeList.release ( pCadaver, size );
}

//
// we need to be careful about exporting a raw IO
// pointer because the IO object may be deleted 
// at any time when the channel disconnects or the
// IO completes
//
//inline bool netWriteNotifyIO::factory ( nciu &chan, cacNotify &notify, ca_uint32_t &id )
//{
//    netWriteNotifyIO *pIO = new netWriteNotifyIO ( chan, notify );
//    if ( pIO ) {
//       id = pIO->getId ();
//        return true;
//    }
//    else {
//        return false;
//    }
//}


