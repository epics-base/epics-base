
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

#ifndef nciuh  
#define nciuh

#include "resourceLib.h"
#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsMutex.h"

#define CA_MINOR_PROTOCOL_REVISION 9
#include "caProto.h"

#define epicsExportSharedSymbols
#include "cacIO.h"
#undef epicsExportSharedSymbols

class cac;
class netiiu;

class cacPrivateListOfIO {
private:
    tsDLList < class baseNMIU > eventq;
    friend class cac;
};

class nciu : public cacChannel, public tsDLNode < nciu >,
    public chronIntIdRes < nciu >, public cacPrivateListOfIO {
public:
    nciu ( cac &, netiiu &, cacChannelNotify &, 
        const char *pNameIn, cacChannel::priLev );
    void connect ( unsigned nativeType, 
        unsigned nativeCount, unsigned sid, bool v41Ok );
    void connect ();
    void connectStateNotify () const;
    void accessRightsNotify () const;
    void disconnect ( netiiu &newiiu );
    bool searchMsg ( unsigned short retrySeqNumber, 
        unsigned &retryNoForThisChannel );
    void createChannelRequest ();
    bool identifierEquivelence ( unsigned idToMatch );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void resetRetryCount ();
    unsigned getRetrySeqNo () const;
    void accessRightsStateChange ( const caAccessRights & );
    ca_uint32_t getSID () const;
    ca_uint32_t getCID () const;
    netiiu * getPIIU ();
    const netiiu * getConstPIIU () const;
    cac & getClient ();
    int printf ( const char *pFormat, ... );
    void searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
        ca_uint16_t typeIn, arrayElementCount countIn );
    void show ( unsigned level ) const;
    void connectTimeoutNotify ();
    const char *pName () const;
    unsigned nameLen () const;
    const char * pHostName () const; // deprecated - please do not use
    arrayElementCount nativeElementCount () const;
    bool connected () const;
    bool previouslyConnected () const;
    void writeException ( int status, const char *pContext, unsigned type, arrayElementCount count );
    cacChannel::priLev getPriority () const;
protected:
    ~nciu (); // force pool allocation
private:
    caAccessRights accessRightState;
    cac & cacCtx;
    char * pNameStr;
    netiiu * piiu;
    ca_uint32_t sid; // server id
    unsigned count;
    unsigned retry; // search retry number
    ca_uint16_t retrySeqNo; // search retry seq number
    ca_uint16_t nameLength; // channel name length
    ca_uint16_t typeCode;
    ca_uint8_t priority; 
    unsigned f_connected:1;
    unsigned f_previousConn:1; // T if connected in the past
    unsigned f_claimSent:1;
    unsigned f_firstConnectDecrementsOutstandingIO:1;
    unsigned f_connectTimeOutSeen:1;
    void initiateConnect ();
    ioStatus read ( unsigned type, arrayElementCount count, 
        cacReadNotify &, ioid * );
    void write ( unsigned type, arrayElementCount count, 
        const void *pValue );
    ioStatus write ( unsigned type, arrayElementCount count, 
        const void *pValue, cacWriteNotify &, ioid * );
    void subscribe ( unsigned type, arrayElementCount nElem, 
        unsigned mask, cacStateNotify &notify, ioid * );
    void ioCancel ( const ioid & );
    void ioShow ( const ioid &, unsigned level ) const;
    short nativeType () const;
    caAccessRights accessRights () const;
    unsigned searchAttempts () const;
    double beaconPeriod () const;
    bool ca_v42_ok () const;
    void hostName ( char *pBuf, unsigned bufLength ) const;
    void notifyStateChangeFirstConnectInCountOfOutstandingIO ();
    static void stringVerify ( const char *pStr, const unsigned count );
    static tsFreeList < class nciu, 1024 > freeList;
    static epicsMutex freeListMutex;
};

inline void * nciu::operator new ( size_t size )
{ 
    epicsAutoMutex locker ( nciu::freeListMutex );
    return nciu::freeList.allocate ( size );
}

inline void nciu::operator delete ( void *pCadaver, size_t size )
{ 
    epicsAutoMutex locker ( nciu::freeListMutex );
    nciu::freeList.release ( pCadaver, size );
}

inline bool nciu::identifierEquivelence ( unsigned idToMatch )
{
    return idToMatch == this->id;
}

inline void nciu::resetRetryCount () 
{
    this->retry = 0u;
}

inline void nciu::accessRightsStateChange ( const caAccessRights &arIn )
{
    this->accessRightState = arIn;
}

inline ca_uint32_t nciu::getSID () const
{
    return this->sid;
}

inline ca_uint32_t nciu::getCID () const
{
    return this->id;
}

inline unsigned nciu::getRetrySeqNo () const
{
    return this->retrySeqNo;
}

// this is to only be used by early protocol revisions
inline void nciu::connect ()
{
    this->connect ( this->typeCode, this->count, this->sid, false );
}

inline void nciu::searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
    ca_uint16_t typeIn, arrayElementCount countIn )
{
    this->piiu = &iiu;
    this->typeCode = typeIn;      
    this->count = countIn;
    this->sid = sidIn;
}

inline bool nciu::connected () const
{
    return this->f_connected;
}

inline bool nciu::previouslyConnected () const
{
    return this->f_previousConn;
}

inline netiiu * nciu::getPIIU ()
{
    return this->piiu;
}

inline const netiiu * nciu::getConstPIIU () const
{
    return this->piiu;
}

inline cac & nciu::getClient ()
{
    return this->cacCtx;
}

inline void nciu::connectTimeoutNotify ()
{
    this->f_connectTimeOutSeen = true;
}

inline void nciu::writeException ( int status,
    const char *pContext, unsigned typeIn, arrayElementCount countIn )
{
    this->notify().writeException ( status, pContext, typeIn, countIn );
}

inline void nciu::connectStateNotify () const
{
    if ( this->f_connected ) {
        this->notify().connectNotify ();
    }
    else {
        this->notify().disconnectNotify ();
    }
}

inline void nciu::accessRightsNotify () const
{
    this->notify().accessRightsNotify ( this->accessRightState );
}

inline cacChannel::priLev nciu::getPriority () const
{
    return this->priority;
}

#endif // ifdef nciuh
