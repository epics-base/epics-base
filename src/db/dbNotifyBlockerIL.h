
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

inline void * dbPutNotifyBlocker::operator new ( size_t size )
{
    epicsAutoMutex locker ( dbPutNotifyBlocker::freeListMutex );
    return dbPutNotifyBlocker::freeList.allocate ( size );
}

inline void dbPutNotifyBlocker::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( dbPutNotifyBlocker::freeListMutex );
    dbPutNotifyBlocker::freeList.release ( pCadaver, size );
}
