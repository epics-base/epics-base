
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
 * 1) This class has a pointer to the IIU. This pointer always points at 
 * a valid IIU. If the client context is deleted then the channel points at a
 * static file scope IIU. IIU's that disconnect go into an inactive state
 * and are stored on a list for later reuse. When the channel calls a
 * member function of the IIU, the IIU verifies that the channel's IIU
 * pointer is still pointing at itself only after it has acquired the IIU 
 * lock.
 */

#include "iocinf.h"

#include "nciu_IL.h"
#include "netReadNotifyIO_IL.h"
#include "netWriteNotifyIO_IL.h"
#include "netSubscription_IL.h"
#include "cac_IL.h"

tsFreeList < class nciu, 1024 > nciu::freeList;
epicsMutex nciu::freeListMutex;

static const caar defaultAccessRights = { false, false };

nciu::nciu ( cac &cacIn, netiiu &iiuIn, cacChannelNotify &chanIn, 
            const char *pNameIn ) :
    cacChannelIO ( chanIn ), 
    cacCtx ( cacIn ),
    accessRightState ( defaultAccessRights ),
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
    f_claimSent ( false ),
    f_firstConnectDecrementsOutstandingIO ( false ),
    f_connectTimeOutSeen ( false )
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

nciu::~nciu ()
{
    if ( ! this->fullyConstructed () ) {
        return;
    }

    // care is taken so that a lock is not applied during this phase
    unsigned i = 0u;
    while ( ! this->piiu->destroyAllIO ( *this ) ) {
        if ( i++ > 1000u ) {
            this->cacCtx.printf ( 
                "CAC: unable to destroy IO when channel destroyed?\n" );
            break;
        }
    }
    this->piiu->clearChannelRequest ( *this );
    this->cacCtx.uninstallChannel ( *this );

    if ( ! this->f_connectTimeOutSeen && ! this->f_previousConn ) {
        if ( this->f_firstConnectDecrementsOutstandingIO ) {
            this->cacCtx.decrementOutstandingIO ();
        }
    }

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
    if ( ! this->accessRightState.read_access ) {
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

/*
 * check_a_dbr_string()
 */
LOCAL int check_a_dbr_string ( const char *pStr, const unsigned count )
{
    for ( unsigned i = 0; i < count; i++ ) {
        unsigned int strsize = 0;
        while ( pStr[strsize++] != '\0' ) {
            if ( strsize >= MAX_STRING_SIZE ) {
                return ECA_STRTOBIG;
            }
        }
        pStr += MAX_STRING_SIZE;
    }

    return ECA_NORMAL;
}

int nciu::write ( unsigned type, unsigned long countIn, const void *pValue )
{
    // check this first so thet get a decent diagnostic
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }

    if ( ! this->accessRightState.write_access ) {
        return ECA_NOWTACCESS;
    }

    if ( countIn > this->count || countIn == 0 ) {
        return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING ) {
        int status = check_a_dbr_string ( (char *) pValue, countIn );
        if ( status != ECA_NORMAL ) {
            return status;
        }
    }

    return this->piiu->writeRequest ( *this, type, countIn, pValue );
}

int nciu::write ( unsigned type, unsigned long countIn, const void *pValue, cacNotify &notify )
{
    // check this first so thet get a decent diagnostic
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }

    if ( ! this->accessRightState.write_access ) {
        return ECA_NOWTACCESS;
    }

    if ( countIn > this->count || countIn == 0 ) {
            return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING ) {
        int status = check_a_dbr_string ( (char *) pValue, countIn );
        if ( status != ECA_NORMAL ) {
            return status;
        }
    }

    return this->piiu->writeNotifyRequest ( *this, notify, type, countIn, pValue );
}

void nciu::initiateConnect ()
{   
    this->notifyStateChangeFirstConnectInCountOfOutstandingIO ();
    this->cacCtx.installNetworkChannel ( *this, this->piiu );
}

void nciu::connect ( unsigned nativeType, 
	unsigned long nativeCount, unsigned sidIn )
{
    bool v41Ok;
    {
        epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );

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


        if ( ! this->f_connectTimeOutSeen && ! this->f_previousConn ) {
            if ( this->f_firstConnectDecrementsOutstandingIO ) {
                this->cacCtx.decrementOutstandingIO ();
            }
        }

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
            this->accessRightState.read_access = true;
            this->accessRightState.write_access = true;
        }
    }

    // resubscribe for monitors from this channel 
    this->piiu->connectAllIO ( *this );

    this->notify ().connectNotify ( *this );

    /*
     * if less than v4.1 then the server will never
     * send access rights and we know that there
     * will always be access and also need to call 
     * their call back here
     */
    if ( ! v41Ok ) {
        this->notify ().accessRightsNotify ( *this, this->accessRightState );
    }
}

void nciu::connectTimeoutNotify ()
{
    if ( ! this->f_connectTimeOutSeen ) {
        epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );
        this->f_connectTimeOutSeen = true;
    }
}

void nciu::disconnect ( netiiu &newiiu )
{
    bool wasConnected;

    this->piiu->disconnectAllIO ( *this );

    {
        epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );

        this->piiu = &newiiu;
        this->retry = 0u;
        this->typeCode = USHRT_MAX;
        this->count = 0u;
        this->sid = UINT_MAX;
        this->accessRightState.read_access = false;
        this->accessRightState.write_access = false;
        this->f_claimSent = false;

        if ( this->f_connected ) {
            wasConnected = true;
        }
        else {
            wasConnected = false;
        }
        this->f_connected = false;
    }

    if ( wasConnected ) {
        /*
         * look for events that have an event cancel in progress
         */
        this->notify ().disconnectNotify ( *this );
        this->notify ().accessRightsNotify ( *this, this->accessRightState );
    }

    this->resetRetryCount ();
}

/*
 * nciu::searchMsg ()
 */
bool nciu::searchMsg ( unsigned short retrySeqNumber, unsigned &retryNoForThisChannel )
{
    caHdr msg;
    bool success;

    msg.m_cmmd = htons ( CA_PROTO_SEARCH );
    msg.m_available = this->getId ();
    msg.m_dataType = htons ( DONTREPLY );
    msg.m_count = htons ( CA_MINOR_VERSION );
    msg.m_cid = this->getId ();

    success = this->piiu->pushDatagramMsg ( msg, 
            this->pNameStr, this->nameLength );
    if ( success ) {
        //
        // increment the number of times we have tried 
        // to find this channel
        //
        epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );
        if ( this->retry < MAXCONNTRIES ) {
            this->retry++;
        }
        this->retrySeqNo = retrySeqNumber;
        retryNoForThisChannel = this->retry;
     }

    return success;
}

void nciu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    this->piiu->hostName ( pBuf, bufLength );
}

// deprecated - please do not use, this is _not_ thread safe
const char * nciu::pHostName () const
{
    return this->piiu->pHostName (); // ouch !
}

bool nciu::ca_v42_ok () const
{
    return this->piiu->ca_v42_ok ();
}

short nciu::nativeType () const
{
    short type;

    epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );
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

unsigned long nciu::nativeElementCount () const
{
    unsigned long countOut;
    epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );

    if ( this->f_connected ) {
        countOut = this->count;
    }
    else {
        countOut = 0ul;
    }

    return countOut;
}

channel_state nciu::state () const
{
    epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );
    channel_state stateOut;

    if ( this->f_connected ) {
        stateOut = cs_conn;
    }
    else if ( this->f_previousConn ) {
        stateOut = cs_prev_conn;
    }
    else {
        stateOut = cs_never_conn;
    }

    return stateOut;
}

caar nciu::accessRights () const
{
    epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );
    caar tmp = this->accessRightState;
    return tmp;
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

double nciu::beaconPeriod () const
{
    return this->piiu->beaconPeriod ();
}

int nciu::createChannelRequest ()
{
    int status = this->piiu->createChannelRequest ( *this );
    if ( status == ECA_NORMAL ) {
        epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );
        this->f_claimSent = true;
    }
    return status;
}

int nciu::subscribe ( unsigned type, unsigned long nElem, 
                         unsigned mask, cacNotify &notify,
                         cacNotifyIO *&pNotifyIO )
{
    if ( INVALID_DB_REQ (type) ) {
        return ECA_BADTYPE;
    }

    if ( mask > 0xffff || mask == 0u ) {
        return ECA_BADMASK;
    }

    netSubscription *pSubcr = new netSubscription ( *this, 
        type, nElem, mask, notify );
    if ( pSubcr ) {
        this->piiu->installSubscription ( *pSubcr );
        pNotifyIO = pSubcr;
        return ECA_NORMAL;;
    }
    else {
        return ECA_ALLOCMEM;
    }
}

void nciu::notifyStateChangeFirstConnectInCountOfOutstandingIO ()
{
    epicsAutoMutex locker ( this->cacCtx.nciuPrivate::mutex );
    // test is performed via a callback so that locking is correct
    if ( ! this->f_connectTimeOutSeen && ! this->f_previousConn ) {
        if ( this->notify ().includeFirstConnectInCountOfOutstandingIO () ) { 
            if ( ! this->f_firstConnectDecrementsOutstandingIO ) {
                this->cacCtx.incrementOutstandingIO ();
                this->f_firstConnectDecrementsOutstandingIO = true;
            }
        }
        else {
            if ( this->f_firstConnectDecrementsOutstandingIO ) {
                this->cacCtx.decrementOutstandingIO ();
                this->f_firstConnectDecrementsOutstandingIO = false;
            }
        }
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
            int tmpTypeCode = static_cast < int > ( this->typeCode );
            printf ( ", native type %s, native element count %u",
                dbf_type_to_text ( tmpTypeCode ), this->count );
            printf ( ", %sread access, %swrite access", 
                this->accessRightState.read_access ? "" : "no ", 
                this->accessRightState.write_access ? "" : "no ");
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
        printf ( "\tnetwork IO pointer = %p\n", 
            static_cast <void *> ( this->piiu ) );
        printf ( "\tserver identifier %u\n", this->sid );
        printf ( "\tsearch retry number=%u, search retry sequence number=%u\n", 
            this->retry, this->retrySeqNo );
        printf ( "\tname length=%u\n", this->nameLength );
        printf ( "\tfully cunstructed boolean=%u\n", this->f_fullyConstructed );
    }
}


