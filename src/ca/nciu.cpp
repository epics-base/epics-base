
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

nciu::nciu ( cac &cacIn, cacChannel &chanIn, const char *pNameIn ) :
    cacChannelIO ( chanIn ), cacPrivateListOfIO ( cacIn )
{
    static const caar defaultAccessRights = { false, false };
    size_t strcnt;

    strcnt = strlen ( pNameIn ) + 1;
    // second constraint is imposed by size field in protocol header
    if ( strcnt > MAX_UDP_SEND - sizeof ( caHdr ) || strcnt > 0xffff ) {
        throwWithLocation ( caErrorCode ( ECA_STRTOBIG ) );
    }

    this->pNameStr = new char [ strcnt ];
    if ( ! this->pNameStr ) {
        this->f_fullyConstructed = false;
        return;
    }
    strcpy ( this->pNameStr, pNameIn );

    this->piiu = 0u;
    this->typeCode = USHRT_MAX; /* invalid initial type    */
    this->count = 0;    /* invalid initial count     */
    this->sid = UINT_MAX; /* invalid initial server id     */
    this->ar = defaultAccessRights;
    this->nameLength = strcnt;
    this->f_previousConn = false;
    this->f_connected = false;
    this->f_fullyConstructed = true;
    this->f_claimSent = false;
    this->retry = 0u;
    this->retrySeqNo = 0u;
    this->ptrLockCount = 0u;
    this->ptrUnlockWaitCount = 0u;

    this->cacCtx.registerChannel ( *this );

    this->cacCtx.installDisconnectedChannel ( *this );

    chanIn.attachIO ( *this );
}

void nciu::destroy ()
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

    this->destroyAllIO ();

    this->lockPIIU ();

    if ( this->f_connected && this->piiu ) {
        this->piiu->clearChannelRequest ( this->getId (), this->sid );
    }
    
    this->detachChanFromIIU ();

    this->unlockPIIU ();

    this->cacCtx.unregisterChannel ( *this );

    delete [] this->pNameStr;
}

int nciu::read ( unsigned type, unsigned long countIn, cacNotify &notify )
{
    int status;
    unsigned idCopy;

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
    
    bool success = netReadNotifyIO::factory ( *this, notify, idCopy );
    if ( ! success ) {
        return ECA_ALLOCMEM;
    }

    this->lockPIIU ();
    if ( this->piiu ) {
        status = this->piiu->readNotifyRequest ( idCopy, this->sid, type, countIn );
    }
    else {
        status = ECA_DISCONNCHID;
    }
    this->unlockPIIU ();

    if ( status != ECA_NORMAL ) {
        this->cacCtx.ioDestroy ( id );
    }

    return status;
}

int nciu::read ( unsigned type, unsigned long countIn, void *pValue )
{
    unsigned idCopy;
    bool success;
    int status;

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

    success = netReadCopyIO::factory ( *this, type, countIn, pValue, 
                this->readSequence (), idCopy );
    if ( ! success ) {
        return ECA_ALLOCMEM;
    }

    this->lockPIIU ();
    if ( this->piiu ) {
        status = this->piiu->readCopyRequest ( idCopy, this->sid, type, countIn );
    }
    else {
        status = ECA_DISCONNCHID;
    }
    this->unlockPIIU ();

    if ( status != ECA_NORMAL ) {
        this->cacCtx.ioDestroy ( id );
    }

    return status;
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

#ifdef JUNKYARD

/*
 * nciu::issuePut ()
 */
int nciu::issuePut ( ca_uint16_t cmd, unsigned idIn, chtype type, 
                     unsigned long countIn, const void *pvalue )
{ 
    int status;
    caHdr hdr;
    unsigned postcnt;
#   ifdef CONVERSION_REQUIRED
        void *pCvrtBuf;
#   endif /*CONVERSION_REQUIRED*/

    /* 
     * fail out if the conn is down or the arguments are otherwise invalid
     */
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }
    if ( INVALID_DB_REQ (type) ) {
        return ECA_BADTYPE;
    }
    /*
     * compound types not allowed
     */
    if ( dbr_value_offset[type] ) {
        return ECA_BADTYPE;
    }
    if ( ! this->ar.write_access ) {
        return ECA_NOWTACCESS;
    }
    if ( countIn > this->count || countIn > 0xffff || countIn == 0 ) {
            return ECA_BADCOUNT;
    }
    if (type==DBR_STRING) {
        status = check_a_dbr_string ( (char *) pvalue, countIn );
        if (status != ECA_NORMAL) {
            return status;
        }
    }
    postcnt = dbr_size_n ( type, countIn );
    if ( postcnt > 0xffff ) {
        return ECA_TOLARGE;
    }

    if ( type == DBR_STRING && countIn == 1 ) {
        char *pstr = (char *) pvalue;
        postcnt = strlen (pstr) +1;
    }
    else {
        postcnt = dbr_size_n ( type, countIn );
    }

#   ifdef CONVERSION_REQUIRED 
    {
        unsigned i;
        void *pdest;
        unsigned size_of_one;

        size_of_one = dbr_size[type];

#error can we eliminate this?
        pCvrtBuf = pdest = this->cacCtx.mallocPutConvert ( postcnt );
        if ( ! pdest ) {
            this->unlockPIIU ();
            return ECA_ALLOCMEM;
        }

        /*
         * No compound types here because these types are read only
         * and therefore only appropriate for gets or monitors
         *
         * I changed from a for to a while loop here to avoid bounds
         * checker pointer out of range error, and unused pointer
         * update when it is a single element.
         */
        i=0;
        while ( TRUE ) {
            switch ( type ) {
            case    DBR_LONG:
                * (dbr_long_t *) pdest = 
                    htonl ( ( * reinterpret_cast < const dbr_long_t * > (pvalue) ) );
                break;

            case    DBR_CHAR:
                * (dbr_char_t *) pdest = * (dbr_char_t *) pvalue;
                break;

            case    DBR_ENUM:
            case    DBR_SHORT:
            case    DBR_PUT_ACKT:
            case    DBR_PUT_ACKS:
#           if DBR_INT != DBR_SHORT
#               error DBR_INT != DBR_SHORT ?
#           endif /*DBR_INT != DBR_SHORT*/
                * (dbr_short_t *) pdest = 
                    htons ( ( * reinterpret_cast < const dbr_short_t * > (pvalue) ) );
                break;

            case    DBR_FLOAT:
                dbr_htonf ( (dbr_float_t *) pvalue, (dbr_float_t *) pdest );
                break;

            case    DBR_DOUBLE: 
                dbr_htond ( (dbr_double_t *) pvalue, (dbr_double_t *) pdest );
            break;

            case    DBR_STRING:
                /*
                 * string size checked above
                 */
                strcpy ( (char *) pdest, (char *) pvalue );
                break;

            default:
                return ECA_BADTYPE;
            }

            if ( ++i >= countIn ) {
                break;
            }

            pdest = ( (char *) pdest ) + size_of_one;
            pvalue = ( (char *) pvalue ) + size_of_one;
        }

        pvalue = pCvrtBuf;
    }
#   endif /*CONVERSION_REQUIRED*/

    hdr.m_cmmd = htons ( cmd );
    hdr.m_dataType = htons ( static_cast <ca_uint16_t> ( type ) );
    hdr.m_count = htons ( static_cast <ca_uint16_t> ( countIn ) );
    hdr.m_cid = this->sid;
    hdr.m_available = idIn;

    this->lockPIIU ();
    if ( this->piiu ) {
        status = this->piiu->pushStreamMsg ( &hdr, pvalue, postcnt );
    }
    else {
        status = ECA_DISCONNCHID;
    }
    this->unlockPIIU ();

#   ifdef CONVERSION_REQUIRED
        this->cacCtx.freePutConvert ( pCvrtBuf );
#   endif /*CONVERSION_REQUIRED*/

    return status;
}

#endif

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

    this->lockPIIU ();
    if ( this->piiu ) {
        status = this->piiu->writeRequest ( this->sid, type, countIn, pValue );
    }
    else {
        status = ECA_DISCONNCHID;
    }
    this->lockPIIU ();
    return status;
}

int nciu::write ( unsigned type, unsigned long countIn, const void *pValue, cacNotify &notify )
{
    ca_uint32_t newId; 
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

    // dont use monix pointer because monix could be deleted
    // when the channel disconnects or when the IO completes
    bool success = netWriteNotifyIO::factory ( *this, notify, newId );
    if ( ! success ) {
        return ECA_ALLOCMEM;
    }

    this->lockPIIU ();

    if ( this->piiu  )  {
        status = this->piiu->writeNotifyRequest ( newId, this->sid, type, countIn, pValue );
    }
    else {
        status = ECA_DISCONNCHID;
    }

    this->unlockPIIU ();

    if ( status != ECA_NORMAL ) {
        /*
         * we need to be careful about touching the monix
         * pointer after the lock has been released
         */
        this->cacCtx.ioDestroy ( newId );
    }

    return status;
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

    this->lockPIIU ();
    bool v41Ok = this->piiu->ca_v41_ok ();
    this->unlockPIIU ();

    this->lock ();

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
    this->subscribeAllIO ();

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

void nciu::disconnect ()
{
    char hostNameBuf[64];
    this->hostName ( hostNameBuf, sizeof (hostNameBuf) );
    bool wasConnected;

    this->lock ();

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
        disconnectAllIO ( hostNameBuf );
        this->disconnectNotify ();
        this->accessRightsNotify ( this->ar );
    }

    this->cacCtx.installDisconnectedChannel ( *this );
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

    this->lockPIIU ();

    if ( this->piiu ) {
        status = this->piiu->pushDatagramMsg ( msg, 
                this->pNameStr, this->nameLength );
    }
    else {
        status = false;
    }

    this->unlockPIIU ();

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

int nciu::subscriptionMsg ( unsigned subscriptionId, unsigned typeIn, 
                           unsigned long countIn, unsigned short maskIn)
{
    int status;

    /*
     * clip to the native count and set to the native count if they
     * specify zero
     */
    if ( countIn > this->count ){
        countIn = this->count;
    }

    if ( this->f_connected ) {
        this->lockPIIU ();
        if ( this->piiu ) {
            status = this->piiu->subscriptionRequest ( subscriptionId, this->sid, typeIn, countIn, maskIn );
        }
        else {
            status = ECA_NORMAL;
        }
        this->unlockPIIU ();
    }
    else {
        status = ECA_NORMAL;
    }

    return status;
}

void nciu::attachChanToIIU ( netiiu &iiu )
{
    this->lockPIIU ();

    if ( this->piiu ) {
        this->piiu->mutex.lock ();
        this->piiu->channelList.remove ( *this );
        if ( this->piiu->channelList.count () == 0u ) {
            this->piiu->lastChannelDetachNotify ();
        }
        this->piiu->mutex.unlock ();
    }

    iiu.mutex.lock ();

    // add to the front of the list so that search requests 
    // for new channels will be sent first and so that
    // channels lacking a claim message prior to connecting
    // are located
    iiu.channelList.push ( *this );
    this->piiu = &iiu;

    iiu.mutex.unlock ();

    this->unlockPIIU ();
}

void nciu::detachChanFromIIU ()
{
    this->lockPIIU ();
    if ( this->piiu ) {
        this->piiu->mutex.lock ();
        this->piiu->channelList.remove ( *this );
        if ( this->piiu->channelList.count () == 0u ) {
            this->piiu->lastChannelDetachNotify ();
        }
        this->piiu->mutex.unlock ();
        this->piiu = 0u;
    }
    this->unlockPIIU ();
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
    this->lockPIIU ();
    if ( this->piiu ) {
        this->piiu->hostName ( pBuf, bufLength );
    }
    else {
        strncpy (pBuf, "<disconnected>", bufLength);
        pBuf[bufLength-1] = '\0';
    }
    this->unlockPIIU ();
}

// deprecated - please do not use, this is _not_ thread safe
const char * nciu::pHostName () const
{
    this->lockPIIU ();
    const char *pName = this->piiu->pHostName (); 
    this->unlockPIIU ();
    return pName; // ouch !
}

bool nciu::ca_v42_ok () const
{
    bool status; 

    this->lockPIIU ();
    if ( this->piiu ) {
        status = this->piiu->ca_v42_ok ();
    }
    else {
        status = false;
    }
    this->unlockPIIU ();
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

unsigned nciu::searchAttempts () const
{
    return this->retry;
}

int nciu::subscribe ( unsigned type, unsigned long countIn, 
                         unsigned mask, cacNotify &notify )
{
    unsigned idCopy;
    bool success = netSubscription::factory ( *this, type, countIn, 
        static_cast <unsigned short> (mask), notify, idCopy );
    if ( success ) {
        return ECA_NORMAL;
    }
    else  {
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
        printf ( "\tnetwork IO pointer=%p, ptr lock count=%u, ptr unlock wait count=%u\n", 
            this->piiu, this->ptrLockCount, this->ptrUnlockWaitCount );
        printf ( "\tserver identifier %u\n", this->sid );
        printf ( "\tsearch retry number=%u, search retry sequence number=%u\n", 
            this->retry, this->retrySeqNo );
        printf ( "\tname length=%u\n", this->nameLength );
        printf ( "\tfully cunstructed boolean=%u\n", this->f_fullyConstructed );
    }
}

bool nciu::setClaimMsgCache ( claimMsgCache &cache )
{
    cache.clientId = this->id;
    cache.serverId = this->sid;
    if ( cache.v44 ) {
        unsigned len = strlen ( this->pNameStr ) + 1u;
        if ( cache.bufLen < len ) {
            unsigned newBufLen = 2 * len;
            char *pNewStr = new char [ newBufLen ];
            if ( pNewStr ) {
                delete [] cache.pStr;
                cache.pStr = pNewStr;
                cache.bufLen = newBufLen;
            }
            else {
                return false;
            }
        }
        strcpy ( cache.pStr, this->pNameStr );
        cache.currentStrLen = len;
    }
    else {
        cache.currentStrLen = 0u;
    }
    this->f_claimSent = true;
    return true;
}

