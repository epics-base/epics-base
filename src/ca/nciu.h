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

#ifdef epicsExportSharedSymbols
#define nciuh_restore_epicsExportSharedSymbols
#undef epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "resourceLib.h"
#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsMutex.h"
#include "epicsSingleton.h"

#ifdef nciuh_restore_epicsExportSharedSymbols
#define epicsExportSharedSymbols
#endif

#include "shareLib.h"

#define CA_MINOR_PROTOCOL_REVISION 10
#include "caProto.h"

#include "cacIO.h"

class cac;
class netiiu;

class cacPrivateListOfIO {
public:
	cacPrivateListOfIO ();
private:
    tsDLList < class baseNMIU > eventq;
    friend class cac;
	cacPrivateListOfIO ( const cacPrivateListOfIO & );
	cacPrivateListOfIO & operator = ( const cacPrivateListOfIO & );
};

class nciu : public cacChannel, public tsDLNode < nciu >,
    public chronIntIdRes < nciu >, public cacPrivateListOfIO {
public:
    nciu ( cac &, netiiu &, cacChannelNotify &, 
        const char *pNameIn, cacChannel::priLev );
    ~nciu (); // force pool allocation
    void connect ( unsigned nativeType, 
        unsigned nativeCount, unsigned sid, bool v41Ok );
    void connect ();
    void connectStateNotify () const;
    void accessRightsNotify () const;
    void disconnect ( netiiu &newiiu );
    bool searchMsg ( class udpiiu & iiu, unsigned short retrySeqNumber, 
        unsigned & retryNoForThisChannel );
    void createChannelRequest ( class tcpiiu & iiu );
    bool identifierEquivelence ( unsigned idToMatch );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void resetRetryCount ();
    unsigned short getRetrySeqNo () const;
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
    void notifyStateChangeFirstConnectInCountOfOutstandingIO ();
private:
    caAccessRights accessRightState;
    cac & cacCtx;
    char * pNameStr;
    netiiu * piiu;
    ca_uint32_t sid; // server id
    unsigned count;
    unsigned retry; // search retry number
    unsigned short retrySeqNo; // search retry seq number
    unsigned short nameLength; // channel name length
    ca_uint16_t typeCode;
    ca_uint8_t priority; 
    bool f_connected:1;
    bool f_previousConn:1; // T if connected in the past
    bool f_claimSent:1;
    bool f_firstConnectDecrementsOutstandingIO:1;
    bool f_connectTimeOutSeen:1;
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
    static void stringVerify ( const char *pStr, const unsigned count );
    static epicsSingleton < tsFreeList < class nciu, 1024 > > pFreeList;
	nciu ( const nciu & );
	nciu & operator = ( const nciu & );
};

inline void * nciu::operator new ( size_t size )
{ 
    return nciu::pFreeList->allocate ( size );
}

inline void nciu::operator delete ( void *pCadaver, size_t size )
{ 
    nciu::pFreeList->release ( pCadaver, size );
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

inline unsigned short nciu::getRetrySeqNo () const
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

inline cacPrivateListOfIO::cacPrivateListOfIO () 
{
}

#endif // ifdef nciuh
