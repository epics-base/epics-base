

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

inline void dbPutNotifyIO::destroy () 
{
    delete this;
}

inline void * dbPutNotifyIO::operator new ( size_t size )
{
    return dbPutNotifyIO::freeList.allocate ( size );
}

inline void dbPutNotifyIO::operator delete ( void *pCadaver, size_t size )
{
    dbPutNotifyIO::freeList.release ( pCadaver, size );
}

