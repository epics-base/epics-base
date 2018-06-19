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

#ifndef nciuh
#define nciuh

#ifdef epicsExportSharedSymbols
#   define nciuh_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "resourceLib.h"
#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsMutex.h"
#include "compilerDependencies.h"

#ifdef nciuh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#define CA_MINOR_PROTOCOL_REVISION 13
#include "caProto.h"

#include "cacIO.h"

class cac;
class netiiu;

// The node and the state which tracks the list membership
// are in the channel, but belong to the circuit.
// Protected by the callback mutex
class channelNode : public tsDLNode < class nciu >
{
protected:
    channelNode ();
    bool isInstalledInServer ( epicsGuard < epicsMutex > & ) const;
    bool isConnected ( epicsGuard < epicsMutex > & ) const;
public:
    static unsigned getMaxSearchTimerCount ();
private:
    enum channelState {
        cs_none,
        cs_disconnGov,
        // note: indexing is used here
        // so these must be contiguous
        cs_searchReqPending0,
        cs_searchReqPending1,
        cs_searchReqPending2,
        cs_searchReqPending3,
        cs_searchReqPending4,
        cs_searchReqPending5,
        cs_searchReqPending6,
        cs_searchReqPending7,
        cs_searchReqPending8,
        cs_searchReqPending9,
        cs_searchReqPending10,
        cs_searchReqPending11,
        cs_searchReqPending12,
        cs_searchReqPending13,
        cs_searchReqPending14,
        cs_searchReqPending15,
        cs_searchReqPending16,
        cs_searchReqPending17,
        // note: indexing is used here
        // so these must be contiguous
        cs_searchRespPending0,
        cs_searchRespPending1,
        cs_searchRespPending2,
        cs_searchRespPending3,
        cs_searchRespPending4,
        cs_searchRespPending5,
        cs_searchRespPending6,
        cs_searchRespPending7,
        cs_searchRespPending8,
        cs_searchRespPending9,
        cs_searchRespPending10,
        cs_searchRespPending11,
        cs_searchRespPending12,
        cs_searchRespPending13,
        cs_searchRespPending14,
        cs_searchRespPending15,
        cs_searchRespPending16,
        cs_searchRespPending17,
        cs_createReqPend,
        cs_createRespPend,
        cs_v42ConnCallbackPend,
        cs_subscripReqPend,
        cs_connected,
        cs_unrespCircuit,
        cs_subscripUpdateReqPend
    } listMember;
    void setRespPendingState ( epicsGuard < epicsMutex > &, unsigned index );
    void setReqPendingState ( epicsGuard < epicsMutex > &, unsigned index );
    unsigned getSearchTimerIndex ( epicsGuard < epicsMutex > & );
    friend class tcpiiu;
    friend class udpiiu;
    friend class tcpSendThread;
    friend class searchTimer;
    friend class disconnectGovernorTimer;
};

class privateInterfaceForIO {
public:
    virtual void ioCompletionNotify (
        epicsGuard < epicsMutex > &, class baseNMIU & ) = 0;
    virtual arrayElementCount nativeElementCount (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual bool connected ( epicsGuard < epicsMutex > & ) const = 0;
protected:
    virtual ~privateInterfaceForIO() {}
};

class nciu :
    public cacChannel,
    public chronIntIdRes < nciu >,
    public channelNode,
    private privateInterfaceForIO {
public:
    nciu ( cac &, netiiu &, cacChannelNotify &,
        const char * pNameIn, cacChannel::priLev );
    ~nciu ();
    void connect ( unsigned nativeType,
        unsigned nativeCount, unsigned sid,
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void connect ( epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void unresponsiveCircuitNotify (
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void circuitHangupNotify ( class udpiiu &,
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void setServerAddressUnknown (
        netiiu & newiiu, epicsGuard < epicsMutex > & guard );
    bool searchMsg (
        epicsGuard < epicsMutex > & );
    void serviceShutdownNotify (
        epicsGuard < epicsMutex > & callbackControlGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void accessRightsStateChange ( const caAccessRights &,
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    ca_uint32_t getSID (
        epicsGuard < epicsMutex > & ) const;
    ca_uint32_t getCID (
        epicsGuard < epicsMutex > & ) const;
    netiiu * getPIIU (
        epicsGuard < epicsMutex > & );
    const netiiu * getConstPIIU (
        epicsGuard < epicsMutex > & ) const;
    cac & getClient ();
    void searchReplySetUp ( netiiu &iiu, unsigned sidIn,
        ca_uint16_t typeIn, arrayElementCount countIn,
        epicsGuard < epicsMutex > & );
    void show (
        unsigned level ) const;
    void show (
        epicsGuard < epicsMutex > &,
        unsigned level ) const;
    unsigned getName (
        epicsGuard < epicsMutex > &,
        char * pBuf, unsigned bufLen ) const throw ();
    const char * pName (
        epicsGuard < epicsMutex > & ) const throw ();
    unsigned nameLen (
        epicsGuard < epicsMutex > & ) const;
    unsigned getHostName (
        epicsGuard < epicsMutex > &,
        char * pBuf, unsigned bufLen ) const throw ();
    void writeException (
        epicsGuard < epicsMutex > &, epicsGuard < epicsMutex > &,
        int status, const char *pContext, unsigned type, arrayElementCount count );
    cacChannel::priLev getPriority (
        epicsGuard < epicsMutex > & ) const;
    void * operator new (
        size_t size, tsFreeList < class nciu, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (
        ( void *, tsFreeList < class nciu, 1024, epicsMutexNOOP > & ))
    //arrayElementCount nativeElementCount ( epicsGuard < epicsMutex > & ) const;
    void resubscribe ( epicsGuard < epicsMutex > & );
    void sendSubscriptionUpdateRequests ( epicsGuard < epicsMutex > & );
    void disconnectAllIO (
        epicsGuard < epicsMutex > &, epicsGuard < epicsMutex > & );
    bool connected ( epicsGuard < epicsMutex > & ) const;
    unsigned getcount() const { return count; }

private:
    tsDLList < class baseNMIU > eventq;
    caAccessRights accessRightState;
    cac & cacCtx;
    char * pNameStr;
    netiiu * piiu;
    ca_uint32_t sid; // server id
    unsigned count;
    unsigned retry; // search retry number
    unsigned short nameLength; // channel name length
    ca_uint16_t typeCode;
    ca_uint8_t priority;
    virtual void destroy (
        CallbackGuard & callbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void initiateConnect (
        epicsGuard < epicsMutex > & );
    unsigned requestMessageBytesPending (
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void flush (
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    ioStatus read (
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count,
        cacReadNotify &, ioid * );
    void write (
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count,
        const void *pValue );
    ioStatus write (
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count,
        const void *pValue, cacWriteNotify &, ioid * );
    void subscribe (
        epicsGuard < epicsMutex > & guard,
        unsigned type, arrayElementCount nElem,
        unsigned mask, cacStateNotify &notify, ioid * );
    // The primary mutex must be released when calling the user's
    // callback, and therefore a finite interval exists when we are
    // moving forward with the intent to call the users callback
    // but the users IO could be deleted during this interval.
    // To prevent the user's callback from being called after
    // destroying his IO we must past a guard for the callback
    // mutex here.
    virtual void ioCancel (
        CallbackGuard & callbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard,
        const ioid & );
    void ioShow (
        epicsGuard < epicsMutex > &,
        const ioid &, unsigned level ) const;
    short nativeType (
        epicsGuard < epicsMutex > & ) const;
    caAccessRights accessRights (
        epicsGuard < epicsMutex > & ) const;
    unsigned searchAttempts (
        epicsGuard < epicsMutex > & ) const;
    double beaconPeriod (
        epicsGuard < epicsMutex > & ) const;
    double receiveWatchdogDelay (
        epicsGuard < epicsMutex > & ) const;
    bool ca_v42_ok (
        epicsGuard < epicsMutex > & ) const;
    arrayElementCount nativeElementCount (
        epicsGuard < epicsMutex > & ) const;
    static void stringVerify ( const char *pStr, const unsigned count );
    void ioCompletionNotify (
        epicsGuard < epicsMutex > &, class baseNMIU & );
    const char * pHostName (
        epicsGuard < epicsMutex > & guard ) const throw ();
	nciu ( const nciu & );
	nciu & operator = ( const nciu & );
    void operator delete ( void * );
};

inline void * nciu::operator new ( size_t size,
    tsFreeList < class nciu, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void nciu::operator delete ( void * pCadaver,
    tsFreeList < class nciu, 1024, epicsMutexNOOP > & freeList )
{
    freeList.release ( pCadaver, sizeof ( nciu ) );
}
#endif

inline ca_uint32_t nciu::getSID (
    epicsGuard < epicsMutex > & ) const
{
    return this->sid;
}

inline ca_uint32_t nciu::getCID (
    epicsGuard < epicsMutex > & ) const
{
    return this->id;
}

// this is to only be used by early protocol revisions
inline void nciu::connect ( epicsGuard < epicsMutex > & cbGuard,
                epicsGuard < epicsMutex > & guard )
{
    this->connect ( this->typeCode, this->count,
        this->sid, cbGuard, guard );
}

inline void nciu::searchReplySetUp ( netiiu &iiu, unsigned sidIn,
    ca_uint16_t typeIn, arrayElementCount countIn,
    epicsGuard < epicsMutex > & )
{
    this->piiu = & iiu;
    this->typeCode = typeIn;
    this->count = countIn;
    this->sid = sidIn;
}

inline netiiu * nciu::getPIIU (
    epicsGuard < epicsMutex > & )
{
    return this->piiu;
}

inline void nciu::writeException (
    epicsGuard < epicsMutex > & /* cbGuard */,
    epicsGuard < epicsMutex > & guard,
    int status, const char * pContext,
    unsigned typeIn, arrayElementCount countIn )
{
    this->notify().writeException ( guard,
        status, pContext, typeIn, countIn );
}

inline const netiiu * nciu::getConstPIIU (
    epicsGuard < epicsMutex > & ) const
{
    return this->piiu;
}

inline cac & nciu::getClient ()
{
    return this->cacCtx;
}

inline cacChannel::priLev nciu::getPriority (
    epicsGuard < epicsMutex > & ) const
{
    return this->priority;
}

inline channelNode::channelNode () :
    listMember ( cs_none )
{
}

inline bool channelNode::isConnected ( epicsGuard < epicsMutex > & ) const
{
    return
        this->listMember == cs_connected ||
        this->listMember == cs_subscripReqPend ||
        this->listMember == cs_subscripUpdateReqPend;
}

inline bool channelNode::isInstalledInServer ( epicsGuard < epicsMutex > & ) const
{
    return
        this->listMember == cs_connected ||
        this->listMember == cs_subscripReqPend ||
        this->listMember == cs_unrespCircuit ||
        this->listMember == cs_subscripUpdateReqPend;
}

#endif // ifdef nciuh
