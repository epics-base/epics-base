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
#include <gddApps.h>

//
// CA Server Datagram (DG) Client
//

//
// casDGClient::casDGClient()
//
casDGClient::casDGClient(caServerI &serverIn) :
	casClient(serverIn, * (casDGIO *) this)
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
void casDGClient::show(unsigned level)
{
	this->casClient::show(level);
	printf("casDGClient at %x\n", (unsigned) this);
}

//
// casDGClient::searchAction()
//
caStatus casDGClient::searchAction()
{
	const caHdr	*mp = this->ctx.getMsg();
	void		*dp = this->ctx.getData();
	const char	*pChanName = (const char *)dp;
	gdd		*pCanonicalName;
	caStatus	status;
	int 		gddStatus;

	if (this->ctx.getServer()->getDebugLevel()>2u) {
		printf("client is searching for \"%s\"\n", pChanName);
	}

	//
	// If we cant allocate a gdd large enogh to hold the
	// longest PV name then just ignore this request
	// (and let the client to try again later)
	//
	pCanonicalName = new gddScalar(gddAppType_name, aitEnumString);
	if (!pCanonicalName) {
		return S_cas_success;
	}

	//
	// ask the server tool if this PV exists
	//
	status = this->ctx.getServer()->pvExistTest(this->ctx, 
			pChanName, *pCanonicalName);
	if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status==S_cas_ioBlocked) {
		//
		// If too many exist test IO operations are in progress
		// then we will just ignore this request (and wait for
		// the client to try again later)
		//
		status = S_cas_success;
	}
	else {
		status = this->searchResponse(NULL, *mp,
				pCanonicalName, status);
	}

	//
	// delete the PV name object
	//
	gddStatus = pCanonicalName->unreference();
	assert(gddStatus==0);

	return S_cas_success;
}


//
// caStatus casDGClient::searchResponse()
//
caStatus casDGClient::searchResponse(casChannelI *nullPtr, const caHdr &msg,
		gdd *pCanonicalName, const caStatus completionStatus)
{
	caStatus	status;
	caHdr 		*search_reply;
	unsigned short	*pMinorVersion;

	assert(nullPtr==NULL);

	this->ctx.getServer()->pvExistTestCompletion();

	//
	// normal search failure is ignored
	//
	if (completionStatus==S_casApp_pvNotFound) {
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
	if (!pCanonicalName) {
		errMessage(S_cas_badParameter, "PV name descr is nill");
		return S_cas_success;
	}
	if (completionStatus) {
		errMessage(completionStatus,NULL);
		return S_cas_success;
	}

	if (this->ctx.getServer()->getDebugLevel()>2u) {
		char	*pCN;
		pCN = *pCanonicalName;
		if (pCN) {
			printf("Search request matched for PV=\"%s\"\n", pCN);
		}
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
	search_reply->m_type = this->ctx.getServer()->serverPortNumber();
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
// caServerI::sendBeacon()
// (implemented here because this has knowledge of the protocol)
//
void caServerI::sendBeacon()
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
	this->dgClient.sendDGBeacon(buf, sizeof(msg), msg.m_available);

	//
	// double the period between beacons (but dont exceed max)
	//
	this->advanceBeaconPeriod();
}

//
// casDGClient::ioSignal()
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
// casDGClient::process()
//
void casDGClient::process()
{
        caStatus                status;
        casFlushCondition       flushCond;
        casFillCondition        fillCond;
 
        //
        // force all replies to be sent to the client
        // that made the request
        //
        this->inBuf::clear();
        this->outBuf::clear();
 
        //
        // read in new input
        //
        fillCond = this->fill();
        if (fillCond == casFillDisconnect) {
                casVerify(0);
        }
        //
        // verify that we have a message to process
        //
        else if (this->inBuf::bytesPresent()>0u) {
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
		flushCond = this->flush();
		if (flushCond!=casFlushCompleted) {
			casVerify(0);
		}
        }
	//
	// clear the input/output buffers so replies
	// are always sent to the sender of the request
	//
	this->inBuf::clear();
	this->outBuf::clear();
}

