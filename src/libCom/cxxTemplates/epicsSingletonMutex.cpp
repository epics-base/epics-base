
/*
 *  $Id$
 *
 *  Author: Jeff O. Hill
 *
 */

#define epicsExportSharedSymbols
#include "epicsSingleton.h"

class epicsShareClass epicsSingletonMutex {
public: 
    epicsSingletonMutex ();
    ~epicsSingletonMutex ();
    epicsMutex & get ();
private:
    epicsThreadOnceId onceFlag;
    epicsMutex * pMutex;
    static void once ( void * );
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

epicsMutex & epicsSingletonMutex::get ()
{
    epicsThreadOnce ( & this->onceFlag, epicsSingletonMutex::once, this );
    return * this->pMutex;
}

epicsMutex & epicsSingletonPrivateMutex ()
{
    static epicsSingletonMutex mutex;
    return mutex.get ();
}
