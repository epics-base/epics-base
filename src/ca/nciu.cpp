
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "iocinf.h"

#include "nciu_IL.h"
#include "netReadNotifyIO_IL.h"
#include "netWriteNotifyIO_IL.h"
#include "netSubscription_IL.h"

tsFreeList < class nciu, 1024 > nciu::freeList;

struct putCvrtBuf {
    ELLNODE             node;
    unsigned long       size;
    void                *pBuf;
};

/*
 * nciu::nciu ()
 */
nciu::nciu ( cac *pcac, cacChannel &chan, const char *pNameIn ) :
    cacChannelIO ( chan )
{
    static const caar defaultAccessRights = { false, false };
    size_t strcnt;

    strcnt = strlen ( pNameIn ) + 1;
    if ( strcnt > MAX_UDP - sizeof ( caHdr ) ) {
        throwWithLocation ( caErrorCode ( ECA_STRTOBIG ) );
    }
    this->pNameStr = reinterpret_cast <char *> ( malloc ( strcnt ) );
    if ( ! this->pNameStr ) {
        this->f_fullyConstructed = false;
        return;
    }
    strcpy ( this->pNameStr, pNameIn );

    this->typeCode = USHRT_MAX; /* invalid initial type    */
    this->count = 0;    /* invalid initial count     */
    this->sid = UINT_MAX; /* invalid initial server id     */
    this->ar = defaultAccessRights;
    this->nameLength = strcnt;
    this->previousConn = 0;
    this->f_connected = false;
    this->f_fullyConstructed = true;

    pcac->lock ();

    pcac->registerChannel ( *this );

    pcac->installDisconnectedChannel ( *this );

    chan.attachIO ( *this );

    pcac->unlock ();
}

/*
 * nciu::~nciu ()
 */
nciu::~nciu ()
{
    netiiu *piiuCopy = this->piiu;

    if ( ! this->fullyConstructed () ) {
        return;
    }

    // this must go in the derived class's destructor because
    // this calls virtual functions in the cacChannelIO base
    this->ioReleaseNotify ();

    if ( this->f_connected ) {
        caHdr hdr;

        hdr.m_cmmd = htons ( CA_PROTO_CLEAR_CHANNEL );
        hdr.m_available = this->getId ();
        hdr.m_cid = this->sid;
        hdr.m_dataType = htons ( 0 );
        hdr.m_count = htons ( 0 );
        hdr.m_postsize = 0;

        this->piiu->pushStreamMsg (&hdr, NULL, true);
    }

    this->piiu->pcas->lock ();

    /*
     * remove any IO blocks still attached to this channel
     */
    tsDLIterBD <baseNMIU> iter = this->eventq.first ();
    while ( iter.valid () ) {
        tsDLIterBD <baseNMIU> next = iter.itemAfter ();
        iter->destroy ();
        iter = next;
    }

    this->piiu->pcas->unregisterChannel ( *this );

    this->piiu->removeFromChanList ( *this );

    free ( reinterpret_cast <void *> ( this->pNameStr ) );

    piiuCopy->pcas->unlock (); // remove clears this->piiu
}

int nciu::read ( unsigned type, unsigned long countIn, cacNotify &notify )
{
    int status;
    caHdr hdr;
    ca_uint16_t type_u16;
    ca_uint16_t count_u16;

    /* 
     * fail out if channel isnt connected or arguments are 
     * otherwise invalid
     */
    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }
    if ( INVALID_DB_REQ (type) ) {
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

    /*
     * only after range checking type and count cast 
     * them down to a smaller size
     */
    type_u16 = (ca_uint16_t) type;
    count_u16 = (ca_uint16_t) countIn;

    this->piiu->pcas->lock ();
    {
        netReadNotifyIO *monix = new netReadNotifyIO ( *this, notify );
        if ( ! monix ) {
            this->piiu->pcas->unlock ();
            return ECA_ALLOCMEM;
        }

        hdr.m_cmmd = htons (CA_PROTO_READ_NOTIFY);
        hdr.m_dataType = htons (type_u16);
        hdr.m_count = htons (count_u16);
        hdr.m_available = monix->getId ();
        hdr.m_postsize = 0;
        hdr.m_cid = this->sid;
    }
    this->piiu->pcas->unlock ();

    status = this->piiu->pushStreamMsg (&hdr, NULL, true);
    if ( status != ECA_NORMAL ) {
        /*
         * we need to be careful about touching the monix
         * pointer after the lock has been released
         */
        this->piiu->pcas->safeDestroyNMIU (hdr.m_available);
    }

    return status;
}

int nciu::read ( unsigned type, unsigned long countIn, void *pValue )
{
    int status;
    caHdr hdr;
    ca_uint16_t type_u16;
    ca_uint16_t count_u16;

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

    /*
     * only after range checking type and count cast 
     * them down to a smaller size
     */
    type_u16 = ( ca_uint16_t ) type;
    count_u16 = ( ca_uint16_t ) countIn;

    this->piiu->pcas->lock ();
    {
        netReadCopyIO *monix = new netReadCopyIO ( *this, 
		type, countIn, pValue, this->readSequence () );
        if ( ! monix ) {
            this->piiu->pcas->unlock ();
            return ECA_ALLOCMEM;
        }

        hdr.m_cmmd = htons ( CA_PROTO_READ );
        hdr.m_dataType = htons ( type_u16 );
        hdr.m_count = htons ( count_u16 );
        hdr.m_available = monix->getId ();
        hdr.m_postsize = 0;
        hdr.m_cid = this->sid;
    }
    this->piiu->pcas->unlock ();

    status = this->piiu->pushStreamMsg ( &hdr, NULL, true );
    if ( status != ECA_NORMAL ) {
        /*
         * we need to be careful about touching the monix
         * pointer after the lock has been released
         */
        this->piiu->pcas->safeDestroyNMIU ( hdr.m_available );
    }

    return status;
}

/*
 * free_put_convert()
 */
#ifdef CONVERSION_REQUIRED 
LOCAL void free_put_convert (cac *pcac, void *pBuf)
{
    struct putCvrtBuf   *pBufHdr;

    pBufHdr = (struct putCvrtBuf *)pBuf;
    pBufHdr -= 1;
    assert ( pBufHdr->pBuf == (void *) ( pBufHdr + 1 ) );

    pcac->lock ();
    ellAdd (&pcac->putCvrtBuf, &pBufHdr->node);
    pcac->unlock ();

    return;
}
#endif /* CONVERSION_REQUIRED */

/*
 * check_a_dbr_string()
 */
LOCAL int check_a_dbr_string (const char *pStr, const unsigned count)
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

/*
 * malloc_put_convert()
 */
#ifdef CONVERSION_REQUIRED 
LOCAL void *malloc_put_convert (cac *pcac, unsigned long size)
{
    struct putCvrtBuf *pBuf;

    pcac->lock ();
    while ( (pBuf = (struct putCvrtBuf *) ellGet(&pcac->putCvrtBuf)) ) {
        if ( pBuf->size >= size ) {
            break;
        }
        else {
            free ( pBuf );
        }
    }
    pcac->unlock ();

    if ( ! pBuf ) {
        pBuf = (struct putCvrtBuf *) malloc ( sizeof (*pBuf) + size );
        if (!pBuf) {
            return NULL;
        }
        pBuf->size = size;
        pBuf->pBuf = (void *) ( pBuf + 1 );
    }

    return pBuf->pBuf;
}
#endif /* CONVERSION_REQUIRED */

/*
 * nciu::issuePut ()
 */
int nciu::issuePut ( ca_uint16_t cmd, unsigned idIn, chtype type, 
                     unsigned long countIn, const void *pvalue )
{ 
    int status;
    caHdr hdr;
    unsigned postcnt;
    ca_uint16_t type_u16;
    ca_uint16_t count_u16;
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

    /*
     * only after range checking type and count cast 
     * them down to a smaller size
     */
    type_u16 = (ca_uint16_t) type;
    count_u16 = (ca_uint16_t) countIn;

    if (type == DBR_STRING && countIn == 1) {
        char *pstr = (char *)pvalue;

        postcnt = strlen(pstr)+1;
    }

#   ifdef CONVERSION_REQUIRED 
    {
        unsigned i;
        void *pdest;
        unsigned size_of_one;

        size_of_one = dbr_size[type];

        pCvrtBuf = pdest = malloc_put_convert (this->piiu->pcas, postcnt);
        if (!pdest) {
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
        while (TRUE) {
            switch (type) {
            case    DBR_LONG:
                *(dbr_long_t *)pdest = htonl (*(dbr_long_t *)pvalue);
                break;

            case    DBR_CHAR:
                *(dbr_char_t *)pdest = *(dbr_char_t *)pvalue;
                break;

            case    DBR_ENUM:
            case    DBR_SHORT:
            case    DBR_PUT_ACKT:
            case    DBR_PUT_ACKS:
#           if DBR_INT != DBR_SHORT
#               error DBR_INT != DBR_SHORT ?
#           endif /*DBR_INT != DBR_SHORT*/
                *(dbr_short_t *)pdest = htons (*(dbr_short_t *)pvalue);
                break;

            case    DBR_FLOAT:
                dbr_htonf ((dbr_float_t *)pvalue, (dbr_float_t *)pdest);
                break;

            case    DBR_DOUBLE: 
                dbr_htond ((dbr_double_t *)pvalue, (dbr_double_t *)pdest);
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

            pdest = ((char *)pdest) + size_of_one;
            pvalue = ((char *)pvalue) + size_of_one;
        }

        pvalue = pCvrtBuf;
    }
#   endif /*CONVERSION_REQUIRED*/

    hdr.m_cmmd = htons (cmd);
    hdr.m_dataType = htons (type_u16);
    hdr.m_count = htons (count_u16);
    hdr.m_cid = this->sid;
    hdr.m_available = idIn;
    hdr.m_postsize = (ca_uint16_t) postcnt;

    status = this->piiu->pushStreamMsg (&hdr, pvalue, true);

#   ifdef CONVERSION_REQUIRED
        free_put_convert (this->piiu->pcas, pCvrtBuf);
#   endif /*CONVERSION_REQUIRED*/

    return status;
}

int nciu::write (unsigned type, unsigned long countIn, const void *pValue)
{
    return this->issuePut (CA_PROTO_WRITE, ~0U, type, countIn, pValue);
}

int nciu::write (unsigned type, unsigned long countIn, const void *pValue, cacNotify &notify)
{
    netWriteNotifyIO *monix;
    unsigned newId; 
    int status;

    if ( ! this->f_connected ) {
        return ECA_DISCONNCHID;
    }

    if ( ! this->piiu->ca_v41_ok () )  {
        return ECA_NOSUPPORT;
    }

    /*
     * lock around io block create and list add
     * so that we are not deleted without
     * reclaiming the resource
     */
    this->piiu->pcas->lock ();

    monix = new netWriteNotifyIO (*this, notify);
    if ( ! monix ) {
        this->piiu->pcas->unlock ();
        return ECA_ALLOCMEM;
    }

    newId = monix->getId ();

    this->piiu->pcas->unlock ();

    status = this->issuePut (CA_PROTO_WRITE_NOTIFY, newId, 
		type, countIn, pValue);
    if ( status != ECA_NORMAL ) {
        /*
         * we need to be careful about touching the monix
         * pointer after the lock has been released
         */
        this->piiu->pcas->safeDestroyNMIU ( newId );
    }
    return status;
}

int nciu::subscribe (unsigned type, unsigned long countIn, 
                         unsigned mask, cacNotify &notify)
{
    netSubscription *pNetMon;

    this->piiu->pcas->lock ();

    pNetMon = new netSubscription (*this, type, countIn, 
        static_cast <unsigned short> (mask), notify);
    if ( ! pNetMon ) {
        this->piiu->pcas->unlock ();
        return ECA_ALLOCMEM;
    }

    this->piiu->pcas->unlock ();

    pNetMon->subscriptionMsg ();

    return ECA_NORMAL;
}

void nciu::destroy ()
{
    delete this;
}

void nciu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    this->piiu->hostName ( pBuf, bufLength );
}

// deprecated - please do not use
const char * nciu::pHostName () const
{
    return this->piiu->pHostName (); 
}

bool nciu::ca_v42_ok () const
{
    return this->piiu->ca_v42_ok ();
}

short nciu::nativeType () const
{
    if ( this->f_connected ) {
        return static_cast <short> (this->typeCode);
    }
    else {
        return TYPENOTCONN;
    }
}

unsigned long nciu::nativeElementCount () const
{
    if ( this->f_connected ) {
        return this->count;
    }
    else {
        return 0ul;
    }
}

channel_state nciu::state () const
{
    if ( this->f_connected ) {
        return cs_conn;
    }
    else if ( this->previousConn ) {
        return cs_prev_conn;
    }
    else {
        return cs_never_conn;
    }
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

void nciu::connect ( tcpiiu &iiu, unsigned nativeType, 
	unsigned long nativeCount, unsigned sidIn )
{
    iiu.pcas->lock ();

    if ( this->connected () ) {
        ca_printf (
            "CAC: Ignored conn resp to conn chan CID=%u SID=%u?\n",
            this->getId (), sidIn );
        iiu.pcas->unlock ();
        return;
    }

    this->typeCode = nativeType;
    this->count = nativeCount;
    this->sid = sidIn;
    this->f_connected = true;
    this->previousConn = true;

    /*
     * if less than v4.1 then the server will never
     * send access rights and we know that there
     * will always be access and call their call back
     * here
     */
    if ( ! CA_V41 ( CA_PROTOCOL_VERSION, iiu.minor_version_number ) ) {
        this->ar.read_access = true;
        this->ar.write_access = true;
        this->accessRightsNotify ( this->ar );
    }

    this->connectNotify ();
 
    // resubscribe for monitors from this channel 
    tsDLIterBD<baseNMIU> iter = this->eventq.first ();
    while ( iter.valid () ) {
        iter->subscriptionMsg ();
        iter++;
    }

    iiu.pcas->unlock ();
}

void nciu::disconnect ()
{
    this->piiu->pcas->lock ();

    this->typeCode = USHRT_MAX;
    this->count = 0u;
    this->sid = UINT_MAX;
    this->ar.read_access = false;
    this->ar.write_access = false;
    this->f_connected = false;

    char hostNameBuf[64];
    this->piiu->hostName ( hostNameBuf, sizeof (hostNameBuf) );

    /*
     * look for events that have an event cancel in progress
     */
    tsDLIterBD <baseNMIU> iter = this->eventq.first ();
    while ( iter.valid () ) {
        tsDLIterBD <baseNMIU> next = iter.itemAfter ();
        iter->disconnect ( hostNameBuf );
        iter = next;
    }

    this->disconnectNotify ();
    this->accessRightsNotify (this->ar);

    this->piiu->pcas->unlock ();

    this->piiu->disconnect ( *this );
}

/*
 * nciu::searchMsg ()
 */
int nciu::searchMsg ()
{
    int         status;
    caHdr       msg;

    if ( this->nameLength > 0xffff ) {
        return ECA_STRTOBIG;
    }

    msg.m_cmmd = htons ( CA_PROTO_SEARCH );
    msg.m_available = this->getId ();
    msg.m_dataType = htons ( DONTREPLY );
    msg.m_count = htons ( CA_MINOR_VERSION );
    msg.m_cid = this->getId ();

    status = this->piiu->pushDatagramMsg (&msg, this->pNameStr, this->nameLength);
    if (status != ECA_NORMAL) {
        return status;
    }

    /*
     * increment the number of times we have tried to find this thisnel
     */
    if ( this->retry < MAXCONNTRIES ) {
        this->retry++;
    }

    /*
     * move the channel to the end of the list so
     * that all channels get a equal chance 
     */
    this->piiu->pcas->lock ();
    this->piiu->chidList.remove (*this);
    this->piiu->chidList.add (*this);
    this->piiu->pcas->unlock ();

    return ECA_NORMAL;
}

void nciu::searchReplySetUp ( unsigned sidIn, 
    unsigned typeIn, unsigned long countIn )
{
    this->typeCode  = typeIn;      
    this->count = countIn;
    this->sid = sidIn;
}

/*
 * nciu::claimMsg ()
 */
bool nciu::claimMsg ( tcpiiu *piiuIn )
{
    caHdr hdr;
    unsigned size;
    const char *pStr;
    int status;

    this->piiu->pcas->lock ();

    if ( ! this->claimPending ) {
        this->piiu->pcas->unlock ();
        return false;
    }

    if ( this->f_connected ) {
        this->piiu->pcas->unlock ();
        return false;
    }

    hdr = cacnullmsg;
    hdr.m_cmmd = htons (CA_PROTO_CLAIM_CIU);

    if ( CA_V44 (CA_PROTOCOL_VERSION, piiuIn->minor_version_number) ) {
        hdr.m_cid = this->getId ();
        pStr = this->pNameStr;
        size = this->nameLength;
    }
    else {
        hdr.m_cid = this->sid;
        pStr = NULL;
        size = 0u;
    }

    hdr.m_postsize = size;

    /*
     * The available field is used (abused)
     * here to communicate the minor version number
     * starting with CA 4.1.
     */
    hdr.m_available = htonl (CA_MINOR_VERSION);

    /*
     * If we are out of buffer space then postpone this
     * operation until later. This avoids any possibility
     * of a push pull deadlock (since this is sent when 
     * parsing the UDP input buffer).
     */
    status = piiuIn->pushStreamMsg (&hdr, pStr, false);
    if ( status == ECA_NORMAL ) {

        /*
         * move to the end of the list once the claim has been sent
         */
        this->claimPending = FALSE;
        piiuIn->chidList.remove (*this);
        piiuIn->chidList.add (*this);

        if ( ! CA_V42 (CA_PROTOCOL_VERSION, piiuIn->minor_version_number) ) {
            this->connect (*piiuIn, this->typeCode, this->count, this->sid);
        }
    }
    else {
        piiuIn->claimRequestsPending = true;
    }
    this->piiu->pcas->unlock ();

    if ( status == ECA_NORMAL ) {
        return true;
    }
    else {
        return false;
    }
}

void nciu::installIO ( baseNMIU &io )
{
    this->piiu->pcas->lock ();
    this->piiu->pcas->installIO ( io );
    this->eventq.add ( io );
    this->piiu->pcas->unlock ();
}

void nciu::uninstallIO ( baseNMIU &io )
{
    this->piiu->pcas->lock ();
    this->eventq.remove ( io );
    this->piiu->pcas->uninstallIO ( io );
    this->piiu->pcas->unlock ();
}

bool nciu::connected () const
{
    return this->f_connected;
}

unsigned nciu::readSequence () const
{
    return this->piiu->pcas->readSequence ();
}

void nciu::incrementOutstandingIO ()
{
    this->piiu->pcas->incrementOutstandingIO ();
}

void nciu::decrementOutstandingIO ()
{
    this->piiu->pcas->decrementOutstandingIO ();
}

void nciu::decrementOutstandingIO ( unsigned seqNumber )
{
    this->piiu->pcas->decrementOutstandingIO ( seqNumber );
}

int nciu::subscriptionMsg ( unsigned subscriptionId, unsigned typeIn, 
                           unsigned long countIn, unsigned short maskIn)
{
    int status;
    struct monops msg;
    ca_float32_t p_delta;
    ca_float32_t n_delta;
    ca_float32_t tmo;

    /*
     * clip to the native count and set to the native count if they
     * specify zero
     */
    if ( countIn > this->count ){
        countIn = this->count;
    }

    /*
     * dont allow overflow when converting to ca_uint16_t
     */
    if ( countIn > 0xffff ) {
        countIn = 0xffff;
    }

    /* msg header    */
    msg.m_header.m_cmmd = htons ( CA_PROTO_EVENT_ADD );
    msg.m_header.m_available = subscriptionId;
    msg.m_header.m_dataType = 
   	    htons ( static_cast <ca_uint16_t> ( typeIn ) );
    msg.m_header.m_count = 
        htons ( static_cast <ca_uint16_t> ( countIn ) );
    msg.m_header.m_cid = this->sid;
    msg.m_header.m_postsize = sizeof ( msg.m_info );

    /* msg body  */
    p_delta = 0.0;
    n_delta = 0.0;
    tmo = 0.0;
    dbr_htonf ( &p_delta, &msg.m_info.m_hval );
    dbr_htonf ( &n_delta, &msg.m_info.m_lval );
    dbr_htonf ( &tmo, &msg.m_info.m_toval );
    msg.m_info.m_mask = htons ( maskIn );
    msg.m_info.m_pad = 0; /* allow future use */    

    if ( this->f_connected ) {
        status = this->piiu->pushStreamMsg ( &msg.m_header, &msg.m_info, true );
    }
    else {
        status = ECA_NORMAL;
    }

    return status;
}

void nciu::lock () const
{
    this->piiu->pcas->lock ();
}

void nciu::unlock () const
{
    this->piiu->pcas->unlock ();
}