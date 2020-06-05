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

#ifndef INC_repeaterClient_H
#define INC_repeaterClient_H

#include "tsDLList.h"
#include "tsFreeList.h"
#include "compilerDependencies.h"

#include "libCaAPI.h"

union osiSockAddr;

/*
 * one socket per client so we will get the ECONNREFUSED
 * error code (and then delete the client)
 */
class repeaterClient : public tsDLNode < repeaterClient > {
public:
    repeaterClient ( const osiSockAddr & from );
    ~repeaterClient ();
    bool connect ();
    bool sendConfirm ();
    bool sendMessage ( const void *pBuf, unsigned bufSize );
    bool verify ();
    bool identicalAddress ( const osiSockAddr &from );
    bool identicalPort ( const osiSockAddr &from );
    void * operator new ( size_t size,
        tsFreeList < repeaterClient, 0x20 > & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < repeaterClient, 0x20 > & ))
private:
    osiSockAddr from;
    SOCKET sock;
    unsigned short port () const;
    void operator delete ( void * );
};

#endif // ifndef INC_repeaterClient_H
