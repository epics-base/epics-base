
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
    nciu ( cac &, netiiu &, 
        cacChannelNotify &, const char *pNameIn );
    void connect ( unsigned nativeType, 
        unsigned long nativeCount, unsigned sid );
    void connect ();
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
    cac & getClient ();
    void searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
        unsigned typeIn, unsigned long countIn );
    void show ( unsigned level ) const;
    void connectTimeoutNotify ();
    const char *pName () const;
    unsigned nameLen () const;
    const char * pHostName () const; // deprecated - please do not use
    unsigned long nativeElementCount () const;
    bool connected () const;
    bool previouslyConnected () const;
protected:
    ~nciu (); // force pool allocation
private:
    cac &cacCtx;
    caAccessRights accessRightState;
    unsigned count;
    char *pNameStr;
    netiiu *piiu;
    ca_uint32_t sid; // server id
    unsigned retry; // search retry number
    unsigned short retrySeqNo; // search retry seq number
    unsigned short nameLength; // channel name length
    unsigned short typeCode;
    unsigned f_connected:1;
    unsigned f_previousConn:1; // T if connected in the past
    unsigned f_claimSent:1;
    unsigned f_firstConnectDecrementsOutstandingIO:1;
    unsigned f_connectTimeOutSeen:1;
    void initiateConnect ();
    ioStatus read ( unsigned type, unsigned long count, 
        cacReadNotify &, ioid * );
    void write ( unsigned type, unsigned long count, 
        const void *pValue );
    ioStatus write ( unsigned type, unsigned long count, 
        const void *pValue, cacWriteNotify &, ioid * );
    void subscribe ( unsigned type, unsigned long nElem, 
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
    this->notify().accessRightsNotify ( arIn );
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
    this->connect ( this->typeCode, this->count, this->sid );
}

inline void nciu::searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
    unsigned typeIn, unsigned long countIn )
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

inline cac & nciu::getClient ()
{
    return this->cacCtx;
}

inline void nciu::connectTimeoutNotify ()
{
    this->f_connectTimeOutSeen = true;
}

#endif // ifdef nciuh
