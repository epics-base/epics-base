
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "nciu.h"
#include "cac.h"

netReadNotifyIO::netReadNotifyIO ( nciu & chan, cacReadNotify & notify ) :
    baseNMIU ( chan ), notify ( notify )
{
}

netReadNotifyIO::~netReadNotifyIO ()
{
}

void netReadNotifyIO::show ( unsigned level ) const
{
    ::printf ( "read notify IO at %p\n", 
        static_cast < const void * > ( this ) );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}

void netReadNotifyIO::destroy ( cacRecycle & recycle )
{
    this->~netReadNotifyIO();
    recycle.recycleReadNotifyIO ( *this );
}

void netReadNotifyIO::completion ()
{
    this->chan.getClient().printf ( "Read response w/o data ?\n" );
}

void netReadNotifyIO::exception ( int status, const char *pContext )
{
    this->notify.exception ( status, pContext, UINT_MAX, 0u );
}

void netReadNotifyIO::exception ( int status, const char *pContext, 
                                 unsigned type, arrayElementCount count )
{
    this->notify.exception ( status, pContext, type, count );
}

void netReadNotifyIO::completion ( unsigned type, 
    arrayElementCount count, const void *pData )
{
    this->notify.completion ( type, count, pData );
}

// NOTE: The constructor for netReadNotifyIO::netReadNotifyIO() currently does
// not throw an exception, but we should eventually have placement delete
// defined for class netReadNotifyIO when compilers support this so that 
// there is no possibility of a leak if there was an exception in
// a future version of netReadNotifyIO::netReadNotifyIO()
#if defined ( NETIO_PLACEMENT_DELETE )
    void netReadNotifyIO::operator delete ( void *pCadaver, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & freeList ) {
        freeList.release ( pCadaver, sizeof ( netReadNotifyIO ) );
    }
#endif

#   if defined (_MSC_VER) && _MSC_VER == 1300
    void netReadNotifyIO::operator delete ( void * ) // avoid visual c++ 7 bug
    {
        throw std::logic_error ( "_MSC_VER == 1300 bogus stub called?" );
    }
#   endif



