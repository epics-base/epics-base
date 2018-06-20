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

#ifndef netiiuh
#define netiiuh

#include "cacIO.h"
#include "caProto.h"

class netWriteNotifyIO;
class netReadNotifyIO;
class netSubscription;
union osiSockAddr;
class cac;
class nciu;

class netiiu {
public:
    virtual ~netiiu () = 0;
    virtual unsigned getHostName ( 
        epicsGuard < epicsMutex > &, char * pBuf, 
        unsigned bufLength ) const throw () = 0;
    virtual const char * pHostName (
        epicsGuard < epicsMutex > & ) const throw () = 0;
    virtual bool ca_v41_ok (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual bool ca_v42_ok (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual unsigned requestMessageBytesPending ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard ) = 0;
    virtual void flush ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard ) = 0;
    virtual void writeRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        unsigned type, arrayElementCount nElem, 
        const void *pValue ) = 0;
    virtual void writeNotifyRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netWriteNotifyIO &, 
        unsigned type, arrayElementCount nElem, 
        const void *pValue ) = 0;
    virtual void readNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        netReadNotifyIO &, unsigned type, 
        arrayElementCount nElem ) = 0;
    virtual void clearChannelRequest ( 
        epicsGuard < epicsMutex > &, 
        ca_uint32_t sid, ca_uint32_t cid ) = 0;
    virtual void subscriptionRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netSubscription & ) = 0;
    virtual void subscriptionUpdateRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netSubscription & ) = 0;
    virtual void subscriptionCancelRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu & chan, netSubscription & subscr ) = 0;
    virtual void flushRequest ( 
        epicsGuard < epicsMutex > & ) = 0;
    virtual void requestRecvProcessPostponedFlush (
        epicsGuard < epicsMutex > & ) = 0;
    virtual osiSockAddr getNetworkAddress (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual void uninstallChan ( 
        epicsGuard < epicsMutex > &, nciu & ) = 0;
    virtual void uninstallChanDueToSuccessfulSearchResponse ( 
        epicsGuard < epicsMutex > &, nciu &, 
        const class epicsTime & currentTime ) = 0;
    virtual double receiveWatchdogDelay (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual bool searchMsg (
        epicsGuard < epicsMutex > &, ca_uint32_t id, 
            const char * pName, unsigned nameLength ) = 0;
};

#endif // netiiuh
