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
 * History
 * $Log$
 * Revision 1.15  1998/04/20 18:11:01  jhill
 * better debug mesg
 *
 * Revision 1.14  1998/04/10 23:13:14  jhill
 * fixed byte swap problems, and use default port if server tool returns PV IP addr, but no port
 *
 * Revision 1.13  1998/02/05 22:54:19  jhill
 * moved inline func here
 *
 * Revision 1.12  1997/08/05 00:47:06  jhill
 * fixed warnings
 *
 * Revision 1.11  1997/06/30 22:54:27  jhill
 * use %p with pointers
 *
 * Revision 1.10  1997/06/13 09:15:56  jhill
 * connect proto changes
 *
 * Revision 1.9  1997/04/10 19:34:06  jhill
 * API changes
 *
 * Revision 1.8  1997/01/10 21:17:53  jhill
 * code around gnu g++ inline bug when -O isnt used
 *
 * Revision 1.7  1996/11/02 00:54:08  jhill
 * many improvements
 *
 * Revision 1.6  1996/09/16 18:24:00  jhill
 * vxWorks port changes
 *
 * Revision 1.5  1996/09/04 20:19:47  jhill
 * added missing byte swap on search reply port no
 *
 * Revision 1.4  1996/08/13 22:54:20  jhill
 * fixed little endian problem
 *
 * Revision 1.3  1996/08/05 19:26:15  jhill
 * made os specific code smaller
 *
 * Revision 1.2  1996/06/26 21:18:52  jhill
 * now matches gdd api revisions
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */
#include "gddApps.h"

#include "server.h"
#include "caServerIIL.h" // caServerI inline func
#include "dgOutBufIL.h" // dgOutBuf inline func
#include "dgInBufIL.h" // dgInBuf inline func
#include "casCtxIL.h" // casCtx inline func
#include "casCoreClientIL.h" // casCoreClient inline func

//
// CA Server Datagram (DG) Client
//

//
// casDGClient::casDGClient()
//
casDGClient::casDGClient(caServerI &serverIn) :
	dgInBuf(*(osiMutex *)this, casDGIntfIO::optimumInBufferSize()),
	dgOutBuf(*(osiMutex *)this, casDGIntfIO::optimumOutBufferSize()),
	casClient(serverIn, *this, *this),
	pOutMsgIO(NULL),
	pInMsgIO(NULL)
{
}

//
// casDGClient::init()
//
caStatus casDGClient::init() 
{
	caStatus status;

	status = this->dgInBuf::init();
	if (status) {
		return status;
	}
	status = this->dgOutBuf::init();
	if (status) {
		return status;
	}
	return S_cas_success;
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
void casDGClient::show(unsigned level) const
{
	this->casClient::show(level);
	printf("casDGClient at %p\n", this);
	this->dgInBuf::show(level);
	this->dgOutBuf::show(level);
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

	if (this->ctx.getServer()->getDebugLevel()>2u) {
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
#ifdef vxWorks
#	error code needs to be implemented here when we port
#	error to memory limited environment 
#endif

	//
	// ask the server tool if this PV exists
	//
	this->asyncIOFlag = 0u;
	pvExistReturn pver = 
		(*this->ctx.getServer())->pvExistTest(this->ctx, pChanName);

	//
	// prevent problems when they initiate
	// async IO but dont return status
	// indicating so (and vise versa)
	//
	if (this->asyncIOFlag) {
		pver = pverAsyncCompletion;
	}
	else if (pver.getStatus() == pverAsyncCompletion) {
		pver = pverDoesNotExistHere;
		errMessage(S_cas_badParameter, 
		"- expected asynch IO status from caServer::pvExistTest()");
	}

	//
	// otherwise we assume sync IO operation was initiated
	//
	switch (pver.getStatus()) {
	case pverDoesNotExistHere:
	case pverAsyncCompletion:
		status = S_cas_success;
		break;

	case pverExistsHere:
		status = this->searchResponse(*mp, pver);
		break;

	default:
		status = S_cas_badParameter;
		break;
	}

	return status;
}


//
// caStatus casDGClient::searchResponse()
//
caStatus casDGClient::searchResponse(const caHdr &msg, 
	const pvExistReturn &retVal)
{
	caStatus	status;
	caHdr 		*search_reply;
	unsigned short	*pMinorVersion;

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
	// if we dont have a virtual out msg io pointer
	// then ignore the request
	//
	if (!this->pOutMsgIO) {
		return S_cas_success;
	}

	/*
	 * starting with V4.1 the count field is used (abused)
	 * by the client to store the minor version number of 
	 * the client.
	 *
	 * Old versions expect alloc of channel in response
	 * to a search request. This is no longer supported.
	 */
	if ( !CA_V44(CA_PROTOCOL_VERSION,msg.m_count) ) {
		if (this->ctx.getServer()->getDebugLevel()>0u) {
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

	/*
	 * obtain space for the reply message
	 */
	status = this->allocMsg(sizeof(*pMinorVersion), &search_reply);
	if (status) {
		return status;
	}


	*search_reply = msg;
	search_reply->m_postsize = sizeof(*pMinorVersion);
	/*
	 * cid field is abused to carry the IP 
	 * address in CA_V48 or higher
	 * (this allows a CA servers to serve
	 * as a directory service)
	 *
	 * type field is abused to carry the IP 
	 * port number here CA_V44 or higher
	 * (this allows multiple CA servers on one
	 * host)
	 */
	if (CA_V48(CA_PROTOCOL_VERSION,msg.m_count)) {
		if (retVal.addrIsValid()) {
			caNetAddr addr = retVal.getAddr();
			struct sockaddr_in ina = addr.getSockIP();
			search_reply->m_cid = ntohl (ina.sin_addr.s_addr);
			if (ina.sin_port==0u) {
				//
				// If they dont specify a port number then the default
				// CA server port is assumed when it it is a server
				// address redirect (it is never correct to use this
				// server's port when it is a redirect).
				//
				search_reply->m_type = CA_SERVER_PORT;
			}
			else {
				search_reply->m_type = ntohs (ina.sin_port);
			}
		}
		else {
			search_reply->m_cid = ~0U;
			search_reply->m_type = this->pOutMsgIO->serverPortNumber();
		}
	}
	else {
		search_reply->m_cid = ~0U;
		search_reply->m_type = this->pOutMsgIO->serverPortNumber();
	}

	search_reply->m_count = 0ul;

	/*
	 * Starting with CA V4.1 the minor version number
	 * is appended to the end of each search reply.
	 * This value is ignored by earlier clients. 
	 */
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
void casDGClient::sendBeacon(casDGIntfIO &io)
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

	//
	// send it to all addresses on the beacon list
	//
	io.sendBeacon(buf, sizeof(msg), msg.m_available, msg.m_count);
}

//
// casDGClient::ioBlockedSignal()
//
void casDGClient::ioBlockedSignal() 
{
	//
	// NOOP
	//
	// The DG client never blocks - if we exceed the 
	// max simultaneous io in progress limit 
	// then we discard the current request 
	// (the client will resend later)
	//
}

//
// casDGClient::processDG()
//
void casDGClient::processDG(casDGIntfIO &inMsgIO, casDGIntfIO &outMsgIO)
{
        caStatus                status;
 
	//
	// !! special locking required here in mt case
	//
    //
    // force all replies to be sent to the client
    // that made the request
    //
	this->pInMsgIO = &inMsgIO;
	this->pOutMsgIO = &outMsgIO;
    this->inBuf::clear();
    this->outBuf::clear();

    //
    // read in new input
    //
	if (this->fill() != casFillDisconnect) {

		//
		// verify that we have a message to process
		//
		if (this->inBuf::bytesPresent()>0u) {
			this->setRecipient(this->getSender());

			//
			// process the message
			//
			status = this->processMsg();
			if (status) {
				errMessage (status,
			"- unexpected error processing stateless protocol");
			}
			//
			// force all replies to go to the sender
			//
			if (this->outBuf::bytesPresent()>0u) {
				casVerify (this->flush()==casFlushCompleted);
			}
		}
	}

	//
	// clear the input/output buffers so replies
	// are always sent to the sender of the request
	//
	this->pInMsgIO = NULL;
	this->pOutMsgIO = NULL;
	this->inBuf::clear();
	this->outBuf::clear();
}

//
// casDGClient::asyncSearchResp()
//
caStatus casDGClient::asyncSearchResponse(
	casDGIntfIO &outMsgIO, const caNetAddr &outAddr,
	const caHdr &msg, const pvExistReturn &retVal)
{
	caStatus stat;

	//
	// !! special locking required here in mt case
	//
	this->pOutMsgIO = &outMsgIO;
	this->dgOutBuf::clear();
	this->setRecipient(outAddr);

	stat = this->searchResponse(msg, retVal);

	this->dgOutBuf::flush();
	this->dgOutBuf::clear();
	this->pOutMsgIO = NULL;

	return stat;
}

//
// casDGClient::xDGSend()
//
xSendStatus casDGClient::xDGSend (char *pBufIn, bufSizeT nBytesNeedToBeSent, 
		bufSizeT &nBytesSent, const caNetAddr &recipient)
{
	xSendStatus stat;

	if (!this->pOutMsgIO) {
		return xSendDisconnect;
	}
	stat = this->pOutMsgIO->osdSend(pBufIn, nBytesNeedToBeSent, 
				nBytesSent, recipient);	
	if (stat==xSendOK) {
		//
		// !! this time fetch may be slowing things down !!
		//
		this->elapsedAtLastSend = osiTime::getCurrent();
	}
	return stat;
}

//
// casDGClient::xDGRecv()
//
xRecvStatus casDGClient::xDGRecv (char *pBufIn, bufSizeT nBytesToRecv,
			bufSizeT &nByesRecv, caNetAddr &sender)
{
	xRecvStatus stat;

	if (!this->pInMsgIO) {
		return xRecvDisconnect;
	}
	stat = this->pInMsgIO->osdRecv(pBufIn, nBytesToRecv, 
						nByesRecv, sender);
	if (stat==xRecvOK) {
		//
		// !! this time fetch may be slowing things down !!
		//
		this->elapsedAtLastRecv = osiTime::getCurrent();
	}
	return stat;
}

//
// casDGClient::incommingBytesPresent()
//
bufSizeT casDGClient::incommingBytesPresent() const
{
	//
	// !!!! perhaps this would run faster if we
	// !!!! checked to see if UDP frames are in
	// !!!! the queue from the same client
	//
	return 0u;
}

//
// casDGClient::getDebugLevel()
//
unsigned casDGClient::getDebugLevel() const
{
	return this->getCAS().getDebugLevel();
}

//
// casDGClient::fetchRespAddr()
//
caNetAddr casDGClient::fetchRespAddr()
{
	return this->getRecipient();
}
 
//
// casDGClient::fetchOutIntf()
//
casDGIntfIO* casDGClient::fetchOutIntf()
{
	return this->pOutMsgIO;
}

