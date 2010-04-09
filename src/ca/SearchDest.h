/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef SearchDest_h
#define SearchDest_h

#include <osiSock.h>
#include <epicsTime.h>
#include <tsDLList.h>
#include "caProto.h"

class channelNode;
class epicsMutex;
template < class T > class epicsGuard;

struct SearchDest :
    public tsDLNode < SearchDest > {
    virtual ~SearchDest () {};
    struct Callback {
        virtual ~Callback () {};
        virtual void notify (
            const caHdr & msg, const void * pPayload,
            const osiSockAddr & addr, const epicsTime & ) = 0;
        virtual void show ( 
            epicsGuard < epicsMutex > &, unsigned level ) const = 0;
    };
    virtual void searchRequest ( epicsGuard < epicsMutex > &,
        const char * pbuf, size_t len ) = 0;
    virtual void show ( epicsGuard < epicsMutex > &, unsigned level ) const = 0;
};

#endif // SearchDest_h
