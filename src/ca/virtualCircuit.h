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

#ifndef virtualCircuith  
#define virtualCircuith

#include "epicsMemory.h"
#include "tsSLList.h"
#include "tsDLList.h"

#include "comBuf.h"
#include "caServerID.h"
#include "netiiu.h"
#include "comQueSend.h"
#include "comQueRecv.h"
#include "tcpRecvWatchdog.h"
#include "tcpSendWatchdog.h"

enum iiu_conn_state { iiu_connecting, iiu_connected, iiu_disconnected };

extern "C" void cacSendThreadTCP ( void *pParam );
extern "C" void cacRecvThreadTCP ( void *pParam );

// a modified ca header with capacity for large arrays
struct  caHdrLargeArray {
    ca_uint32_t m_postsize;     // size of message extension 
    ca_uint32_t m_count;        // operation data count 
    ca_uint32_t m_cid;          // channel identifier 
    ca_uint32_t m_available;    // protocol stub dependent
    ca_uint16_t m_dataType;     // operation data type 
    ca_uint16_t m_cmmd;         // operation to be performed 
};

class hostNameCache;
class ipAddrToAsciiEngine;

class tcpiiu : 
        public netiiu, public tsDLNode < tcpiiu >,
        public tsSLNode < tcpiiu >, public caServerID, 
        private wireSendAdapter, private wireRecvAdapter {
public:
    tcpiiu ( cac &cac, double connectionTimeout, 
        epicsTimerQueue &timerQueue, const osiSockAddr &addrIn, 
        unsigned minorVersion, ipAddrToAsciiEngine & engineIn,
        const cacChannel::priLev & priorityIn );
    ~tcpiiu ();
    bool start ();
    void cleanShutdown ();
    void beaconAnomalyNotify ();
    void beaconArrivalNotify ();
    void forcedShutdown ();

    void flushRequest ();
    bool flushBlockThreshold () const;
    void flushRequestIfAboveEarlyThreshold ();
    void blockUntilSendBacklogIsReasonable 
        ( epicsMutex * pCallBack, epicsMutex & primary );
    virtual void show ( unsigned level ) const;
    bool setEchoRequestPending ();
    void requestRecvProcessPostponedFlush ();
    void clearChannelRequest ( ca_uint32_t sid, ca_uint32_t cid );

    bool ca_v41_ok () const;
    bool ca_v42_ok () const;
    bool ca_v44_ok () const;
    bool ca_v49_ok () const;

    void hostName ( char *pBuf, unsigned bufLength ) const;
    const char * pHostName () const; // deprecated - please do not use
    bool isVirtualCircuit ( const char *pChannelName, const osiSockAddr &addr ) const;
    bool alive () const;
    osiSockAddr getNetworkAddress () const;

private:
    tcpRecvWatchdog recvDog;
    tcpSendWatchdog sendDog;
    comQueSend sendQue;
    comQueRecv recvQue;
    caHdrLargeArray curMsg;
    arrayElementCount curDataMax;
    arrayElementCount curDataBytes;
    epics_auto_ptr < hostNameCache > pHostNameCache;
    char * pCurData;
    unsigned minorProtocolVersion;
    iiu_conn_state state;
    epicsEvent sendThreadFlushEvent;
    epicsEvent sendThreadExitEvent;
    epicsEvent flushBlockEvent;
    SOCKET sock;
    unsigned contigRecvMsgCount;
    unsigned blockingForFlush;
    unsigned socketLibrarySendBufferSize;
    unsigned unacknowledgedSendBytes;
    bool busyStateDetected; // only modified by the recv thread
    bool flowControlActive; // only modified by the send process thread
    bool echoRequestPending; 
    bool oldMsgHeaderAvailable;
    bool msgHeaderAvailable;
    bool sockCloseCompleted;
    bool earlyFlush;
    bool recvProcessPostponedFlush;

    void shutdown ( bool discardPendingMessages );
    void stopThreads ();
    bool processIncoming ();
    unsigned sendBytes ( const void *pBuf, unsigned nBytesInBuf );
    unsigned recvBytes ( void *pBuf, unsigned nBytesInBuf );
    void lastChannelDetachNotify ();
    void connect ();

    // send protocol stubs
    void echoRequest ();
    void versionMessage ( const cacChannel::priLev & priority );
    void disableFlowControlRequest ();
    void enableFlowControlRequest ();
    void hostNameSetRequest ();
    void userNameSetRequest ();
    void writeRequest ( nciu &, unsigned type, unsigned nElem, const void *pValue );
    void writeNotifyRequest ( nciu &, netWriteNotifyIO &, unsigned type, unsigned nElem, const void *pValue );
    void readNotifyRequest ( nciu &, netReadNotifyIO &, unsigned type, unsigned nElem );
    void createChannelRequest ( nciu & );
    void subscriptionRequest ( nciu &, netSubscription & subscr );
    void subscriptionCancelRequest (  nciu & chan, netSubscription & subscr );
    void flushIfRecvProcessRequested ();
    bool flush (); // only to be called by the send thread

    friend void cacSendThreadTCP ( void *pParam );
    friend void cacRecvThreadTCP ( void *pParam );

	tcpiiu ( const tcpiiu & );
	tcpiiu & operator = ( const tcpiiu & );
};

inline void tcpiiu::flushRequest ()
{
    this->sendThreadFlushEvent.signal ();
}

inline bool tcpiiu::ca_v41_ok () const
{
    return CA_V41 ( this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v44_ok () const
{
    return CA_V44 ( this->minorProtocolVersion );
}

inline bool tcpiiu::ca_v49_ok () const
{
    return CA_V49 ( this->minorProtocolVersion );
}

inline bool tcpiiu::alive () const // X aCC 361
{
    if ( this->state == iiu_connecting || 
        this->state == iiu_connected ) {
        return true;
    }
    else {
        return false;
    }
}

inline void tcpiiu::beaconAnomalyNotify ()
{
    this->recvDog.beaconAnomalyNotify ();
}

inline void tcpiiu::beaconArrivalNotify ()
{
    this->recvDog.beaconArrivalNotify ();
}

inline void tcpiiu::flushIfRecvProcessRequested ()
{
    if ( this->recvProcessPostponedFlush ) {
        this->flushRequest ();
        this->recvProcessPostponedFlush = false;
    }
}

#endif // ifdef virtualCircuith

