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
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */
#include "gddApps.h"

#include "server.h"
#include "caServerIIL.h" // caServerI inline func
#include "inBufIL.h" // inline functions for inBuf
#include "outBufIL.h" // inline func for outBuf
#include "casCtxIL.h" // casCtx inline func
#include "casCoreClientIL.h" // casCoreClient inline func
#include "osiPoolStatus.h" // osi pool monitoring functions

//
// CA Server Datagram (DG) Client
//

//
// casDGClient::casDGClient()
//
casDGClient::casDGClient ( caServerI & serverIn ) :
	casClient ( serverIn, MAX_UDP_RECV + sizeof ( cadg ) ),
    seqNoOfReq ( 0 )
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
	}
	this->casClient::show (level);
}

//
// casDGClient::uknownMessageAction()
//
caStatus casDGClient::uknownMessageAction ()
{
	const caHdrLargeArray * mp = this->ctx.getMsg();

	this->dumpMsg ( mp, this->ctx.getData(), 
        "bad request code=%u in DG\n", mp->m_cmmd );

	return S_cas_internal;
}

//
// casDGClient::searchAction()
//
caStatus casDGClient::searchAction()
{
	const caHdrLargeArray	*mp = this->ctx.getMsg();
    const char              *pChanName = static_cast <char * > ( this->ctx.getData() );
	caStatus	            status;

    //
    // check the sanity of the message
    //
    if ( mp->m_postsize <= 1 ) {
	    this->dumpMsg ( mp, this->ctx.getData(), 
            "empty PV name extension in UDP search request?\n" );
        return S_cas_success;
    }

    if ( pChanName[0] == '\0' ) {
	    this->dumpMsg ( mp, this->ctx.getData(), 
            "zero length PV name in UDP search request?\n" );
        return S_cas_success;
    }

    // check for an unterminated string before calling server tool
    // by searching backwards through the string (some early versions 
    // of the client library might not be setting the pad bytes to nill)
    for ( unsigned i = mp->m_postsize-1; pChanName[i] != '\0'; i-- ) {
        if ( i <= 1 ) {
	        this->dumpMsg ( mp, this->ctx.getData(), 
                "unterminated PV name in UDP search request?\n" );
            return S_cas_success;
        }
    }

	if ( this->getCAS().getDebugLevel() > 2u ) {
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
    if (!osiSufficentSpaceInPool(0)) {
        return S_cas_success;
    }

	//
	// ask the server tool if this PV exists
	//
	this->asyncIOFlag = 0u;
	pvExistReturn pver = 
		this->getCAS()->pvExistTest(this->ctx, pChanName);

	//
	// prevent problems when they initiate
	// async IO but dont return status
	// indicating so (and vise versa)
	//
	if (this->asyncIOFlag) {
        if (pver.getStatus()!=pverAsyncCompletion) {
		    errMessage (S_cas_badParameter, 
		        "- assuming asynch IO status from caServer::pvExistTest()");
        }
        status = S_cas_success;
	}
	else {
	    //
	    // otherwise we assume sync IO operation was initiated
	    //
        switch (pver.getStatus()) {
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
     
    //
    // normal search failure is ignored
    //
    if ( retVal.getStatus() == pverDoesNotExistHere ) {
        return S_cas_success;
    }
    
    if (retVal.getStatus()!=pverExistsHere) {
        fprintf(stderr, 
            "async exist completion with invalid return code \"pverAsynchCompletion\"?\n");
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
        if (this->getCAS().getDebugLevel()>0u) {
            char pName[64u];
            this->hostName (pName, sizeof (pName));
            printf("client \"%s\" using EPICS R3.11 CA connect protocol was ignored\n", pName);
        }
        //
        // old connect protocol was dropped when the
        // new API was added to the server (they must
        // now use clients at EPICS 3.12 or higher)
        //
        status = this->sendErr(&msg, ECA_DEFUNCT, 
            "R3.11 connect sequence from old client was ignored");
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
                ina.sin_port = epicsHTON16 (static_cast <unsigned short> (CA_SERVER_PORT));
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
                ina.sin_addr.s_addr = epicsNTOH32 (~0U);
            }
        }
        serverAddr = epicsNTOH32 (ina.sin_addr.s_addr);
        serverPort = epicsNTOH16 (ina.sin_port);
    }
    else {
        caNetAddr addr = this->serverAddress ();
        struct sockaddr_in inetAddr = addr.getSockIP();
        serverAddr = ~0U;
        serverPort = epicsNTOH16 ( inetAddr.sin_port );
    }
    
    ca_uint16_t * pMinorVersion;
    status = this->out.copyInHeader ( CA_PROTO_SEARCH, 
        sizeof ( *pMinorVersion ), serverPort, 0, 
        serverAddr, msg.m_available, 
        reinterpret_cast <void **> ( &pMinorVersion ) );

    //
    // Starting with CA V4.1 the minor version number
    // is appended to the end of each search reply.
    // This value is ignored by earlier clients. 
    //
    *pMinorVersion = epicsHTON16 ( CA_MINOR_PROTOCOL_REVISION );
    
    this->out.commitMsg ();
    
    return S_cas_success;
}

//
// 	casDGClient::searchFailResponse()
//	(only when requested by the client
//	- when it isnt a reply to a broadcast)
//
caStatus casDGClient::searchFailResponse ( const caHdrLargeArray * mp )
{
	int		status;

    status = this->out.copyInHeader ( CA_PROTO_NOT_FOUND, 0,
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

    if ( mp->m_count != 0 ) {
        this->minor_version_number = mp->m_count;
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
		caHdr	msg;
		char	buf;
	};

	//
	// create the message
	//
	memset ( & buf, 0, sizeof ( msg ) );
    msg.m_cmmd = epicsHTON16 ( CA_PROTO_RSRV_IS_UP );
    msg.m_dataType = epicsHTON16 ( CA_MINOR_PROTOCOL_REVISION );
    msg.m_cid = epicsHTON32 ( beaconNumber );

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
outBufClient::flushCondition casDGClient::xSend ( char *pBufIn, // X aCC 361
        bufSizeT nBytesAvailableToSend, bufSizeT nBytesNeedToBeSent,
        bufSizeT &nBytesSent ) 
{
    assert ( nBytesAvailableToSend >= nBytesNeedToBeSent );

    bufSizeT totalBytes = 0;
    while ( totalBytes < nBytesNeedToBeSent ) {
        cadg *pHdr = reinterpret_cast<cadg *>(&pBufIn[totalBytes]);

        assert ( totalBytes <= bufSizeT_MAX-pHdr->cadg_nBytes );
        assert ( totalBytes+pHdr->cadg_nBytes <= nBytesAvailableToSend );

        if ( pHdr->cadg_addr.isValid() ) {

            char * pDG = reinterpret_cast < char * > ( pHdr + 1 );
            unsigned sizeDG = pHdr->cadg_nBytes - sizeof ( *pHdr );
            caHdr * pMsg = reinterpret_cast < caHdr * > ( pDG );
            assert ( ntohs ( pMsg->m_cmmd ) == CA_PROTO_VERSION );
            if ( CA_V411 ( this->minor_version_number ) ) {
                pMsg->m_cid = htonl ( this->seqNoOfReq );
                pMsg->m_dataType = htons ( sequenceNoIsValid );
            }
            else {
                pDG += sizeof (caHdr);
                sizeDG -= sizeof (caHdr);
            }

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
inBufClient::fillCondition casDGClient::xRecv (char *pBufIn, bufSizeT nBytesToRecv, // X aCC 361
        fillParameter parm, bufSizeT &nByesRecv)
{
    const char *pAfter = pBufIn + nBytesToRecv;
    char *pCurBuf = pBufIn;
    bufSizeT nDGBytesRecv;
    inBufClient::fillCondition stat;
    cadg *pHdr;

    while (pAfter-pCurBuf >= static_cast<int>(MAX_UDP_RECV+sizeof(cadg))) {
        pHdr = reinterpret_cast<cadg *>(pCurBuf);
	    stat = this->osdRecv (reinterpret_cast<char *>(pHdr+1), MAX_UDP_RECV, parm, 
            nDGBytesRecv, pHdr->cadg_addr);
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
caStatus casDGClient::asyncSearchResponse ( const caNetAddr & outAddr,
	const caHdrLargeArray & msg, const pvExistReturn & retVal )
{
	caStatus stat;

    //
    // start a DG context in the output protocol stream
    // and grab the send lock
    //
    void * pRaw;
    const outBufCtx outctx = this->out.pushCtx 
                    ( sizeof(cadg), MAX_UDP_SEND, pRaw );
    if ( outctx.pushResult() != outBufCtx::pushCtxSuccess ) {
        return S_cas_sendBlocked;
    }

    cadg * pRespHdr = reinterpret_cast<cadg *> ( pRaw );
	stat = this->searchResponse ( msg, retVal );

    pRespHdr->cadg_nBytes = this->out.popCtx (outctx);
    if ( pRespHdr->cadg_nBytes > 0 ) {
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
        const cadg * pReqHdr = reinterpret_cast<cadg *> ( this->in.msgPtr () );

        if (bytesLeft<sizeof(*pReqHdr)) {
            this->in.removeMsg (bytesLeft);
            errlogPrintf ("casDGClient::processMsg: incomplete DG header?");
            status = S_cas_internal;
            break;
        }

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
        
        cadg *pRespHdr = reinterpret_cast <cadg *> (pRaw);
        
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

        // insert version header at the start of the reply message
        this->sendVersion ();

        status = this->processMsg ();
        dgInBytesConsumed = this->in.popCtx ( inctx );
        if (dgInBytesConsumed>0) {

            //
            // at this point processMsg() bailed out because:
            // a) it used all of the incoming DG or
            // b) it used all of the outgoing DG
            //
            // In either case commit the DG to the protocol stream and 
            // release the send lock
            //
            pRespHdr->cadg_nBytes = this->out.popCtx ( outctx ) + sizeof ( *pRespHdr );
            // if there are not additional messages passed the version header
            // then discard the message
            if ( pRespHdr->cadg_nBytes > sizeof ( *pRespHdr ) + sizeof (caHdr) ) {
                pRespHdr->cadg_addr = pReqHdr->cadg_addr;
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
                    pReqHdrMove = reinterpret_cast <cadg *> ( this->in.msgPtr () );
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

        if (status!=S_cas_success) {
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
// casDGClient::hostName()
//
void casDGClient::hostName ( char *pBufIn, unsigned bufSizeIn ) const
{
    this->lastRecvAddr.stringConvert ( pBufIn, bufSizeIn );
}

void casDGClient::userName ( char * pBuf, unsigned bufSizeIn ) const
{
    if ( bufSizeIn ) {
        strncpy ( pBuf, "?", bufSizeIn );
        pBuf[bufSizeIn - 1] = '\0';
    }
}

