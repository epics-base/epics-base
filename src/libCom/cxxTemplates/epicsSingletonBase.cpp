/*
 *  $Id$
 *
 *  Author: Jeffrey O. Hill
 *
 */

#include <new>

#define epicsExportSharedSymbols
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsSingleton.h"

epicsMutex epicsSingletonBase::mutex;

epicsSingletonBase::epicsSingletonBase () : pSingleton ( 0 )
{
}

void * epicsSingletonBase::singletonPointer () const
{
    return this->pSingleton;
}

epicsSingletonBase::~epicsSingletonBase ()
{
}

void epicsSingletonBase::lockedFactory ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( ! this->pSingleton ) {
        this->pSingleton = this->factory ();
        if ( ! this->pSingleton ) {
            throw std::bad_alloc ();
        }
    }
}