
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
union osiSockAddr;

class cac;

class netiiu {
public:
    netiiu ( cac * );
    virtual ~netiiu ();
    void show ( unsigned level ) const;
    unsigned channelCount () const;
    void disconnectAllChan ( netiiu & newiiu );
    void connectTimeoutNotify ();
    bool searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel );
    void resetChannelRetryCounts ();
    void attachChannel ( nciu &chan );
    void detachChannel ( nciu &chan );
    int printf ( const char *pformat, ... );
    virtual void hostName (char *pBuf, unsigned bufLength) const;
    virtual const char * pHostName () const; // deprecated - please do not use
    virtual bool isVirtaulCircuit ( const char *pChannelName, const osiSockAddr &addr ) const;
    virtual bool ca_v42_ok () const;
    virtual bool ca_v41_ok () const;
    virtual bool pushDatagramMsg ( const caHdr &hdr, const void *pExt, ca_uint16_t extsize);
    virtual void writeRequest ( nciu &, unsigned type, unsigned nElem, const void *pValue );
    virtual void writeNotifyRequest ( nciu &, netWriteNotifyIO &, unsigned type, unsigned nElem, const void *pValue );
    virtual void readNotifyRequest ( nciu &, netReadNotifyIO &, unsigned type, unsigned nElem );
    virtual void createChannelRequest ( nciu & );
    virtual void connectAllIO ( nciu &chan );
    virtual void disconnectAllIO ( nciu &chan );
    virtual void clearChannelRequest ( nciu & );
    virtual void subscriptionRequest ( nciu &, netSubscription &subscr );
    virtual void subscriptionCancelRequest ( nciu &, netSubscription &subscr );
    virtual double beaconPeriod () const;
    virtual void flushRequest ();
    virtual bool flushBlockThreshold () const;
    virtual void flushRequestIfAboveEarlyThreshold ();
    virtual void blockUntilSendBacklogIsReasonable ( epicsMutex & );
protected:
    cac * pCAC () const;
private:
    tsDLList < nciu > channelList;
    cac *pClientCtx;
    virtual void lastChannelDetachNotify ();
};

class limboiiu : public netiiu {
public:
    limboiiu ();
};

extern limboiiu limboIIU;

inline cac * netiiu::pCAC () const
{
    return this->pClientCtx;
}

inline unsigned netiiu::channelCount () const
{
    return this->channelList.count ();
}

// cac lock must also be applied when calling this
inline void netiiu::attachChannel ( class nciu &chan )
{
    this->channelList.add ( chan );
}

// cac lock must also be applied when calling this
inline void netiiu::detachChannel ( class nciu &chan )
{
    this->channelList.remove ( chan );
    if ( this->channelList.count () == 0u ) {
        this->lastChannelDetachNotify ();
    }
}

#endif // netiiuh
