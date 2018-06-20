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

#ifndef noopiiuh
#define noopiiuh

#include "netiiu.h"

class noopiiu : public netiiu {
public:
    ~noopiiu ();
    unsigned getHostName ( 
        epicsGuard < epicsMutex > &, char * pBuf, 
        unsigned bufLength ) const throw ();
    const char * pHostName (
        epicsGuard < epicsMutex > & ) const throw (); 
    bool ca_v41_ok (
        epicsGuard < epicsMutex > & ) const;
    bool ca_v42_ok (
        epicsGuard < epicsMutex > & ) const;
    unsigned requestMessageBytesPending ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void flush ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void writeRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        unsigned type, arrayElementCount nElem, 
        const void *pValue );
    void writeNotifyRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netWriteNotifyIO &, 
        unsigned type, arrayElementCount nElem, 
        const void *pValue );
    void readNotifyRequest ( 
        epicsGuard < epicsMutex > &, nciu &, 
        netReadNotifyIO &, unsigned type, 
        arrayElementCount nElem );
    void clearChannelRequest ( 
        epicsGuard < epicsMutex > &, 
        ca_uint32_t sid, ca_uint32_t cid );
    void subscriptionRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netSubscription & );
    void subscriptionUpdateRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu &, netSubscription & );
    void subscriptionCancelRequest ( 
        epicsGuard < epicsMutex > &, 
        nciu & chan, netSubscription & subscr );
    void flushRequest ( 
        epicsGuard < epicsMutex > & );
    void requestRecvProcessPostponedFlush (
        epicsGuard < epicsMutex > & );
    osiSockAddr getNetworkAddress (
        epicsGuard < epicsMutex > & ) const;
    void uninstallChan ( 
        epicsGuard < epicsMutex > & mutex, 
        nciu & );
    void uninstallChanDueToSuccessfulSearchResponse ( 
        epicsGuard < epicsMutex > &, nciu &, 
        const class epicsTime & currentTime );
    double receiveWatchdogDelay (
        epicsGuard < epicsMutex > & ) const;
    bool searchMsg (
        epicsGuard < epicsMutex > &, ca_uint32_t id, 
            const char * pName, unsigned nameLength );
};

extern noopiiu noopIIU;

#endif // ifndef noopiiuh
