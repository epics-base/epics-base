/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  $Id$
 *
 *  Author: Jeff O. Hill
 *
 */

#define epicsExportSharedSymbols
#include "epicsSingleton.h"

extern "C" void epicsSingletonMutexOnce ( void * pParm );

class epicsShareClass epicsSingletonMutex {
public: 
    epicsSingletonMutex ();
    ~epicsSingletonMutex ();
    epicsMutex & get ();
private:
    epicsThreadOnceId onceFlag;
    epicsMutex * pMutex;
    static void once ( void * );
    friend void epicsSingletonMutexOnce ( void * pParm );
};

epicsSingletonMutex::epicsSingletonMutex () :
    onceFlag ( EPICS_THREAD_ONCE_INIT ), pMutex ( 0 )
{
}

epicsSingletonMutex::~epicsSingletonMutex ()
{
    delete this->pMutex;
}

void epicsSingletonMutex::once ( void * pParm )
{
    epicsSingletonMutex *pSM = static_cast < epicsSingletonMutex * > ( pParm );
    pSM->pMutex = new epicsMutex;
}

extern "C" void epicsSingletonMutexOnce ( void * pParm )
{
    epicsSingletonMutex::once ( pParm );
}

epicsMutex & epicsSingletonMutex::get ()
{
    epicsThreadOnce ( & this->onceFlag, epicsSingletonMutexOnce, this );
    return * this->pMutex;
}

epicsMutex & epicsSingletonPrivateMutex ()
{
    static epicsSingletonMutex mutex;
    return mutex.get ();
}
