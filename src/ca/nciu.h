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
#include "cxxCompilerDependencies.h"

#ifdef nciuh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#define CA_MINOR_PROTOCOL_REVISION 11
#include "caProto.h"

#include "cacIO.h"

class cac;
class netiiu;
class callbackMutex;

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
    ~nciu ();
    void destroy ();
    void connect ( unsigned nativeType, 
        unsigned nativeCount, unsigned sid, bool v41Ok );
    void connect ();
    void connectStateNotify ( epicsGuard < callbackMutex > &  ) const;
    void accessRightsNotify ( epicsGuard < callbackMutex > & ) const;
    void disconnect ( netiiu & newiiu );
    bool searchMsg ( class udpiiu & iiu, unsigned & retryNoForThisChannel );
    void createChannelRequest ( class tcpiiu & iiu );
    bool identifierEquivelence ( unsigned idToMatch );
    void beaconAnomalyNotify ();
    void serviceShutdownNotify ();
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
    const char *pName () const;
    unsigned nameLen () const;
    const char * pHostName () const; // deprecated - please do not use
    arrayElementCount nativeElementCount () const;
    bool connected () const;
    void writeException ( epicsGuard < callbackMutex > &,
        int status, const char *pContext, unsigned type, arrayElementCount count );
    cacChannel::priLev getPriority () const;
    void * operator new ( size_t size, tsFreeList < class nciu, 1024 > & );
    epicsPlacementDeleteOperator (( void *, tsFreeList < class nciu, 1024 > & ))
private:
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
    bool f_connected:1;
    bool f_claimSent:1;
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
    double receiveWatchdogDelay () const;
    bool ca_v42_ok () const;
    void hostName ( char *pBuf, unsigned bufLength ) const;
    static void stringVerify ( const char *pStr, const unsigned count );
	nciu ( const nciu & );
	nciu & operator = ( const nciu & );
    void * operator new ( size_t );
    void operator delete ( void * );
};

inline void * nciu::operator new ( size_t size, 
    tsFreeList < class nciu, 1024 > & freeList )
{ 
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void nciu::operator delete ( void * pCadaver,
    tsFreeList < class nciu, 1024 > & freeList )
{ 
    freeList.release ( pCadaver, sizeof ( nciu ) );
}
#endif

inline bool nciu::identifierEquivelence ( unsigned idToMatch )
{
    return idToMatch == this->id;
}

inline void nciu::accessRightsStateChange ( const caAccessRights & arIn )
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

inline netiiu * nciu::getPIIU ()
{
    return this->piiu;
}

inline void nciu::writeException ( epicsGuard < callbackMutex > &, int status,
    const char *pContext, unsigned typeIn, arrayElementCount countIn )
{
    this->notify().writeException ( status, pContext, typeIn, countIn );
}

inline void nciu::accessRightsNotify ( epicsGuard < callbackMutex > & ) const
{
    this->notify().accessRightsNotify ( this->accessRightState );
}

inline void nciu::connectStateNotify ( epicsGuard < callbackMutex > & ) const
{
    if ( this->f_connected ) {
        this->notify().connectNotify ();
    }
    else {
        this->notify().disconnectNotify ();
    }
}

inline const netiiu * nciu::getConstPIIU () const
{
    return this->piiu;
}

inline void nciu::serviceShutdownNotify ()
{
    this->notify().serviceShutdownNotify ();
}

inline cac & nciu::getClient ()
{
    return this->cacCtx;
}

inline cacChannel::priLev nciu::getPriority () const
{
    return this->priority;
}

inline cacPrivateListOfIO::cacPrivateListOfIO () 
{
}

#endif // ifdef nciuh
