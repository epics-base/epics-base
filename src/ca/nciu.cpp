
/*  $Id$
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
 * 1) This class has a pointer to the IIU. Since it is possible
 * for the channel to exist when no IIU exists (because the user
 * created the channel), and because an IIU can disconnect and be 
 * destroyed at any time, then it is necessary to hold a mutex while
 * the IIU pointer is in use. This mutex can not be the IIU's mutex
 * because the IIU's lock must not be held while waiting for a
 * message to be sent (otherwise a push pull deadlock can occur).
 */

#include "iocinf.h"

#include "nciu_IL.h"
#include "netReadCopyIO_IL.h"
#include "netReadNotifyIO_IL.h"
#include "netWriteNotifyIO_IL.h"
#include "netSubscription_IL.h"
#include "cac_IL.h"
#include "ioCounter_IL.h"

tsFreeList < class nciu, 1024 > nciu::freeList;

static const caar defaultAccessRights = { false, false };

nciu::nciu ( cac &cacIn, netiiu &iiuIn, cacChannel &chanIn, 
            const char *pNameIn ) :
    cacChannelIO ( chanIn ), 
    cacCtx ( cacIn ),
    ar ( defaultAccessRights ),
    count ( 0 ),
    piiu ( &iiuIn ),
    sid ( UINT_MAX ),
    retry ( 0u ),
    retrySeqNo ( 0u ),
    nameLength ( strlen ( pNameIn ) + 1 ),
    typeCode ( USHRT_MAX ),
    f_connected ( false ),
    f_fullyConstructed ( true ),
    f_previousConn ( false ),
    f_claimSent ( false )
{
    // second constraint is imposed by size field in protocol header
    if ( this->nameLength > MAX_UDP_SEND - sizeof ( caHdr ) || this->nameLength > 0xffff ) {
        throwWithLocation ( caErrorCode ( ECA_STRTOBIG ) );
    }

    this->pNameStr = new char [ this->nameLength ];
    if ( ! this->pNameStr ) {
        this->f_fullyConstructed = false;
        return;
    }
    strcpy ( this->pNameStr, pNameIn );
}

void nciu::destroy ()
{
    // this occurs here so that it happens when
    // a lock is not applied
    this->piiu->destroyAllIO ( *this );
    this->piiu->clearChannelRequest ( *this );
    this->cacCtx.destroyNCIU ( *this );
}

void nciu::cacDestroy ()
{
    delete this;
}

nciu::~nciu ()
{
    if ( ! this->fullyConstructed () ) {
        return;
    }

    // this must go in the derived class's destructor because
    // this calls virtual functions in the cacChannelIO base
    this->ioReleaseNotify ();

    delete [] this->pNameStr;
}

int nciu::read ( unsigned type, unsigned long countIn, cacNotify &notify )
{
    //
    // fail out if their arguments are invalid
    //
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }
    if ( INVALID_DB_REQ (type) ) {
        return ECA_BADTYPE;
    }
    if ( ! this->ar.read_access ) {
        return ECA_NORDACCESS;
    }
    if ( countIn > UINT_MAX ) {
        return ECA_BADCOUNT;
    }
    if ( countIn == 0 ) {
        countIn = this->count;
    }
    
    return this->piiu->readNotifyRequest ( *this, notify, type, countIn );
}

int nciu::read ( unsigned type, unsigned long countIn, void *pValue )
{
    /* 
     * fail out if channel isnt connected or arguments are 
     * otherwise invalid
     */
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }
    if ( INVALID_DB_REQ ( type ) ) {
        return ECA_BADTYPE;
    }
    if ( ! this->ar.read_access ) {
        return ECA_NORDACCESS;
    }
    if ( countIn > this->count || countIn > 0xffff ) {
        return ECA_BADCOUNT;
    }
    if ( countIn == 0 ) {
        countIn = this->count;
    }

    return this->piiu->readCopyRequest ( *this, type, countIn, pValue );
}

/*
 * check_a_dbr_string()
 */
LOCAL int check_a_dbr_string ( const char *pStr, const unsigned count )
{
    unsigned i;

    for ( i = 0; i < count; i++ ) {
        unsigned int strsize = 0;
        while ( 1 ) {
            if (strsize >= MAX_STRING_SIZE ) {
                return ECA_STRTOBIG;
            }
            if ( pStr[strsize] == '\0' ) {
                break;
            }
            strsize++;
        }
        pStr += MAX_STRING_SIZE;
    }

    return ECA_NORMAL;
}

int nciu::write ( unsigned type, unsigned long countIn, const void *pValue )
{
    int status;

    // check this first so thet get a decent diagnostic
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }

    if ( ! this->ar.write_access ) {
        return ECA_NOWTACCESS;
    }

    if ( countIn > this->count || countIn == 0 ) {
        return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING ) {
        status = check_a_dbr_string ( (char *) pValue, countIn );
        if ( status != ECA_NORMAL ) {
            return status;
        }
    }

    return this->piiu->writeRequest ( *this, type, countIn, pValue );
}

int nciu::write ( unsigned type, unsigned long countIn, const void *pValue, cacNotify &notify )
{
    int status;

    // check this first so thet get a decent diagnostic
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }

    if ( ! this->ar.write_access ) {
        return ECA_NOWTACCESS;
    }

    if ( countIn > this->count || countIn == 0 ) {
            return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING ) {
        status = check_a_dbr_string ( (char *) pValue, countIn );
        if ( status != ECA_NORMAL ) {
            return status;
        }
    }

    return this->piiu->writeNotifyRequest ( *this, notify, type, countIn, pValue );
}

void nciu::connect ( unsigned nativeType, 
	unsigned long nativeCount, unsigned sidIn )
{
    if ( ! this->f_claimSent ) {
        ca_printf (
            "CAC: Ignored conn resp to chan lacking virtual circuit CID=%u SID=%u?\n",
            this->getId (), sidIn );
        return;
    }

    if ( this->f_connected ) {
        ca_printf (
            "CAC: Ignored conn resp to conn chan CID=%u SID=%u?\n",
            this->getId (), sidIn );
        return;
    }

    this->lock ();
    bool v41Ok;
    if ( this->piiu ) {
        v41Ok = this->piiu->ca_v41_ok ();
    }
    else {
        v41Ok = false;
    }    
    this->typeCode = nativeType;
    this->count = nativeCount;
    this->sid = sidIn;
    this->f_connected = true;
    this->f_previousConn = true;

    /*
     * if less than v4.1 then the server will never
     * send access rights and we know that there
     * will always be access 
     */
    if ( ! v41Ok ) {
        this->ar.read_access = true;
        this->ar.write_access = true;
    }

    this->unlock ();

    // resubscribe for monitors from this channel 
    this->piiu->connectAllIO ( *this );

    this->connectNotify ();

    /*
     * if less than v4.1 then the server will never
     * send access rights and we know that there
     * will always be access and also need to call 
     * their call back here
     */
    if ( ! v41Ok ) {
        this->accessRightsNotify ( this->ar );
    }
}

void nciu::disconnect ( netiiu &newiiu )
{
    char hostNameBuf[64];
    this->hostName ( hostNameBuf, sizeof ( hostNameBuf ) );
    bool wasConnected;

    this->piiu->disconnectAllIO ( *this );

    this->lock ();

    this->piiu = &newiiu;
    this->retry = 0u;
    this->typeCode = USHRT_MAX;
    this->count = 0u;
    this->sid = UINT_MAX;
    this->ar.read_access = false;
    this->ar.write_access = false;
    this->f_claimSent = false;

    if ( this->f_connected ) {
        wasConnected = true;
    }
    else {
        wasConnected = false;
    }
    this->f_connected = false;

    this->unlock ();

    if ( wasConnected ) {
        /*
         * look for events that have an event cancel in progress
         */
        this->disconnectNotify ();
        this->accessRightsNotify ( this->ar );
    }

    this->resetRetryCount ();
}

/*
 * nciu::searchMsg ()
 */
bool nciu::searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel )
{
    caHdr msg;
    bool status;

    msg.m_cmmd = htons ( CA_PROTO_SEARCH );
    msg.m_available = this->getId ();
    msg.m_dataType = htons ( DONTREPLY );
    msg.m_count = htons ( CA_MINOR_VERSION );
    msg.m_cid = this->getId ();

    status = this->piiu->pushDatagramMsg ( msg, 
            this->pNameStr, this->nameLength );
    if ( status ) {
        //
        // increment the number of times we have tried 
        // to find this channel
        //
        this->lock ();

        if ( this->retry < MAXCONNTRIES ) {
            this->retry++;
        }

        this->retrySeqNo = retrySeqNumber;
        retryNoForThisChannel = this->retry;

        this->unlock ();
    }

    return status;
}

void nciu::incrementOutstandingIO ()
{
    this->cacCtx.incrementOutstandingIO ();
}

void nciu::decrementOutstandingIO ()
{
    this->cacCtx.decrementOutstandingIO ();
}

void nciu::decrementOutstandingIO ( unsigned seqNumber )
{
    this->cacCtx.decrementOutstandingIO ( seqNumber );
}

void nciu::lockOutstandingIO () const
{
    this->cacCtx.lockOutstandingIO ();
}

void nciu::unlockOutstandingIO () const
{
    this->cacCtx.unlockOutstandingIO ();
}

unsigned nciu::readSequence () const
{
    return this->cacCtx.readSequenceOfOutstandingIO ();
}

void nciu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    this->piiu->hostName ( pBuf, bufLength );
}

// deprecated - please do not use, this is _not_ thread safe
const char * nciu::pHostName () const
{
    this->lock ();
    const char *pName = this->piiu->pHostName (); 
    this->unlock ();
    return pName; // ouch !
}

bool nciu::ca_v42_ok () const
{
    bool status; 

    this->lock ();
    if ( this->piiu ) {
        status = this->piiu->ca_v42_ok ();
    }
    else {
        status = false;
    }
    this->unlock ();
    return status;
}

short nciu::nativeType () const
{
    short type;

    this->lock ();
    if ( this->f_connected ) {
        type = static_cast <short> ( this->typeCode );
    }
    else {
        type = TYPENOTCONN;
    }
    this->unlock ();

    return type;
}

unsigned long nciu::nativeElementCount () const
{
    unsigned long countOut;

    this->lock ();
    if ( this->f_connected ) {
        countOut = this->count;
    }
    else {
        countOut = 0ul;
    }
    this->unlock ();

    return countOut;
}

channel_state nciu::state () const
{
    channel_state stateOut;

    this->lock ();

    if ( this->f_connected ) {
        stateOut = cs_conn;
    }
    else if ( this->f_previousConn ) {
        stateOut = cs_prev_conn;
    }
    else {
        stateOut = cs_never_conn;
    }

    this->unlock ();

    return stateOut;
}

caar nciu::accessRights () const
{
    return this->ar;
}

const char *nciu::pName () const
{
    return this->pNameStr;
}

unsigned nciu::nameLen () const
{
    return this->nameLength;
}

unsigned nciu::searchAttempts () const
{
    return this->retry;
}

int nciu::createChannelRequest ()
{
    int status = this->piiu->createChannelRequest ( *this );
    if ( status == ECA_NORMAL ) {
        this->f_claimSent = true;
    }
    return status;
}

int nciu::subscribe ( unsigned type, unsigned long nElem, 
                         unsigned mask, cacNotify &notify )
{
    netSubscription *pSubcr = new netSubscription ( *this, 
        type, nElem, mask, notify );
    if ( pSubcr ) {
        int status = this->piiu->installSubscription ( *pSubcr );
        if ( status != ECA_NORMAL ) {
            pSubcr->destroy ();
        }
        return status;
    }
    else {
        return ECA_ALLOCMEM;
    }
}

void nciu::show ( unsigned level ) const
{
    if ( this->f_connected ) {
        char hostNameTmp [256];
        this->hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
        printf ( "Channel \"%s\", connected to server %s", 
            this->pNameStr, hostNameTmp );
        if ( level > 1u ) {
            printf ( ", native type %s, native element count %u",
                dbf_type_to_text ( this->typeCode ), this->count );
            printf ( ", %sread access, %swrite access", 
                this->ar.read_access ? "" : "no ", 
                this->ar.write_access ? "" : "no ");
        }
        printf ( "\n" );
    }
    else if ( this->f_previousConn ) {
        printf ( "Channel \"%s\" (previously connected to a server)\n", this->pNameStr );
    }
    else {
        printf ( "Channel \"%s\" (unable to locate server)\n", this->pNameStr );
    }

    if ( level > 2u ) {
        printf ( "\tnetwork IO pointer=%p\n", 
            this->piiu );
        printf ( "\tserver identifier %u\n", this->sid );
        printf ( "\tsearch retry number=%u, search retry sequence number=%u\n", 
            this->retry, this->retrySeqNo );
        printf ( "\tname length=%u\n", this->nameLength );
        printf ( "\tfully cunstructed boolean=%u\n", this->f_fullyConstructed );
    }
}


