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

#include <string>
#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "msgForMultiplyDefinedPV.h"
#include "cac.h"
#include "caerr.h" // for ECA_DBLCHNL

msgForMultiplyDefinedPV::msgForMultiplyDefinedPV ( 
    ipAddrToAsciiEngine & engine,
    callbackForMultiplyDefinedPV & cbIn, 
    const char * pChannelName, const char * pAcc ) :
    dnsTransaction ( engine.createTransaction () ), cb ( cbIn )
{
    strncpy ( this->acc, pAcc, sizeof ( this->acc ) );
    this->acc[ sizeof ( this->acc ) - 1 ] = '\0';
    strncpy ( this->channel, pChannelName, sizeof ( this->channel ) );
    this->channel[ sizeof ( this->channel ) - 1 ] = '\0';
}

msgForMultiplyDefinedPV::~msgForMultiplyDefinedPV ()
{
    this->dnsTransaction.release ();
}

void msgForMultiplyDefinedPV::transactionComplete ( const char * pHostNameRej )
{
    this->cb.pvMultiplyDefinedNotify ( *this, this->channel, this->acc, pHostNameRej );
    // !! dont touch this pointer after this point because object has been deleted !!
}

void * msgForMultiplyDefinedPV::operator new ( size_t size, 
    tsFreeList < class msgForMultiplyDefinedPV, 16 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void msgForMultiplyDefinedPV::operator delete ( void *pCadaver, 
    tsFreeList < class msgForMultiplyDefinedPV, 16 > & freeList ) 
{
    freeList.release ( pCadaver, sizeof ( msgForMultiplyDefinedPV ) );
}
#endif

void msgForMultiplyDefinedPV::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

callbackForMultiplyDefinedPV::~callbackForMultiplyDefinedPV ()
{
}


