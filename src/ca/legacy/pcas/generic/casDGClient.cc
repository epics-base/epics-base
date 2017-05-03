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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include "gddApps.h"
#include "caerr.h"
#include "osiWireFormat.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "casDGClient.h"
#include "osiPoolStatus.h" // osi pool monitoring functions

casDGClient::pCASMsgHandler const casDGClient::msgHandlers[] =
{
    & casDGClient::versionAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,

    & casDGClient::uknownMessageAction,
    & casDGClient::searchAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,

    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,

    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,

    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,

    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction,
    & casDGClient::uknownMessageAction
};

//
// casDGClient::casDGClient()
//
casDGClient::casDGClient ( caServerI & serverIn, clientBufMemoryManager & mgrIn ) :
    casCoreClient ( serverIn ),
    in ( *this, mgrIn, MAX_UDP_RECV + sizeof ( cadg ) ),
    out ( *this, mgrIn ),
    seqNoOfReq ( 0 ),
    minor_version_number ( 0 )
{
}

//
// casDGClient::~casDGClient()
// (virtual destructor)
//
casDGClient::~casDGClient()
{
}

//
// casDGClient::destroy()
//
void casDGClient::destroy()
{
    printf("Attempt to destroy the DG client was ignored\n");
}

//
// casDGClient::show()
//
void casDGClient::show (unsigned level) const
{
    printf ( "casDGClient at %p\n",
    static_cast <const void *> ( this ) );
    if (level>=1u) {
        char buf[64];
        this->hostName (buf, sizeof(buf));
        printf ("Client Host=%s\n", buf);
        this->casCoreClient::show ( level - 1u );
        this->in.show ( level - 1u );
        this->out.show ( level - 1u );
    }
}

//
// casDGClient::uknownMessageAction()
//
caStatus casDGClient::uknownMessageAction ()
{
    const caHdrLargeArray * mp = this->ctx.getMsg();

    if ( this->getCAS().getDebugLevel() > 3u ) {
        char pHostName[64u];
        this->lastRecvAddr.stringConvert ( pHostName, sizeof ( pHostName ) );

        caServerI::dumpMsg ( pHostName, "?", mp, this->ctx.getData(),
                             "bad request code=%u in DG\n", mp->m_cmmd );
    }

    return S_cas_badProtocol;
}

//
// casDGClient::searchAction()
//
caStatus casDGClient::searchAction()
{
    const caHdrLargeArray   *mp = this->ctx.getMsg();
    const char              *pChanName = static_cast <char * > ( this->ctx.getData() );
    caStatus                status;

    if (!CA_VSUPPORTED(mp->m_count)) {
        if ( this->getCAS().getDebugLevel() > 3u ) {
            char pHostName[64u];
            this->hostName ( pHostName, sizeof ( pHostName ) );
            printf ( "\"%s\" is searching for \"%s\" but is too old\n",
                pHostName, pChanName );
        }
        return S_cas_badProtocol;
    }

    //
    // check the sanity of the message
    //
    if ( mp->m_postsize <= 1 ) {
        char pHostName[64u];
        this->lastRecvAddr.stringConvert ( pHostName, sizeof ( pHostName ) );
        caServerI::dumpMsg ( pHostName, "?", mp, this->ctx.getData(),
            "empty PV name extension in UDP search request?\n" );
        return S_cas_success;
    }

    if ( pChanName[0] == '\0' ) {
        char pHostName[64u];
        this->lastRecvAddr.stringConvert ( pHostName, sizeof ( pHostName ) );
        caServerI::dumpMsg ( pHostName, "?", mp, this->ctx.getData(),
            "zero length PV name in UDP search request?\n" );
        return S_cas_success;
    }

    // check for an unterminated string before calling server tool
    // by searching backwards through the string (some early versions
    // of the client library might not be setting the pad bytes to nill)
    for ( unsigned i = mp->m_postsize-1; pChanName[i] != '\0'; i-- ) {
        if ( i <= 1 ) {
            char pHostName[64u];
            this->lastRecvAddr.stringConvert ( pHostName, sizeof ( pHostName ) );
            caServerI::dumpMsg ( pHostName, "?", mp, this->ctx.getData(),
                "unterminated PV name in UDP search request?\n" );
            return S_cas_success;
        }
    }

    if ( this->getCAS().getDebugLevel() > 6u ) {
        char pHostName[64u];
        this->hostName ( pHostName, sizeof ( pHostName ) );
        printf ( "\"%s\" is searching for \"%s\"\n",
            pHostName, pChanName );
    }

    //
    // verify that we have sufficent memory for a PV and a
    // monitor prior to calling PV exist test so that when
    // the server runs out of memory we dont reply to
    // search requests, and therefore dont thrash through
    // caServer::pvExistTest() and casCreatePV::pvAttach()
    //
    if ( ! osiSufficentSpaceInPool ( 0 ) ) {
        return S_cas_success;
    }

    //
    // ask the server tool if this PV exists
    //
    this->userStartedAsyncIO = false;
    pvExistReturn pver =
        this->getCAS()->pvExistTest ( this->ctx, this->lastRecvAddr, pChanName );

    //
    // prevent problems when they initiate
    // async IO but dont return status
    // indicating so (and vise versa)
    //
    if ( this->userStartedAsyncIO ) {
        if ( pver.getStatus() != pverAsyncCompletion ) {
            errMessage (S_cas_badParameter,
                "- assuming asynch IO status from caServer::pvExistTest()");
        }
        status = S_cas_success;
    }
    else {
        //
        // otherwise we assume sync IO operation was initiated
        //
        switch ( pver.getStatus() ) {
        case pverExistsHere:
            status = this->searchResponse (*mp, pver);
            break;

        case pverDoesNotExistHere:
            status = S_cas_success;
            break;

        case pverAsyncCompletion:
            errMessage (S_cas_badParameter,
                "- unexpected asynch IO status from caServer::pvExistTest() ignored");
            status = S_cas_success;
            break;

        default:
            errMessage (S_cas_badParameter,
                "- invalid return from caServer::pvExistTest() ignored");
            status = S_cas_success;
            break;
        }
    }
    return status;
}

//
// caStatus casDGClient::searchResponse()
//
caStatus casDGClient::searchResponse ( const caHdrLargeArray & msg,
                                     const pvExistReturn & retVal )
{
    caStatus status;

    if ( retVal.getStatus() != pverExistsHere ) {
        return S_cas_success;
    }

    //
    // starting with V4.1 the count field is used (abused)
    // by the client to store the minor version number of
    // the client.
    //
    // Old versions expect alloc of channel in response
    // to a search request. This is no longer supported.
    //
    if ( !CA_V44(msg.m_count) ) {
            char pName[64u];
            this->hostName (pName, sizeof (pName));
            errlogPrintf (
                "client \"%s\" using EPICS R3.11 CA connect protocol was ignored\n",
                pName);
        //
        // old connect protocol was dropped when the
        // new API was added to the server (they must
        // now use clients at EPICS 3.12 or higher)
        //
        status = this->sendErr ( &msg, ECA_DEFUNCT, invalidResID,
            "R3.11 connect sequence from old client was ignored" );
        return status;
    }

    //
    // cid field is abused to carry the IP
    // address in CA_V48 or higher
    // (this allows a CA servers to serve
    // as a directory service)
    //
    // data type field is abused to carry the IP
    // port number here CA_V44 or higher
    // (this allows multiple CA servers on one
    // host)
    //
    ca_uint32_t serverAddr;
    ca_uint16_t serverPort;
    if ( CA_V48( msg.m_count ) ) {
        struct sockaddr_in  ina;
        if ( retVal.addrIsValid() ) {
            caNetAddr addr = retVal.getAddr();
            ina = addr.getSockIP();
            //
            // If they dont specify a port number then the default
            // CA server port is assumed when it it is a server
            // address redirect (it is never correct to use this
            // server's port when it is a redirect).
            //
            if (ina.sin_port==0u) {
                ina.sin_port = htons ( CA_SERVER_PORT );
            }
        }
        else {
            caNetAddr addr = this->serverAddress ();
            ina = addr.getSockIP();
            //
            // We dont fill in the servers address here
            // because the server was not bound to a particular
            // interface, and we would need to waste CPU performing
            // the following steps to determine the interface that
            // will be used:
            //
            // o connect UDP socket to the destination IP
            // o perform a getsockname() call
            // o disconnect UDP socket from the destination IP
            //
            if ( ina.sin_addr.s_addr == INADDR_ANY ) {
                ina.sin_addr.s_addr = htonl ( ~0U );
            }
        }
        serverAddr = ntohl ( ina.sin_addr.s_addr );
        serverPort = ntohs ( ina.sin_port );
    }
    else {
        caNetAddr addr = this->serverAddress ();
        struct sockaddr_in inetAddr = addr.getSockIP();
        serverAddr = ~0U;
        serverPort = ntohs ( inetAddr.sin_port );
    }

    ca_uint16_t * pMinorVersion;
    epicsGuard < epicsMutex > guard ( this->mutex );
    status = this->out.copyInHeader ( CA_PROTO_SEARCH,
        sizeof ( *pMinorVersion ), serverPort, 0,
        serverAddr, msg.m_available,
        reinterpret_cast <void **> ( &pMinorVersion ) );
    //
    // Starting with CA V4.1 the minor version number
    // is appended to the end of each search reply.
    // This value is ignored by earlier clients.
    //
    if ( status == S_cas_success ) {
        AlignedWireRef < epicsUInt16 > tmp ( *pMinorVersion );
        tmp = CA_MINOR_PROTOCOL_REVISION;
        this->out.commitMsg ();
    }

    return status;
}

//
//  casDGClient::searchFailResponse()
//  (only when requested by the client
//  - when it isnt a reply to a broadcast)
//
caStatus casDGClient::searchFailResponse ( const caHdrLargeArray * mp )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->out.copyInHeader ( CA_PROTO_NOT_FOUND, 0,
        mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available, 0 );

    this->out.commitMsg ();

    return S_cas_success;
}

/*
 * casDGClient::versionAction()
 */
caStatus casDGClient::versionAction ()
{
    const caHdrLargeArray * mp = this->ctx.getMsg();

    if (!CA_VSUPPORTED(mp->m_count)) {
        if ( this->getCAS().getDebugLevel() > 3u ) {
            char pHostName[64u];
            this->hostName ( pHostName, sizeof ( pHostName ) );
            printf ( "\"%s\" is too old\n",
                pHostName );
        }
        return S_cas_badProtocol;
    }
    if ( mp->m_count != 0 ) {
        this->minor_version_number = static_cast <ca_uint16_t> ( mp->m_count );
        if ( CA_V411 ( mp->m_count ) ) {
            this->seqNoOfReq = mp->m_cid;
        }
        else {
            this->seqNoOfReq = 0;
        }
    }
    return S_cas_success;
}

//
// casDGClient::sendBeacon()
// (implemented here because this has knowledge of the protocol)
//
void casDGClient::sendBeacon ( ca_uint32_t beaconNumber )
{
    union {
        caHdr   msg;
        char    buf;
    };

    //
    // create the message
    //
    memset ( & buf, 0, sizeof ( msg ) );
    AlignedWireRef < epicsUInt16 > ( msg.m_cmmd ) = CA_PROTO_RSRV_IS_UP;
    AlignedWireRef < epicsUInt16 > ( msg.m_dataType ) = CA_MINOR_PROTOCOL_REVISION;
    AlignedWireRef < epicsUInt32 > ( msg.m_cid ) = beaconNumber;

    //
    // send it to all addresses on the beacon list,
    // but let the IO specific code set the address
    // field and the port field
    //
    this->sendBeaconIO ( buf, sizeof (msg), msg.m_count, msg.m_available );
}

//
// casDGClient::xSend()
//
outBufClient::flushCondition casDGClient::xSend ( char *pBufIn,
                        bufSizeT nBytesToSend, bufSizeT & nBytesSent )
{
    bufSizeT totalBytes = 0;
    while ( totalBytes < nBytesToSend ) {
        cadg *pHdr = reinterpret_cast < cadg * > ( & pBufIn[totalBytes] );

        assert ( totalBytes <= bufSizeT_MAX - pHdr->cadg_nBytes );
        assert ( totalBytes + pHdr->cadg_nBytes <= nBytesToSend );

        char * pDG = reinterpret_cast < char * > ( pHdr + 1 );
        unsigned sizeDG = pHdr->cadg_nBytes - sizeof ( *pHdr );

        if ( pHdr->cadg_addr.isValid() ) {
            outBufClient::flushCondition stat =
                this->osdSend ( pDG, sizeDG, pHdr->cadg_addr );
            if ( stat != outBufClient::flushProgress ) {
                break;
            }
        }

        totalBytes += pHdr->cadg_nBytes;
    }

    if ( totalBytes ) {
        //
        // !! this time fetch may be slowing things down !!
        //
        //this->lastSendTS = epicsTime::getCurrent();
        nBytesSent = totalBytes;
        return outBufClient::flushProgress;
    }
    else {
        return outBufClient::flushNone;
    }
}

//
// casDGClient::xRecv ()
//
inBufClient::fillCondition casDGClient::xRecv (char *pBufIn, bufSizeT nBytesToRecv,
        fillParameter parm, bufSizeT &nByesRecv)
{
    const char *pAfter = pBufIn + nBytesToRecv;
    char *pCurBuf = pBufIn;
    bufSizeT nDGBytesRecv;
    inBufClient::fillCondition stat;
    cadg *pHdr;

    while (pAfter-pCurBuf >= static_cast<int>(MAX_UDP_RECV+sizeof(cadg))) {
        pHdr = reinterpret_cast < cadg * > ( pCurBuf );
        stat = this->osdRecv ( reinterpret_cast < char * > ( pHdr + 1 ),
            MAX_UDP_RECV, parm, nDGBytesRecv, pHdr->cadg_addr);
        if (stat==casFillProgress) {
            pHdr->cadg_nBytes = nDGBytesRecv + sizeof(*pHdr);
            pCurBuf += pHdr->cadg_nBytes;
            //
            // !! this time fetch may be slowing things down !!
            //
            //this->lastRecvTS = epicsTime::getCurrent();
        }
        else {
            break;
        }
    }

    nDGBytesRecv = pCurBuf - pBufIn;
    if (nDGBytesRecv) {
        nByesRecv = nDGBytesRecv;
        return casFillProgress;
    }
    else {
        return casFillNone;
    }
}

//
// casDGClient::asyncSearchResp()
//
//
// this results in many small UDP frames which unfortunately
// isnt particularly efficient
//
caStatus casDGClient::asyncSearchResponse (
    epicsGuard < casClientMutex > &, const caNetAddr & outAddr,
    const caHdrLargeArray & msg, const pvExistReturn & retVal,
    ca_uint16_t protocolRevision, ca_uint32_t sequenceNumber )
{
    if ( retVal.getStatus() != pverExistsHere ) {
        return S_cas_success;
    }

    void * pRaw;
    const outBufCtx outctx = this->out.pushCtx
                    ( sizeof(cadg), MAX_UDP_SEND, pRaw );
    if ( outctx.pushResult() != outBufCtx::pushCtxSuccess ) {
        return S_cas_sendBlocked;
    }

    cadg * pRespHdr = static_cast < cadg * > ( pRaw );

    // insert version header at the start of the reply message
    this->sendVersion ();

    caHdr * pMsg = reinterpret_cast < caHdr * > ( pRespHdr + 1 );
    assert ( ntohs ( pMsg->m_cmmd ) == CA_PROTO_VERSION );
    if ( CA_V411 ( protocolRevision ) ) {
        pMsg->m_cid = htonl ( sequenceNumber );
        pMsg->m_dataType = htons ( sequenceNoIsValid );
    }

    caStatus stat = this->searchResponse ( msg, retVal );

    pRespHdr->cadg_nBytes = this->out.popCtx (outctx) + sizeof ( *pRespHdr );
    if ( pRespHdr->cadg_nBytes > sizeof ( *pRespHdr ) + sizeof (caHdr) ) {
        pRespHdr->cadg_addr = outAddr;
        this->out.commitRawMsg ( pRespHdr->cadg_nBytes );
    }

    return stat;
}

//
// casDGClient::processDG ()
//
caStatus casDGClient::processDG ()
{
    bufSizeT bytesLeft;
    caStatus status;

    status = S_cas_success;
    while ( ( bytesLeft = this->in.bytesPresent() ) ) {
        bufSizeT dgInBytesConsumed;
        const cadg * pReqHdr = reinterpret_cast < cadg * > ( this->in.msgPtr () );

        if (bytesLeft<sizeof(*pReqHdr)) {
            this->in.removeMsg (bytesLeft);
            errlogPrintf ("casDGClient::processMsg: incomplete DG header?");
            status = S_cas_internal;
            break;
        }

        epicsGuard < epicsMutex > guard ( this->mutex );

        //
        // start a DG context in the output protocol stream
        // and grab the send lock
        //
        void *pRaw;
        const outBufCtx outctx = this->out.pushCtx ( sizeof ( cadg ), MAX_UDP_SEND, pRaw );
        if ( outctx.pushResult() != outBufCtx::pushCtxSuccess ) {
            status = S_cas_sendBlocked;
            break;
        }

        // insert version header at the start of the reply message
        this->sendVersion ();

        cadg * pRespHdr = static_cast < cadg * > ( pRaw );

        //
        // select the next DG in the input stream and start processing it
        //
        const bufSizeT reqBodySize = pReqHdr->cadg_nBytes - sizeof (*pReqHdr);

        const inBufCtx inctx = this->in.pushCtx ( sizeof (*pReqHdr), reqBodySize);
        if ( inctx.pushResult() != inBufCtx::pushCtxSuccess ) {
            this->in.removeMsg ( bytesLeft );
            this->out.popCtx ( outctx );
            errlogPrintf ("casDGClient::processMsg: incomplete DG?\n");
            status = S_cas_internal;
            break;
        }

        this->lastRecvAddr = pReqHdr->cadg_addr;
        this->seqNoOfReq = 0;
        this->minor_version_number = 0;

        status = this->processMsg ();
        pRespHdr->cadg_nBytes = this->out.popCtx ( outctx ) + sizeof ( *pRespHdr );
        dgInBytesConsumed = this->in.popCtx ( inctx );

        if ( dgInBytesConsumed > 0 ) {

            //
            // at this point processMsg() bailed out because:
            // a) it used all of the incoming DG or
            // b) it used all of the outgoing DG
            //
            // In either case commit the DG to the protocol stream and
            // release the send lock
            //
            // if there are not additional messages passed the version header
            // then discard the message
            if ( pRespHdr->cadg_nBytes > sizeof ( *pRespHdr ) + sizeof (caHdr) ) {
                pRespHdr->cadg_addr = pReqHdr->cadg_addr;

                caHdr * pMsg = reinterpret_cast < caHdr * > ( pRespHdr + 1 );
                assert ( ntohs ( pMsg->m_cmmd ) == CA_PROTO_VERSION );

                if ( CA_V411 ( this->minor_version_number ) ) {
                    pMsg->m_cid = htonl ( this->seqNoOfReq );
                    pMsg->m_dataType = htons ( sequenceNoIsValid );
                }

                this->out.commitRawMsg ( pRespHdr->cadg_nBytes );
            }

            //
            // check to see that all of the incoming UDP frame was used
            //
            if ( dgInBytesConsumed < reqBodySize ) {
                //
                // remove the bytes in the body that were consumed,
                // but _not_ the header bytes
                //
                this->in.removeMsg (dgInBytesConsumed);

                //
                // slide the UDP header forward and correct the byte count
                //
                {
                    cadg *pReqHdrMove;
                    cadg copy = *pReqHdr;
                    pReqHdrMove = reinterpret_cast < cadg * > ( this->in.msgPtr () );
                    pReqHdrMove->cadg_addr = copy.cadg_addr;
                    pReqHdrMove->cadg_nBytes = copy.cadg_nBytes - dgInBytesConsumed;
                }
            }
            else {
                //
                // remove the header and all of the body
                //
                this->in.removeMsg ( pReqHdr->cadg_nBytes );
            }
        }

        if ( status != S_cas_success ) {
            break;
        }
    }
    return status;
}

//
// casDGClient::getDebugLevel()
//
unsigned casDGClient::getDebugLevel() const
{
    return this->getCAS().getDebugLevel();
}

//
// casDGClient::fetchLastRecvAddr ()
//
caNetAddr casDGClient::fetchLastRecvAddr () const
{
    return this->lastRecvAddr;
}

//
// casDGClient::datagramSequenceNumber ()
//
ca_uint32_t casDGClient::datagramSequenceNumber () const
{
    return this->seqNoOfReq;
}

//
// casDGClient::hostName()
//
void casDGClient::hostName ( char *pBufIn, unsigned bufSizeIn ) const
{
    this->lastRecvAddr.stringConvert ( pBufIn, bufSizeIn );
}

// send minor protocol revision to the client
void casDGClient::sendVersion ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    caStatus status = this->out.copyInHeader ( CA_PROTO_VERSION, 0,
        0, CA_MINOR_PROTOCOL_REVISION, 0, 0, 0 );
    if ( ! status ) {
        this->out.commitMsg ();
    }
}

bool casDGClient::inBufFull () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->in.full ();
}

void casDGClient::inBufFill ( inBufClient::fillParameter parm )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->in.fill ( parm );
}

bufSizeT casDGClient ::
    inBufBytesPending () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->in.bytesPresent ();
}

bufSizeT casDGClient ::
    outBufBytesPending () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->out.bytesPresent ();
}

outBufClient::flushCondition casDGClient::flush ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->out.flush ();
}

//
// casDGClient::processMsg ()
// process any messages in the in buffer
//
caStatus casDGClient::processMsg ()
{
    int status = S_cas_success;

    try {
        unsigned bytesLeft;
        while ( ( bytesLeft = this->in.bytesPresent() ) ) {
            caHdrLargeArray msgTmp;
            unsigned msgSize;
            ca_uint32_t hdrSize;
            char * rawMP;
            {
                //
                // copy as raw bytes in order to avoid
                // alignment problems
                //
                caHdr smallHdr;
                if ( bytesLeft < sizeof ( smallHdr ) ) {
                    break;
                }

                rawMP = this->in.msgPtr ();
                memcpy ( & smallHdr, rawMP, sizeof ( smallHdr ) );

                ca_uint32_t payloadSize = AlignedWireRef < epicsUInt16 > ( smallHdr.m_postsize );
                ca_uint32_t nElem = AlignedWireRef < epicsUInt16 > ( smallHdr.m_count );
                if ( payloadSize != 0xffff && nElem != 0xffff ) {
                    hdrSize = sizeof ( smallHdr );
                }
                else {
                    ca_uint32_t LWA[2];
                    hdrSize = sizeof ( smallHdr ) + sizeof ( LWA );
                    if ( bytesLeft < hdrSize ) {
                        break;
                    }
                    //
                    // copy as raw bytes in order to avoid
                    // alignment problems
                    //
                    memcpy ( LWA, rawMP + sizeof ( caHdr ), sizeof( LWA ) );
                    payloadSize = AlignedWireRef < epicsUInt32 > ( LWA[0] );
                    nElem = AlignedWireRef < epicsUInt32 > ( LWA[1] );
                }

                msgTmp.m_cmmd = AlignedWireRef < epicsUInt16 > ( smallHdr.m_cmmd );
                msgTmp.m_postsize = payloadSize;
                msgTmp.m_dataType = AlignedWireRef < epicsUInt16 > ( smallHdr.m_dataType );
                msgTmp.m_count = nElem;
                msgTmp.m_cid = AlignedWireRef < epicsUInt32 > ( smallHdr.m_cid );
                msgTmp.m_available = AlignedWireRef < epicsUInt32 > ( smallHdr.m_available );

                if ( payloadSize & 0x7 ) {
                    status = this->sendErr (
                        & msgTmp, invalidResID, ECA_INTERNAL,
                        "CAS: Datagram request wasn't 8 byte aligned" );
                    this->in.removeMsg ( bytesLeft );
                    break;
                }

                msgSize = hdrSize + payloadSize;
                if ( bytesLeft < msgSize ) {
                    if ( msgSize > this->in.bufferSize() ) {
                        status = this->sendErr ( & msgTmp, invalidResID, ECA_TOLARGE,
                            "client's request didnt fit within the CA server's message buffer" );
                        this->in.removeMsg ( bytesLeft );
                    }
                    break;
                }

                this->ctx.setMsg ( msgTmp, rawMP + hdrSize );

                if ( this->getCAS().getDebugLevel() > 5u ) {
                    char pHostName[64u];
                    this->lastRecvAddr.stringConvert ( pHostName, sizeof ( pHostName ) );
                    caServerI::dumpMsg ( pHostName, "?",
                        & msgTmp, rawMP + hdrSize, 0 );
                }

            }

            //
            // Reset the context to the default
            // (guarantees that previous message does not get mixed
            // up with the current message)
            //
            this->ctx.setChannel ( NULL );
            this->ctx.setPV ( NULL );

            //
            // Call protocol stub
            //
            casDGClient::pCASMsgHandler pHandler;
            if ( msgTmp.m_cmmd < NELEMENTS ( casDGClient::msgHandlers ) ) {
                pHandler = this->casDGClient::msgHandlers[msgTmp.m_cmmd];
            }
            else {
                pHandler = & casDGClient::uknownMessageAction;
            }
            status = ( this->*pHandler ) ();
            if ( status ) {
                this->in.removeMsg ( this->in.bytesPresent() );
                break;
            }

            this->in.removeMsg ( msgSize );
        }
    }
    catch ( std::exception & except ) {
        this->in.removeMsg ( this->in.bytesPresent() );
        this->sendErr (
            this->ctx.getMsg(), invalidResID, ECA_INTERNAL,
            "C++ exception \"%s\" in CA circuit server",
            except.what () );
        status = S_cas_internal;
    }
    catch (...) {
        this->in.removeMsg ( this->in.bytesPresent() );
        this->sendErr (
            this->ctx.getMsg(), invalidResID, ECA_INTERNAL,
            "unexpected C++ exception in CA datagram server" );
        status = S_cas_internal;
    }

    return status;
}

//
//  casDGClient::sendErr()
//
caStatus casDGClient::sendErr ( const caHdrLargeArray *curp,
    ca_uint32_t cid, const int reportedStatus, const char *pformat, ... )
{
    unsigned stringSize;
    char msgBuf[1024]; /* allocate plenty of space for the message string */
    if ( pformat ) {
        va_list args;
        va_start ( args, pformat );
        int status = vsprintf ( msgBuf, pformat, args );
        if ( status < 0 ) {
            errPrintf (S_cas_internal, __FILE__, __LINE__,
                "bad sendErr(%s)", pformat);
            stringSize = 0u;
        }
        else {
            stringSize = 1u + (unsigned) status;
        }
        va_end ( args );
    }
    else {
        stringSize = 0u;
    }

    unsigned hdrSize = sizeof ( caHdr );
    if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) &&
            CA_V49( this->minor_version_number ) ) {
        hdrSize += 2 * sizeof ( ca_uint32_t );
    }

    caHdr * pReqOut;
    epicsGuard < epicsMutex > guard ( this->mutex );
    caStatus status = this->out.copyInHeader ( CA_PROTO_ERROR,
        hdrSize + stringSize, 0, 0, cid, reportedStatus,
        reinterpret_cast <void **> ( & pReqOut ) );
    if ( ! status ) {
        char * pMsgString;

        /*
         * copy back the request protocol
         * (in network byte order)
         */
        if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) &&
                CA_V49( this->minor_version_number ) ) {
            ca_uint32_t *pLW = ( ca_uint32_t * ) ( pReqOut + 1 );
            pReqOut->m_cmmd = htons ( curp->m_cmmd );
            pReqOut->m_postsize = htons ( 0xffff );
            pReqOut->m_dataType = htons ( curp->m_dataType );
            pReqOut->m_count = htons ( 0u );
            pReqOut->m_cid = htonl ( curp->m_cid );
            pReqOut->m_available = htonl ( curp->m_available );
            pLW[0] = htonl ( curp->m_postsize );
            pLW[1] = htonl ( curp->m_count );
            pMsgString = ( char * ) ( pLW + 2 );
        }
        else {
            pReqOut->m_cmmd = htons (curp->m_cmmd);
            pReqOut->m_postsize = htons ( ( (ca_uint16_t) curp->m_postsize ) );
            pReqOut->m_dataType = htons (curp->m_dataType);
            pReqOut->m_count = htons ( ( (ca_uint16_t) curp->m_count ) );
            pReqOut->m_cid = htonl (curp->m_cid);
            pReqOut->m_available = htonl (curp->m_available);
            pMsgString = ( char * ) ( pReqOut + 1 );
         }

        /*
         * add their context string into the protocol
         */
        memcpy ( pMsgString, msgBuf, stringSize );

        this->out.commitMsg ();
    }

    return S_cas_success;
}


//
// echoAction()
//
caStatus casDGClient::echoAction ()
{
    const caHdrLargeArray * mp = this->ctx.getMsg();
    const void * dp = this->ctx.getData();
    void * pPayloadOut;

    epicsGuard < epicsMutex > guard ( this->mutex );
    caStatus status = this->out.copyInHeader ( mp->m_cmmd, mp->m_postsize,
        mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available,
        & pPayloadOut );
    if ( ! status ) {
        memcpy ( pPayloadOut, dp, mp->m_postsize );
        this->out.commitMsg ();
    }
    return S_cas_success;
}
