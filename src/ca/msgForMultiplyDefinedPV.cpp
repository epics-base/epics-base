
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "msgForMultiplyDefinedPV.h"
#include "cac.h"

#define epicsExportSharedSymbols
#include "caerr.h" // for ECA_DBLCHNL
#undef epicsExportSharedSymbols

epicsSingleton < tsFreeList < class msgForMultiplyDefinedPV, 16 > >
	msgForMultiplyDefinedPV::pFreeList;

msgForMultiplyDefinedPV::msgForMultiplyDefinedPV ( 
    callbackMutex & mutexIn, cac & cacRefIn, 
    const char * pChannelName, const char * pAcc, const osiSockAddr &rej ) :
    ipAddrToAsciiAsynchronous ( rej ), cacRef ( cacRefIn ),
    mutex ( mutexIn )
{
    strncpy ( this->acc, pAcc, sizeof ( this->acc ) );
    this->acc[ sizeof ( this->acc ) - 1 ] = '\0';
    strncpy ( this->channel, pChannelName, sizeof ( this->channel ) );
    this->channel[ sizeof ( this->channel ) - 1 ] = '\0';
}

void msgForMultiplyDefinedPV::ioCompletionNotify ( const char * pHostNameRej )
{
    char buf[256];
    sprintf ( buf, "Channel: \"%.64s\", Connecting to: %.64s, Ignored: %.64s",
            this->channel, this->acc, pHostNameRej );
    {
        epicsGuard < callbackMutex > cbGuard ( this->mutex );
        genLocalExcep ( cbGuard, this->cacRef, ECA_DBLCHNL, buf );
    }
    delete this;
}
