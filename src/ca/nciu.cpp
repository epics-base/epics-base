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
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 *
 * Notes:
 * 1) This class has a pointer to the IIU. This pointer always points at 
 * a valid IIU. If the client context is deleted then the channel points at a
 * static file scope IIU. IIU's that disconnect go into an inactive state
 * and are stored on a list for later reuse. When the channel calls a
 * member function of the IIU, the IIU verifies that the channel's IIU
 * pointer is still pointing at itself only after it has acquired the IIU 
 * lock.
 */

#include <new>
#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "cac.h"
#include "osiWireFormat.h"
#include "udpiiu.h"
#include "virtualCircuit.h"
#include "cadef.h"
#include "db_access.h" // for INVALID_DB_REQ

nciu::nciu ( cac & cacIn, netiiu & iiuIn, cacChannelNotify & chanIn, 
            const char *pNameIn, cacChannel::priLev pri ) :
    cacChannel ( chanIn ), 
    cacCtx ( cacIn ),
    piiu ( &iiuIn ),
    sid ( UINT_MAX ),
    count ( 0 ),
    retry ( 0u ),
    nameLength ( 0u ),
    typeCode ( USHRT_MAX ),
    priority ( static_cast <ca_uint8_t> ( pri ) ),
    f_connected ( false ),
    f_claimSent ( false )
{
	size_t nameLengthTmp = strlen ( pNameIn ) + 1;

    // second constraint is imposed by size field in protocol header
    if ( nameLengthTmp > MAX_UDP_SEND - sizeof ( caHdr ) || nameLengthTmp > USHRT_MAX ) {
        throw cacChannel::badString ();
    }
    
    if ( pri > 0xff ) {
        throw cacChannel::badPriority ();
    }

	this->nameLength = static_cast <unsigned short> ( nameLengthTmp );

    this->pNameStr = new char [ this->nameLength ];
    strcpy ( this->pNameStr, pNameIn );
}

nciu::~nciu ()
{
    delete [] this->pNameStr;
}

void nciu::destroy ()
{
    // care is taken so that a lock is not applied during this phase
    this->cacCtx.destroyChannel ( *this );
}

void nciu::operator delete ( void * pCadaver )
{ 
    throw std::logic_error 
        ( "compiler is confused about placement delete" );
}

void nciu::initiateConnect ()
{   
    this->cacCtx.initiateConnect ( *this );
}

void nciu::connect ( unsigned nativeType, 
	unsigned nativeCount, unsigned sidIn, bool v41Ok )
{
    if ( ! this->f_claimSent ) {
        this->cacCtx.printf (
            "CAC: Ignored conn resp to chan lacking virtual circuit CID=%u SID=%u?\n",
            this->getId (), sidIn );
        return;
    }

    if ( this->f_connected ) {
        this->cacCtx.printf (
            "CAC: Ignored conn resp to conn chan CID=%u SID=%u?\n",
            this->getId (), sidIn );
        return;
    }

    if ( ! dbf_type_is_valid ( nativeType ) ) {
        this->cacCtx.printf (
            "CAC: Ignored conn resp with bad native data type CID=%u SID=%u?\n",
            this->getId (), sidIn );
        return;
    }

    this->typeCode = static_cast < unsigned short > ( nativeType );
    this->count = nativeCount;
    this->sid = sidIn;
    this->f_connected = true;

    /*
     * if less than v4.1 then the server will never
     * send access rights and there will always be access 
     */
    if ( ! v41Ok ) {
        this->accessRightState.setReadPermit();
        this->accessRightState.setWritePermit();
    }
}

void nciu::disconnect ( netiiu & newiiu )
{
    this->piiu = & newiiu;
    this->retry = 0u;
    this->typeCode = USHRT_MAX;
    this->count = 0u;
    this->sid = UINT_MAX;
    this->accessRightState.clrReadPermit();
    this->accessRightState.clrWritePermit();
    this->f_claimSent = false;
    this->f_connected = false;
}

/*
 * nciu::searchMsg ()
 */
bool nciu::searchMsg ( udpiiu & iiu, unsigned & retryNoForThisChannel )
{
    caHdr msg;
    bool success;

    msg.m_cmmd = epicsHTON16 ( CA_PROTO_SEARCH );
    msg.m_available = epicsHTON32 ( this->getId () );
    msg.m_dataType = epicsHTON16 ( DONTREPLY );
    msg.m_count = epicsHTON16 ( CA_MINOR_PROTOCOL_REVISION );
    msg.m_cid = epicsHTON32 ( this->getId () );

    success = iiu.pushDatagramMsg ( msg, 
            this->pNameStr, this->nameLength );
    if ( success ) {
        //
        // increment the number of times we have tried 
        // to find this channel
        //
        if ( this->retry < UINT_MAX ) {
            this->retry++;
        }
        retryNoForThisChannel = this->retry;
    }

    return success;
}


const char *nciu::pName () const
{
    return this->pNameStr;
}

unsigned nciu::nameLen () const
{
    return this->nameLength;
}

void nciu::createChannelRequest ( tcpiiu & iiu )
{
    iiu.createChannelRequest ( *this );
    this->f_claimSent = true;
}

cacChannel::ioStatus nciu::read ( unsigned type, arrayElementCount countIn, 
                     cacReadNotify &notify, ioid *pId )
{
    //
    // fail out if their arguments are invalid
    //
    if ( ! this->f_connected ) {
        throw notConnected ();
    }
    if ( INVALID_DB_REQ (type) ) {
        throw badType ();
    }
    if ( ! this->accessRightState.readPermit() ) {
        throw noReadAccess ();
    }
    if ( countIn > UINT_MAX ) {
        throw outOfBounds ();
    }

    if ( countIn == 0 ) {
        countIn = this->count;
    }
    
    ioid tmpId = this->cacCtx.readNotifyRequest ( *this, type, countIn, notify );
    if ( pId ) {
        *pId = tmpId;
    }
    return cacChannel::iosAsynch;
}

void nciu::stringVerify ( const char *pStr, const unsigned count )
{
    for ( unsigned i = 0; i < count; i++ ) {
        unsigned int strsize = 0;
        while ( pStr[strsize++] != '\0' ) {
            if ( strsize >= MAX_STRING_SIZE ) {
                throw badString();
            }
        }
        pStr += MAX_STRING_SIZE;
    }
}

void nciu::write ( unsigned type, 
                 arrayElementCount countIn, const void *pValue )
{
    // make sure that they get this and not "no write access"
    // if disconnected
    if ( ! this->f_connected ) {
        throw notConnected();
    }

    if ( ! this->accessRightState.writePermit() ) {
        throw noWriteAccess();
    }

    if ( countIn > this->count || countIn == 0 ) {
        throw outOfBounds();
    }

    if ( type == DBR_STRING ) {
        nciu::stringVerify ( (char *) pValue, countIn );
    }

    this->cacCtx.writeRequest ( *this, type, countIn, pValue );
}

cacChannel::ioStatus nciu::write ( unsigned type, arrayElementCount countIn, 
                        const void *pValue, cacWriteNotify &notify, ioid *pId )
{
    // make sure that they get this and not "no write access"
    // if disconnected
    if ( ! this->f_connected ) {
        throw notConnected();
    }

    if ( ! this->accessRightState.writePermit() ) {
        throw noWriteAccess();
    }

    if ( countIn > this->count || countIn == 0 ) {
        throw outOfBounds();
    }

    if ( type == DBR_STRING ) {
        nciu::stringVerify ( (char *) pValue, countIn );
    }

    ioid tmpId = this->cacCtx.writeNotifyRequest ( *this, type, countIn, pValue, notify );
    if ( pId ) {
        *pId = tmpId;
    }
    return cacChannel::iosAsynch;
}

void nciu::subscribe ( unsigned type, arrayElementCount nElem, 
                         unsigned mask, cacStateNotify &notify, ioid *pId )
{
    if ( INVALID_DB_REQ(type) ) {
        throw badType();
    }

    if ( mask > 0xffff || mask == 0u ) {
        throw badEventSelection();
    }

    ioid tmpId = this->cacCtx.subscriptionRequest ( 
                *this, type, nElem, mask, notify );
    if ( pId ) {
        *pId = tmpId;
    }
}

void nciu::ioCancel ( const ioid &idIn )
{
    this->cacCtx.ioCancel ( *this, idIn );
}

void nciu::ioShow ( const ioid &idIn, unsigned level ) const
{
    this->cacCtx.ioShow ( idIn, level );
}

void nciu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    epicsGuard < cacMutex > locker ( this->cacCtx.mutexRef() );
    this->piiu->hostName ( pBuf, bufLength );
}

// deprecated - please do not use, this is _not_ thread safe
const char * nciu::pHostName () const
{
    return this->piiu->pHostName (); // ouch !
}

bool nciu::ca_v42_ok () const
{
    epicsGuard < cacMutex > locker ( this->cacCtx.mutexRef() );
    return this->piiu->ca_v42_ok ();
}

short nciu::nativeType () const
{
    epicsGuard < cacMutex > locker ( this->cacCtx.mutexRef() );
    short type;
    if ( this->f_connected ) {
        if ( this->typeCode < SHRT_MAX ) {
            type = static_cast <short> ( this->typeCode );
        }
        else {
            type = TYPENOTCONN;
        }
    }
    else {
        type = TYPENOTCONN;
    }
    return type;
}

arrayElementCount nciu::nativeElementCount () const
{
    epicsGuard < cacMutex > locker ( this->cacCtx.mutexRef() );
    arrayElementCount countOut;
    if ( this->f_connected ) {
        countOut = this->count;
    }
    else {
        countOut = 0ul;
    }
    return countOut;
}

caAccessRights nciu::accessRights () const
{
    epicsGuard < cacMutex > locker ( this->cacCtx.mutexRef() );
    caAccessRights tmp = this->accessRightState;
    return tmp;
}

unsigned nciu::searchAttempts () const
{
    epicsGuard < cacMutex > locker ( this->cacCtx.mutexRef() );
    return this->retry;
}

double nciu::beaconPeriod () const
{
    return this->cacCtx.beaconPeriod ( *this );
}

void nciu::show ( unsigned level ) const
{
    epicsGuard < cacMutex > locker ( this->cacCtx.mutexRef() );
    if ( this->f_connected ) {
        char hostNameTmp [256];
        this->hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
        ::printf ( "Channel \"%s\", connected to server %s", 
            this->pNameStr, hostNameTmp );
        if ( level > 1u ) {
            int tmpTypeCode = static_cast < int > ( this->typeCode );
            ::printf ( ", native type %s, native element count %u",
                dbf_type_to_text ( tmpTypeCode ), this->count );
            ::printf ( ", %sread access, %swrite access", 
                this->accessRightState.readPermit() ? "" : "no ", 
                this->accessRightState.writePermit() ? "" : "no ");
        }
        ::printf ( "\n" );
    }
    else {
        ::printf ( "Channel \"%s\" is disconnected\n", this->pNameStr );
    }

    if ( level > 2u ) {
        ::printf ( "\tnetwork IO pointer = %p\n", 
            static_cast <void *> ( this->piiu ) );
        ::printf ( "\tserver identifier %u\n", this->sid );
        ::printf ( "\tsearch retry number=%u\n", this->retry );
        ::printf ( "\tname length=%u\n", this->nameLength );
    }
}

int nciu::printf ( const char *pFormat, ... )
{
    va_list theArgs;

    va_start ( theArgs, pFormat );
    
    int status = this->cacCtx.vPrintf ( pFormat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void nciu::beaconAnomalyNotify () 
{
    if ( this->retry > beaconAnomalyRetrySetpoint ) {
        this->retry = beaconAnomalyRetrySetpoint;
    }
}

