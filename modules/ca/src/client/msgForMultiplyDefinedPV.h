/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
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
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 *  505 665 1831
 */

#ifndef INC_msgForMultiplyDefinedPV_H
#define INC_msgForMultiplyDefinedPV_H

#include "ipAddrToAsciiAsynchronous.h"
#include "tsFreeList.h"
#include "tsDLList.h"
#include "compilerDependencies.h"

class callbackForMultiplyDefinedPV {
public:
    virtual ~callbackForMultiplyDefinedPV () = 0;
    virtual void pvMultiplyDefinedNotify (
        class msgForMultiplyDefinedPV &, const char * pChannelName,
        const char * pAcc, const char * pRej ) = 0;
};

class msgForMultiplyDefinedPV :
        public ipAddrToAsciiCallBack,
        public tsDLNode < msgForMultiplyDefinedPV >  {
public:
    msgForMultiplyDefinedPV ( ipAddrToAsciiEngine & engine,
        callbackForMultiplyDefinedPV &, const char * pChannelName,
        const char * pAcc );
    virtual ~msgForMultiplyDefinedPV ();
    void ioInitiate ( const osiSockAddr46 & rej );
    void * operator new ( size_t size, tsFreeList < class msgForMultiplyDefinedPV, 16 > & );
    epicsPlacementDeleteOperator (( void *, tsFreeList < class msgForMultiplyDefinedPV, 16 > & ))
private:
    char acc[64];
    char channel[64];
    ipAddrToAsciiTransaction & dnsTransaction;
    callbackForMultiplyDefinedPV & cb;
    void transactionComplete ( const char * pHostName );
    msgForMultiplyDefinedPV ( const msgForMultiplyDefinedPV & );
    msgForMultiplyDefinedPV & operator = ( const msgForMultiplyDefinedPV & );
    void operator delete ( void * );
};

inline void msgForMultiplyDefinedPV::ioInitiate ( const osiSockAddr46 & rej )
{
    this->dnsTransaction.ipAddrToAscii ( rej, *this );
}

#endif // ifdef INC_msgForMultiplyDefinedPV_H

