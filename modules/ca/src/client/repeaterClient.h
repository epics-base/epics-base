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

#ifndef repeaterClienth
#define repeaterClienth

#ifdef epicsExportSharedSymbols
#   define repeaterClienth_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "tsDLList.h"
#include "tsFreeList.h"
#include "compilerDependencies.h"

#ifdef repeaterClienth_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

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

#endif // repeaterClienth


