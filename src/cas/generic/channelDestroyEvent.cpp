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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */


#define epicsExportSharedSymbols
#include "channelDestroyEvent.h"
#include "casCoreClient.h"

caStatus channelDestroyEvent::cbFunc (  
    casCoreClient & client, 
    epicsGuard < casClientMutex > & clientGuard,
    epicsGuard < evSysMutex > & )
{
    caStatus status = client.channelDestroyEventNotify ( 
        clientGuard, this->pChan, this->sid );
    if ( status != S_cas_sendBlocked ) {
        delete this;
    }
    return status;
}

