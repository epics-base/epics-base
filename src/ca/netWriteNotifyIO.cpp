
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

netWriteNotifyIO::netWriteNotifyIO ( nciu &chan, cacWriteNotify &notifyIn ) :
    baseNMIU ( chan ), notify ( notifyIn )
{
}

netWriteNotifyIO::~netWriteNotifyIO ()
{
}

void netWriteNotifyIO::show ( unsigned level ) const
{
    ::printf ( "read write notify IO at %p\n", 
        static_cast < const void * > ( this ) );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}

void netWriteNotifyIO::destroy ( cacRecycle & recycle )
{
    this->~netWriteNotifyIO ();
    recycle.recycleWriteNotifyIO ( *this );
}

void netWriteNotifyIO::completion ()
{
    this->notify.completion ();
}

void netWriteNotifyIO::exception ( int status, const char *pContext )
{
    this->notify.exception ( status, pContext, UINT_MAX, 0u );
}

void netWriteNotifyIO::exception ( int status, const char *pContext, 
                                  unsigned type, arrayElementCount count )
{
    this->notify.exception ( status, pContext, type, count );
}


void netWriteNotifyIO::completion ( unsigned /* type */, 
    arrayElementCount /* count */, const void * /* pData */ )
{
    this->chan.getClient().printf ( "Write response with data ?\n" );
}

// NOTE: The constructor for netWriteNotifyIO::netWriteNotifyIO() currently does
// not throw an exception, but we should eventually have placement delete
// defined for class netWriteNotifyIO when compilers support this so that 
// there is no possibility of a leak if there was an exception in
// a future version of netWriteNotifyIO::netWriteNotifyIO()
#if defined ( NETIO_PLACEMENT_DELETE )
    void netWriteNotifyIO::operator delete ( void *pCadaver, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > &freeList )
    {
        freeList.release ( pCadaver, sizeof ( netWriteNotifyIO ) );
    }
#endif

#   if defined (_MSC_VER) && _MSC_VER <= 1300
    void netWriteNotifyIO::operator delete ( void * ) // avoid visual c++ 7 bug
    {
        throw std::logic_error ( "_MSC_VER == 1300 bogus stub called?" );
    }
#   endif




