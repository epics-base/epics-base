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

#ifndef netiiuh
#define netiiuh

#include "tsDLList.h"

#include "nciu.h"

class netWriteNotifyIO;
class netReadNotifyIO;
class netSubscription;
class cacMutex;
union osiSockAddr;

class cac;
class callbackMutex;

class netiiu {
public:
    virtual ~netiiu ();
    virtual void hostName ( char *pBuf, unsigned bufLength ) const = 0;
    virtual const char * pHostName () const = 0; // deprecated - please do not use
    virtual bool ca_v42_ok () const = 0;
    virtual void writeRequest ( epicsGuard < cacMutex > &, nciu &, 
                    unsigned type, unsigned nElem, const void *pValue ) = 0;
    virtual void writeNotifyRequest ( epicsGuard < cacMutex > &, 
                    nciu &, netWriteNotifyIO &, 
                    unsigned type, unsigned nElem, const void *pValue ) = 0;
    virtual void readNotifyRequest ( epicsGuard < cacMutex > &, nciu &, 
                    netReadNotifyIO &, unsigned type, unsigned nElem ) = 0;
    virtual void clearChannelRequest ( epicsGuard < cacMutex > &, 
                    ca_uint32_t sid, ca_uint32_t cid ) = 0;
    virtual void subscriptionRequest ( epicsGuard < cacMutex > &, 
                    nciu &, netSubscription &subscr ) = 0;
    virtual void subscriptionCancelRequest ( epicsGuard < cacMutex > &, 
                    nciu & chan, netSubscription & subscr ) = 0;
    virtual void flushRequest () = 0;
    virtual bool flushBlockThreshold ( epicsGuard < cacMutex > & ) const = 0;
    virtual void flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & ) = 0;
    virtual void blockUntilSendBacklogIsReasonable 
        ( cacNotify &, epicsGuard < cacMutex > & ) = 0;
    virtual void requestRecvProcessPostponedFlush () = 0;
    virtual osiSockAddr getNetworkAddress () const = 0;
    virtual void uninstallChan ( epicsGuard < cacMutex > &, nciu & ) = 0;
};

class limboiiu : public netiiu { // X aCC 655
public:
    limboiiu ();
private:
    void hostName ( char *pBuf, unsigned bufLength ) const;
    const char * pHostName () const; // deprecated - please do not use
    bool ca_v42_ok () const;
    void writeRequest ( epicsGuard < cacMutex > &, nciu &, 
                    unsigned type, unsigned nElem, const void *pValue );
    void writeNotifyRequest ( epicsGuard < cacMutex > &, nciu &, netWriteNotifyIO &, 
                    unsigned type, unsigned nElem, const void *pValue );
    void readNotifyRequest ( epicsGuard < cacMutex > &, nciu &, netReadNotifyIO &, 
                    unsigned type, unsigned nElem );
    void clearChannelRequest ( epicsGuard < cacMutex > &, 
                    ca_uint32_t sid, ca_uint32_t cid );
    void subscriptionRequest ( epicsGuard < cacMutex > &, nciu &, 
                    netSubscription &subscr );
    void subscriptionCancelRequest ( epicsGuard < cacMutex > &, 
                    nciu & chan, netSubscription & subscr );
    void flushRequest ();
    bool flushBlockThreshold ( epicsGuard < cacMutex > & ) const;
    void flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & );
    void blockUntilSendBacklogIsReasonable 
        ( cacNotify &, epicsGuard < cacMutex > & );
    void requestRecvProcessPostponedFlush ();
    osiSockAddr getNetworkAddress () const;
    void uninstallChan ( epicsGuard < cacMutex > &, nciu & );
	limboiiu ( const limboiiu & );
	limboiiu & operator = ( const limboiiu & );
};

#endif // netiiuh
