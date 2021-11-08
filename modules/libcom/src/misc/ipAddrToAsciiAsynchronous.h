/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef ipAddrToAsciiAsynchronous_h
#define ipAddrToAsciiAsynchronous_h

#include "osiSock.h"
#include "libComAPI.h"

class LIBCOM_API ipAddrToAsciiCallBack {
public:
    virtual void transactionComplete ( const char * pHostName ) = 0;
    virtual void show ( unsigned level ) const;
    virtual ~ipAddrToAsciiCallBack () = 0;
};

class LIBCOM_API ipAddrToAsciiTransaction {
public:
    virtual void release () = 0;
    virtual void ipAddrToAscii ( const osiSockAddr46 &, ipAddrToAsciiCallBack & ) = 0;
    virtual osiSockAddr46 address () const  = 0;
    virtual void show ( unsigned level ) const = 0;
protected:
    virtual ~ipAddrToAsciiTransaction () = 0;
};

class LIBCOM_API ipAddrToAsciiEngine {
public:
    virtual void release () = 0;
    virtual ipAddrToAsciiTransaction & createTransaction () = 0;
    virtual void show ( unsigned level ) const = 0;
    static ipAddrToAsciiEngine & allocate ();
protected:
    virtual ~ipAddrToAsciiEngine () = 0;
public:
#ifdef EPICS_PRIVATE_API
    static void cleanup();
#endif
};

#endif // ifdef ipAddrToAsciiAsynchronous_h
