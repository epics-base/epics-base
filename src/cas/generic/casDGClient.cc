/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 */
#include "gddApps.h"

#include "server.h"
#include "caServerIIL.h" // caServerI inline func
#include "outBufIL.h" // inline func for outBuf
#include "dgInBufIL.h" // dgInBuf inline func
#include "casCtxIL.h" // casCtx inline func
#include "casCoreClientIL.h" // casCoreClient inline func
#include "osiPoolStatus.h" // osi pool monitoring functions

//
// CA Server Datagram (DG) Client
//

//
// casDGClient::casDGClient()
//
casDGClient::casDGClient (caServerI &serverIn) :
	casClient (serverIn, MAX_UDP+sizeof(cadg))
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
	this->casClient::show(level);
	printf("casDGClient at %p\n", this);
	if (level>=1u) {
		char buf[64];
		this->clientHostName (buf, sizeof(buf));
        	printf ("Client Host=%s\n", buf);
	}
	this->inBuf::show(level);
}

//
// casDGClient::uknownMessageAction()
//
caStatus casDGClient::uknownMessageAction ()
{
	const caHdr *mp = this->ctx.getMsg();

	ca_printf ("CAS: bad request code in DG =%u\n", mp->m_cmmd);
	this->dumpMsg (mp, this->ctx.getData());

	return S_cas_internal;
}

//
// casDGClient::searchAction()
//
caStatus casDGClient::searchAction()
{
	const caHdr	*mp = this->ctx.getMsg();
	void		*dp = this->ctx.getData();
	const char	*pChanName = (const char *) dp;
	caStatus	status;

	if (this->getCAS().getDebugLevel()>2u) {
		char pName[64u];
		this->clientHostName (pName, sizeof (pName));
		printf("%s is searching for \"%s\"\n", pName, pChanName);
	}

	//
	// verify that we have sufficent memory for a PV and a
	// monitor prior to calling PV exist test so that when
	// the server runs out of memory we dont reply to
	// search requests, and therefore dont thrash through
	// caServer::pvExistTest() and casCreatePV::pvAttach()
	//
    if (!osiSufficentSpaceInPool()) {
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
		    status =  this->searchResponse (*mp, pver);
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
caStatus casDGClient::searchResponse(const caHdr &msg, 
                                     const pvExistReturn &retVal)
{
    caStatus            status;
    caHdr               *search_reply;
    struct sockaddr_in  ina;
    unsigned short      *pMinorVersion;
    
    //
    // normal search failure is ignored
    //
    if (retVal.getStatus()==pverDoesNotExistHere) {
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
    if ( !CA_V44(CA_PROTOCOL_VERSION,msg.m_count) ) {
        if (this->getCAS().getDebugLevel()>0u) {
            char pName[64u];
            this->clientHostName (pName, sizeof (pName));
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
    // obtain space for the reply message
    //
    status = this->allocMsg(sizeof(*pMinorVersion), &search_reply);
    if (status) {
        return status;
    }
    
    *search_reply = msg;
    search_reply->m_postsize = sizeof(*pMinorVersion);
    //
    // cid field is abused to carry the IP 
    // address in CA_V48 or higher
    // (this allows a CA servers to serve
    // as a directory service)
    //
    // type field is abused to carry the IP 
    // port number here CA_V44 or higher
    // (this allows multiple CA servers on one
    // host)
    //
    if (CA_V48(CA_PROTOCOL_VERSION,msg.m_count)) {        
        if (retVal.addrIsValid()) {
            caNetAddr addr = retVal.getAddr();
            ina = addr.getSockIP();
            //
            // If they dont specify a port number then the default
            // CA server port is assumed when it it is a server
            // address redirect (it is never correct to use this
            // server's port when it is a redirect).
            //
            if (ina.sin_port==0u) {
                ina.sin_port = htons (CA_SERVER_PORT);
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
                ina.sin_addr.s_addr = ntohl(~0U);
            }
        }
        search_reply->m_cid = ntohl (ina.sin_addr.s_addr);
        search_reply->m_dataType = ntohs (ina.sin_port);
    }
    else {
        caNetAddr addr = this->serverAddress ();
        struct sockaddr_in ina = addr.getSockIP();
        search_reply->m_cid = ~0U;
        search_reply->m_dataType = ntohs (ina.sin_port);
    }
    
    search_reply->m_count = 0ul;
    
    //
    // Starting with CA V4.1 the minor version number
    // is appended to the end of each search reply.
    // This value is ignored by earlier clients. 
    //
    pMinorVersion = (unsigned short *) (search_reply+1);
    *pMinorVersion = htons (CA_MINOR_VERSION);
    
    this->commitMsg();
    
    return S_cas_success;
}

//
// 	casDGClient::searchFailResponse()
//	(only when requested by the client
//	- when it isnt a reply to a broadcast)
//
caStatus casDGClient::searchFailResponse(const caHdr *mp)
{
	int		status;
	caHdr 	*reply;

	status = this->allocMsg(0u, &reply);
	if(status){
		return status;
	}

	*reply = *mp;
	reply->m_cmmd = CA_PROTO_NOT_FOUND;
	reply->m_postsize = 0u;

	this->commitMsg();

	return S_cas_success;
}

//
// casDGClient::sendBeacon()
// (implemented here because this has knowledge of the protocol)
//
void casDGClient::sendBeacon ()
{
	union {
		caHdr	msg;
		char	buf;
	};

	//
	// create the message
	//
	memset(&buf, 0, sizeof(msg));
	msg.m_cmmd = htons(CA_PROTO_RSRV_IS_UP);
    msg.m_available = htonl (INADDR_ANY);

	//
	// send it to all addresses on the beacon list
	//
	this->sendBeaconIO (buf, sizeof(msg), msg.m_count);
}

//
// casDGClient::xSend()
//
outBuf::flushCondition casDGClient::xSend (char *pBufIn,
        bufSizeT nBytesAvailableToSend, bufSizeT nBytesNeedToBeSent,
        bufSizeT &nBytesSent)
{
    outBuf::flushCondition stat;
    bufSizeT totalBytes;
    char *pBuf = pBufIn;
    cadg *pHdr;

    assert (nBytesAvailableToSend>=nBytesNeedToBeSent);

    totalBytes = 0;
    while (1) {
        pHdr = reinterpret_cast<cadg *>(&pBuf[totalBytes]);

        assert (totalBytes<=bufSizeT_MAX-pHdr->cadg_nBytes);
        assert (totalBytes+pHdr->cadg_nBytes<=nBytesAvailableToSend);

        if (pHdr->cadg_addr.isValid()) {
	        stat = this->osdSend (reinterpret_cast<char *>(pHdr+1), 
                pHdr->cadg_nBytes-sizeof(*pHdr), pHdr->cadg_addr);	
	        if (stat!=outBuf::flushProgress) {
                 break;
            }
        }

        totalBytes += pHdr->cadg_nBytes;

        if (totalBytes>=nBytesNeedToBeSent) {
            break;
        }
    }

    if (totalBytes) {
		//
		// !! this time fetch may be slowing things down !!
		//
		//this->lastSendTS = osiTime::getCurrent();
        nBytesSent = totalBytes;
        return outBuf::flushProgress;
    }
    else {
        return outBuf::flushNone;
    }
}

//
// casDGClient::xRecv ()
//
inBuf::fillCondition casDGClient::xRecv (char *pBufIn, bufSizeT nBytesToRecv,
        fillParameter parm, bufSizeT &nByesRecv)
{
    const char *pAfter = pBufIn + nBytesToRecv;
    char *pBuf = pBufIn;
    bufSizeT nDGBytesRecv;
    inBuf::fillCondition stat;
    cadg *pHdr;

    while (pAfter-pBuf >= static_cast<int>(MAX_UDP+sizeof(cadg))) {
        pHdr = reinterpret_cast<cadg *>(pBuf);
	    stat = this->osdRecv (reinterpret_cast<char *>(pHdr+1), MAX_UDP, parm, 
            nDGBytesRecv, pHdr->cadg_addr);
	    if (stat==casFillProgress) {
            pHdr->cadg_nBytes = nDGBytesRecv + sizeof(*pHdr);
            pBuf += pHdr->cadg_nBytes;
		    //
		    // !! this time fetch may be slowing things down !!
		    //
		    //this->lastRecvTS = osiTime::getCurrent();
	    }
        else {
            break;
        }
    }

    nDGBytesRecv = pBuf - pBufIn;
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
caStatus casDGClient::asyncSearchResponse (const caNetAddr &outAddr,
	const caHdr &msg, const pvExistReturn &retVal)
{
	caStatus stat;

    //
    // start a DG context in the output protocol stream
    // and grab the send lock
    //
    void *pRaw;
    const outBufCtx outctx = this->outBuf::pushCtx 
                    (sizeof(cadg), MAX_UDP, pRaw);
    if (outctx.pushResult()!=outBufCtx::pushCtxSuccess) {
        return S_cas_sendBlocked;
    }

    cadg *pRespHdr = reinterpret_cast<cadg *>(pRaw);
	stat = this->searchResponse (msg, retVal);

    pRespHdr->cadg_nBytes = this->outBuf::popCtx (outctx);
    if (pRespHdr->cadg_nBytes>0) {
        pRespHdr->cadg_addr = outAddr;
        this->outBuf::commitRawMsg (pRespHdr->cadg_nBytes);
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
    while ( (bytesLeft = this->inBuf::bytesPresent()) ) {
        bufSizeT dgInBytesConsumed;
        const cadg *pReqHdr = reinterpret_cast<cadg *>(this->inBuf::msgPtr ());

        if (bytesLeft<sizeof(*pReqHdr)) {
            this->inBuf::removeMsg (bytesLeft);
            ca_printf ("casDGClient::processMsg: incomplete DG header?");
            status = S_cas_internal;
            break;
        }

        //
        // start a DG context in the output protocol stream
        // and grab the send lock
        //
        void *pRaw;
        const outBufCtx outctx = this->outBuf::pushCtx (sizeof(cadg), MAX_UDP, pRaw);
        if ( outctx.pushResult() != outBufCtx::pushCtxSuccess ) {
            status = S_cas_sendBlocked;
            break;
        }
        
        cadg *pRespHdr = reinterpret_cast <cadg *> (pRaw);
        
        //
        // select the next DG in the input stream and start processing it
        //
        const bufSizeT reqBodySize = pReqHdr->cadg_nBytes-sizeof (*pReqHdr);
        const inBufCtx inctx = this->inBuf::pushCtx (sizeof (*pReqHdr), reqBodySize);
        if ( inctx.pushResult() != inBufCtx::pushCtxSuccess ) {
            this->inBuf::removeMsg (bytesLeft);
            this->outBuf::popCtx (outctx);
            ca_printf ("casDGClient::processMsg: incomplete DG?\n");
            status = S_cas_internal;
            break;
        }

        this->lastRecvAddr = pReqHdr->cadg_addr;

        status = this->processMsg ();
        dgInBytesConsumed = this->inBuf::popCtx (inctx);
        if (dgInBytesConsumed>0) {

            //
            // at this point processMsg() bailed out because:
            // a) it used all of the incoming DG or
            // b) it used all of the outgoing DG
            //
            // In either case commit the DG to the protocol stream and 
            // release the send lock
            //
            pRespHdr->cadg_nBytes = this->outBuf::popCtx (outctx) + sizeof(*pRespHdr);
            if (pRespHdr->cadg_nBytes>sizeof(*pRespHdr)) {
                pRespHdr->cadg_addr = pReqHdr->cadg_addr;
                this->outBuf::commitRawMsg (pRespHdr->cadg_nBytes);
            }

            //
            // check to see that all of the incoming UDP frame was used
            //
            if (dgInBytesConsumed<reqBodySize) {
                //
                // remove the bytes in the body that were consumed,
                // but _not_ the header bytes
                //
                this->inBuf::removeMsg (dgInBytesConsumed);

                //
                // slide the UDP header forward and correct the byte count
                //
                {
                    cadg *pReqHdrMove;
                    cadg copy = *pReqHdr;
                    pReqHdrMove = reinterpret_cast <cadg *> (this->inBuf::msgPtr ());
                    pReqHdrMove->cadg_addr = copy.cadg_addr;
                    pReqHdrMove->cadg_nBytes = copy.cadg_nBytes - dgInBytesConsumed;
                }
            }
            else {
                //
                // remove the header and all of the body
                //
                this->inBuf::removeMsg ( pReqHdr->cadg_nBytes );
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
// casDGClient::clientHostName()
//
void casDGClient::clientHostName (char *pBufIn, unsigned bufSizeIn) const
{
    this->lastRecvAddr.stringConvert (pBufIn, bufSizeIn);
}
