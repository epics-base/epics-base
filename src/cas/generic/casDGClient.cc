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

#include <server.h>
#include <caServerIIL.h> // caServerI inline func
#include <casClientIL.h> // casClient inline func
#include <dgOutBufIL.h> // dgOutBuf inline func
#include <dgInBufIL.h> // dgInBuf inline func
#include <casCtxIL.h> // casCtx inline func
#include <inBufIL.h> // inBuf inline func
#include <outBufIL.h> // outBuf inline func
#include <casCoreClientIL.h> // casCoreClient inline func
#include <gddApps.h>

//
// CA Server Datagram (DG) Client
//

//
// casDGClient::casDGClient()
//
casDGClient::casDGClient(caServerI &serverIn) :
	dgInBuf(*(osiMutex *)this, casDGIntfIO::optimumBufferSize()),
	dgOutBuf(*(osiMutex *)this, casDGIntfIO::optimumBufferSize()),
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
	printf("casDGClient at %x\n", (unsigned) this);
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
		printf("client is searching for \"%s\"\n", pChanName);
	}

	//
	// ask the server tool if this PV exists
	//
	pvExistReturn retVal = 
		this->ctx.getServer()->pvExistTest(this->ctx, pChanName);
	if (retVal.getStatus() == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (retVal.getStatus()==S_cas_ioBlocked) {
		//
		// If too many exist test IO operations are in progress
		// then we will just ignore this request (and wait for
		// the client to try again later)
		//
		status = S_cas_success;
	}
	else {
		status = this->searchResponse(*mp, retVal);
	}

	return S_cas_success;
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

	this->ctx.getServer()->pvExistTestCompletion();

	//
	// normal search failure is ignored
	//
	if (retVal.getStatus()==S_casApp_pvNotFound) {
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
	// check for bad parameters
	//
	if (retVal.getStatus()) {
		errMessage(retVal.getStatus(),NULL);
		return S_cas_success;
	}
	if (retVal.getString()) {
		if (retVal.getString()[0]=='\0') {
			errMessage(S_cas_badParameter, 
				"PV name descr is empty");
			return S_cas_success;
		}
		if (this->ctx.getServer()->getDebugLevel()>2u) {
			printf("Search request matched for PV=\"%s\"\n", 
				retVal.getString());
		}
	}
	else {
		errMessage(S_cas_badParameter, "PV name descr is nill");
		return S_cas_success;
	}

	/*
	 * obtain space for the reply message
	 */
	status = this->allocMsg(sizeof(*pMinorVersion), &search_reply);
	if (status) {
		return status;
	}

	/*
	 * type field is abused to carry the IP 
	 * port number here CA_V44 or higher
	 * (this allows multiple CA servers on one
	 * host)
	 */
	*search_reply = msg;
	search_reply->m_postsize = sizeof(*pMinorVersion);
	search_reply->m_cid = ~0U;
	search_reply->m_type = this->pOutMsgIO->serverPortNumber();
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
	io.sendBeacon(buf, sizeof(msg), msg.m_available);
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
			"unexpected error processing stateless protocol");
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
caStatus casDGClient::asyncSearchResponse(casDGIntfIO &outMsgIO, const caAddr &outAddr,
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
		bufSizeT &nBytesSent, const caAddr &recipient)
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
			bufSizeT &nByesRecv, caAddr &sender)
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
caAddr casDGClient::fetchRespAddr()
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

