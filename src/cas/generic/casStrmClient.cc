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
 * Revision 1.2  1996/06/21 02:30:57  jhill
 * solaris port
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#include <server.h>
#include <dbMapper.h>		// ait to dbr types
#include <gddAppTable.h>        // EPICS application type table
#include <caServerIIL.h>	// caServerI inline functions
#include <casClientIL.h>	// casClient inline functions
#include <casChannelIIL.h>	// casChannelI inline functions
#include <casPVIIL.h>		// casPVI inline functions
#include <gddApps.h>

VERSIONID(casStrmClientcc,"%W% %G%")

static const caHdr nill_msg = {0u,0u,0u,0u,0u,0u};

//
// casStrmClient::verifyRequest()
//
inline caStatus casStrmClient::verifyRequest (casChannelI *&pChan)
{
        const caHdr   *mp = this->ctx.getMsg();
 
        //
        // channel exists for this resource id ?
        //
        pChan = this->resIdToChannel(mp->m_cid);
        if (!pChan) {
                return this->sendErr(mp, ECA_BADCHID, NULL);
        }
 
        //
        // data type out of range ?
        //
        if (mp->m_type>((unsigned)LAST_BUFFER_TYPE)) {
                return this->sendErr(mp, ECA_BADTYPE, NULL);
        }
 
        //
        // element count out of range ?
        //
        if (mp->m_count> pChan->getPVI().nativeCount()) {
                return this->sendErr(mp, ECA_BADCOUNT, NULL);
        }
 
        //
        // If too many IO operations are in progress against this pv 
        // then we will need to wait until later to initiate this
        // request.
        //
        if (pChan->getIOOPSInProgress() >=
                pChan->getPVI()->maxSimultAsyncOps()) {
		this->ioBlocked::setBlocked (pChan->getPVI());
                return S_cas_ioBlocked;
        }
 
        return S_cas_validRequest;
}


//
// casStrmClient::createChannel()
//
inline caStatus casStrmClient::createChannel (const char *pName) 
{
	caStatus	status;
	int		gddStatus;
	gdd		*pCanonicalName;

// set correct appl type here !!!!
	pCanonicalName = new gddAtomic(0u, aitEnumString, 1u);
	if (!pCanonicalName) {
		return S_cas_noMemory;
	}

	/*
	 * Verify that the server still has this channel
	 * and that we have its correct official name.
	 * I assume that we may have found this server by
	 * contacting a name resolution service so we need to
	 * verify that the specified channel still exists here.
	 */
	status = this->ctx.getServer()->pvExistTest(
			this->ctx, pName, *pCanonicalName);
	if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status == S_cas_ioBlocked) {
		this->ioBlocked::setBlocked (this->getCAS());
	}
	else {
		status = createChanResponse (NULL, *this->ctx.getMsg(),
				pCanonicalName, status);
	}
	gddStatus = pCanonicalName->unreference ();
	assert (!gddStatus);
	return status;
}



//
// casStrmClient::casStrmClient()
//
casStrmClient::casStrmClient(caServerI &serverInternal, casMsgIO &ioIn) :
	casClient(serverInternal, ioIn)
{
	assert(&ioIn);
	this->ctx.getServer()->installClient(this);

	this->pUserName = NULL;
	this->pHostName = NULL;
	this->ioBlockCache = NULL;
}

//
// casStrmClient::init()
//
caStatus casStrmClient::init()
{
        caStatus status;
 
	status = casClient::init();
	if (status) {
		return status;
	}

        this->pHostName = new char [1u];
        if (!this->pHostName) {
                return S_cas_noMemory;
        }
        *this->pHostName = '\0';
 
        this->pUserName = new char [1u];
        if (!this->pUserName) {
                return S_cas_noMemory;
        }
        *this->pUserName= '\0';
 
	return this->start();
}


//
// casStrmClient::~casStrmClient()
//
casStrmClient::~casStrmClient()
{
	casChannelI		*pChan;
	casChannelI		*pNextChan;
	tsDLIter<casChannelI>	iter(this->chanList);

	//
	// remove this from the list of connected clients
	//
	this->ctx.getServer()->removeClient(this);

        if (this->pUserName) {
                delete [] this->pUserName;
        }
 
        if (this->pHostName) {
                delete [] this->pHostName;
        }

        this->lock();
 
	//
	// delete all channel attached
	//
	pChan = iter();
	while (pChan) {
		//
		// destroying the channel removes it from the list
		//
		pNextChan = iter();
		pChan->clientDestroy();
		pChan = pNextChan;
        }
}

//
// find the monitor associated with a resource id
//
inline casClientMon *caServerI::resIdToClientMon(const caResId &idIn)
{
	casRes *pRes;

	pRes = this->lookupRes(idIn, casClientMonT);
	//
	// cast is ok since the type code was verified 
	// (and we know casClientMon derived from resource)
	//
	return (casClientMon *) pRes;
}


//
// casStrmClient::show (unsigned level)
//
void casStrmClient::show (unsigned level)
{
	this->casClient::show (level);
	printf ("casStrmClient at %x\n", (unsigned) this);
	if (level > 1u) {
		printf ("\tuser %s at %s\n", this->pUserName, this->pHostName);
	}
}


/*
 * casStrmClient::readAction()
 */
caStatus casStrmClient::readAction ()
{
	const caHdr 	*mp = this->ctx.getMsg();
	caStatus	status;
	casChannelI	*pChan;
	gdd		*pDesc;

	status = this->verifyRequest (pChan);
	if (status != S_cas_validRequest) {
		return status;
	}

	/*
	 * verify read access
	 */
	if ((*pChan)->readAccess()!=aitTrue) {
		int	v41;

		v41 = CA_V41(CA_PROTOCOL_VERSION,this->minor_version_number);
		if(v41){
			status = ECA_NORDACCESS;
		}
		else{
			status = ECA_GETFAIL;
		}

		return this->sendErr(mp, status, "read access denied");
	}

	pDesc = NULL;
	status = this->read(pDesc); 
	if (status==S_casApp_success && pDesc) {
		status = this->readResponse(pChan, *mp, pDesc, S_cas_success);
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else {
		status = this->sendErrWithEpicsStatus(mp, status, ECA_GETFAIL);
	}

	if (pDesc) {
		int gddStatus;

		gddStatus = pDesc->unreference();
		assert(gddStatus==0);
	}

	return status;
}



//
// casStrmClient::readResponse()
//
caStatus casStrmClient::readResponse (casChannelI *pChan, const caHdr &msg, 
					gdd *pDesc, const caStatus status)
{
	caHdr 		*reply;
	unsigned	size;
	int        	localStatus;
	int        	strcnt;

	if (status!=S_casApp_success) {
		return this->sendErrWithEpicsStatus(&msg, status, ECA_GETFAIL);
	}

	//
	// must have a descriptor if status is S_casApp_success 
	//
	else if (!pDesc) {
		return this->sendErrWithEpicsStatus(&msg, 
				S_cas_badParameter, ECA_GETFAIL);
	}

	size = dbr_size_n (msg.m_type, msg.m_count);
	localStatus = this->allocMsg(size, &reply);
	if (localStatus) {
		if (localStatus==S_cas_hugeRequest) {
			localStatus = sendErr(&msg, ECA_TOLARGE, NULL);
		}
		return localStatus;
	}

	//
	// setup response message
	//
	*reply = msg; 
	reply->m_postsize = size;
	reply->m_cid = pChan->getCID();

	//
	// convert gdd to db_access type
	// (places the data in network format)
	//
	gddMapDbr[msg.m_type].conv_dbr((reply+1), pDesc);

	//
	// force string message size to be the true size rounded to even
	// boundary
	//
	if (msg.m_type == DBR_STRING && msg.m_count == 1u) {
		/* add 1 so that the string terminator will be shipped */
		strcnt = strlen((char *)(reply + 1u)) + 1u;
		reply->m_postsize = strcnt;
	}

	this->commitMsg ();

	return localStatus;
}


//
// casStrmClient::readNotifyAction()
//
caStatus casStrmClient::readNotifyAction ()
{
	const caHdr 	*mp = this->ctx.getMsg();
	int		status;
	casChannelI	*pChan;
	gdd		*pDesc;

	status = this->verifyRequest (pChan);
	if (status != S_cas_validRequest) {
		return status;
	}

	//
	// verify read access
	// 
	if ((*pChan)->readAccess()!=aitTrue) {
		return this->readNotifyResponse(pChan, *mp, NULL, S_cas_noRead);
	}

	pDesc = NULL;
	status = this->read(pDesc); 
	if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else {
		status = this->readNotifyResponse(pChan, *mp, pDesc, status);
	}

	if (pDesc) {
		int gddStatus;
		gddStatus = pDesc->unreference();
		assert(gddStatus==0);
	}

	return status;
}



//
// casStrmClient::readNotifyResponse()
//
caStatus casStrmClient::readNotifyResponse (casChannelI *, 
		const caHdr &msg, gdd *pDesc, const caStatus completionStatus)
{
	caHdr 		*reply;
	unsigned	size;
	caStatus	status;
	int        	strcnt;

	size = dbr_size_n (msg.m_type, msg.m_count);
	status = this->allocMsg(size, &reply);
	if (status) {
		if (status==S_cas_hugeRequest) {
			//
			// All read notify responses must include a buffer of
			// the size they specify - otherwise an exception
			// is generated
			//
			status = sendErr(&msg, ECA_TOLARGE, NULL);
		}
		return status;
	}

	//
	// setup response message
	//
	*reply = msg; 
	reply->m_postsize = size;

	//
	// cid field abused to store the status here
	//
	if (completionStatus == S_cas_success) {
		if (pDesc) {
			//
			// convert gdd to db_access type
			// (places the data in network format)
			//
			gddMapDbr[msg.m_type].conv_dbr((reply+1), pDesc);
			reply->m_cid = ECA_NORMAL;
		}
		else {
			errMessage(S_cas_badParameter, 
			"because no data in server tool asynch read resp");
			reply->m_cid = ECA_GETFAIL;
		}
	}
	else if (completionStatus==S_cas_noRead &&
		CA_V41(CA_PROTOCOL_VERSION,this->minor_version_number)) {
		reply->m_cid = ECA_NORDACCESS;
	}
	else {
		errMessage(completionStatus, 
			"for call back asynch completion");
		reply->m_cid = ECA_GETFAIL;
	}

	//
	// If they return non-zero status or a nill gdd ptr
	//
	if (reply->m_cid != ECA_NORMAL) {
		//
		// If the operation failed clear the response data
		// area
		//
		memset ((char *)(reply+1), '\0', size);
	}

	//
	// force string message size to be the true size rounded to even
	// boundary
	//
	if (msg.m_type == DBR_STRING && msg.m_count == 1u) {
		/* add 1 so that the string terminator will be shipped */
		strcnt = strlen((char *)(reply + 1u)) + 1u;
		reply->m_postsize = strcnt;
	}

	this->commitMsg ();

	return S_cas_success;
}


//
// casStrmClient::monitorResponse()
//
caStatus casStrmClient::monitorResponse (casChannelI *pChan, 
		const caHdr &msg, gdd *pDesc, const caStatus completionStatus)
{
	caStatus	completionStatusCopy = completionStatus;
	caHdr 		*pReply;
	unsigned	size;
	caStatus	status;
	int        	strcnt;
	gdd		*pDBRDD;
	gddStatus       gdds;

	//
	// verify read access
	//
        if (!(*pChan)->readAccess()) {
		completionStatusCopy = S_cas_noRead;
        }

	size = dbr_size_n (msg.m_type, msg.m_count);
	status = this->allocMsg(size, &pReply);
	if (status) {
		if (status==S_cas_hugeRequest) {
			//
			// If we cant includ the data it is a proto
			// violation - so we generate an exception
			// instead
			//
			status = sendErr(&msg, ECA_TOLARGE, 
					"unable to xmit enevt info");
		}
		return status;
	}

	//
	// setup response message
	//
	*pReply = msg; 
	pReply->m_postsize = size;

	//
	// cid field abused to store the status here
	//
	if (completionStatusCopy == S_cas_success) {
		if (pDesc) {

			status = createDBRDD(msg.m_type, msg.m_count, pDBRDD);
			if (status != S_cas_success) {
				pDBRDD = NULL;
				completionStatusCopy = status;
			}
			else {
				gdds = gddApplicationTypeTable::
					app_table.smartCopy(pDBRDD, pDesc);
				if (gdds) {
					errPrintf (status, __FILE__, __LINE__,
"no conversion between event app type=%d and DBR type=%d Element count=%d",
						pDesc->applicationType(),
						msg.m_type,
						msg.m_count);
					completionStatusCopy = S_cas_noConvert;
				}
			}
		}
		else {
			completionStatusCopy = S_cas_badParameter;
		}
	}

	//	
	// see no DD and no convert case above
	//
	if (completionStatusCopy == S_cas_success) {
		pReply->m_cid = ECA_NORMAL;

		//
		// there appears to be no success/fail
		// status from this routine
		//
		gddMapDbr[msg.m_type].conv_dbr ((pReply+1), pDBRDD);

		//
		// force string message size to be the true size 
		//
		if (msg.m_type == DBR_STRING && msg.m_count == 1u) {
			// add 1 so that the string terminator 
			// will be shipped 
			strcnt = strlen((char *)(pReply + 1u)) + 1u;
			pReply->m_postsize = strcnt;
		}
	}
	else {
		errMessage(completionStatusCopy, "monitor response");
		if (completionStatusCopy== S_cas_noRead) {
			pReply->m_cid = ECA_NORDACCESS;
		}
		else {
			pReply->m_cid = ECA_GETFAIL;
		}

		//
		// If the operation failed clear the response data
		// area
		//
		memset ((char *)(pReply+1u), '\0', size);
	}

	this->commitMsg ();

	if (pDBRDD) {
		pDBRDD->unreference();
	}

	return S_cas_success;
}



/*
 * casStrmClient::writeAction()
 */
caStatus casStrmClient::writeAction()
{	
	const caHdr 	*mp = this->ctx.getMsg();
	caStatus	status;
	casChannelI	*pChan;

	status = this->verifyRequest (pChan);
	if (status != S_cas_validRequest) {
		return status;
	}

	//
	// verify write access
	// 
	if ((*pChan)->writeAccess()!=aitTrue) {
		int	v41;

		v41 = CA_V41(CA_PROTOCOL_VERSION,this->minor_version_number);
		if (v41) {
			status = ECA_NOWTACCESS;
		}
		else{
			status = ECA_PUTFAIL;
		}

		return this->sendErr(mp, status, "write access denied");
	}

	//
	// initiate the  write operation
	//
	status = this->write(); 
	if (status==S_casApp_success) {
		status = S_cas_success;
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else {
		status = this->sendErrWithEpicsStatus(mp, status, ECA_PUTFAIL);
		//
		// I have assumed that the server tool has deleted the gdd here
		//
	}

	//
	// The gdd created above is deleted by the server tool 
	//

	return status;

}


//
// casStrmClient::writeResponse()
//
caStatus casStrmClient::writeResponse (casChannelI *, 
		const caHdr &msg, gdd *pDesc, const caStatus completionStatus)
{
	caStatus status;

	if (pDesc) {
		errMessage(S_cas_badParameter, 
			"didnt expect a data descriptor from asynch write");
	}

	if (completionStatus) {
		errMessage(completionStatus, NULL);
		status = this->sendErrWithEpicsStatus(&msg, 
				completionStatus, ECA_PUTFAIL);
	}
	else {
		status = S_cas_success;
	}

	return status;
}


/*
 * casStrmClient::writeNotifyAction()
 */
caStatus casStrmClient::writeNotifyAction()
{	
	const caHdr 	*mp = this->ctx.getMsg();
	int		status;
	casChannelI	*pChan;

	status = this->verifyRequest (pChan);
	if (status != S_cas_validRequest) {
		return status;
	}

	//
	// verify write access
	// 
	if ((*pChan)->writeAccess()!=aitTrue) {
		return casStrmClient::writeNotifyResponse(pChan, *mp,
				NULL, S_cas_noWrite);
	}

	//
	// initiate the  write operation
	//
	status = this->write(); 
	if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else {
		status = casStrmClient::writeNotifyResponse(pChan, *mp,
				NULL, status);
	}

	return status;
}


/* 
 * casStrmClient::writeNotifyResponse()
 */
caStatus casStrmClient::writeNotifyResponse(casChannelI *, 
		const caHdr &msg, gdd *pDesc, const caStatus completionStatus)
{
	caHdr		*preply;
	caStatus	opStatus;

	if (pDesc) {
		errMessage(S_cas_badParameter, 
			"didnt expect a data descriptor from asynch write");
	}

	opStatus = this->allocMsg(0u, &preply);
	if (opStatus) {
		return opStatus;
	}

	*preply = msg;
	preply->m_postsize = 0u;
	if (completionStatus==S_cas_success) {
		preply->m_cid = ECA_NORMAL;
	}
	else if (completionStatus==S_cas_noWrite &&
		CA_V41(CA_PROTOCOL_VERSION,this->minor_version_number)) {
		preply->m_cid = ECA_NOWTACCESS;
	}
	else {
		errMessage(completionStatus, NULL);
		preply->m_cid = ECA_PUTFAIL;	
	}

	/* commit the message */
	this->commitMsg();

	return S_cas_success;
}


/*
 * casStrmClient::hostNameAction()
 */
caStatus casStrmClient::hostNameAction()
{
	const caHdr 		*mp = this->ctx.getMsg();
	char 			*pName = (char *) this->ctx.getData();
	casChannelI		*pciu;
	tsDLIter<casChannelI>	iter(this->chanList);
	unsigned		size;
	char 			*pMalloc;

	size = strlen(pName)+1u;
	/*
	 * user name will not change if there isnt enough memory
	 */
	pMalloc = new char [size];
	if(!pMalloc){
		this->sendErr(mp, ECA_ALLOCMEM, pName);
		return S_cas_internal;
	}
	strncpy(
		pMalloc, 
		pName, 
		size-1);
	pMalloc[size-1]='\0';

	this->lock();

	if (this->pHostName) {
		delete [] this->pHostName;
	}
	this->pHostName = pMalloc;

	while ( (pciu = iter()) ) {
		(*pciu)->setOwner(this->pUserName, this->pHostName);
	}

	this->unlock();

	return S_cas_success;
}


/*
 * casStrmClient::clientNameAction()
 */
caStatus casStrmClient::clientNameAction()
{
	const caHdr 		*mp = this->ctx.getMsg();
	char 			*pName = (char *) this->ctx.getData();
	casChannelI		*pciu;
	tsDLIter<casChannelI>	iter(this->chanList);
	unsigned		size;
	char 			*pMalloc;

	size = strlen(pName)+1;

	/*
	 * user name will not change if there isnt enough memory
	 */
	pMalloc = new char [size];
	if(!pMalloc){
		this->sendErr(mp, ECA_ALLOCMEM, pName);
		return S_cas_internal;
	}
	strncpy(
		pMalloc, 
		pName, 
		size-1);
	pMalloc[size-1]='\0';

	this->lock();
	if (this->pUserName) {
		delete [] this->pUserName;
	}
	this->pUserName = pMalloc;

	while ( (pciu = iter()) ) {
		(*pciu)->setOwner(this->pUserName, this->pHostName);
	}
	this->unlock();

	return S_cas_success;
}



/*
 * casStrmClientMon::claimChannelAction()
 */
caStatus casStrmClient::claimChannelAction()
{
	const caHdr 	*mp = this->ctx.getMsg();
	char		*pName = (char *) this->ctx.getData();
	int		status;
	unsigned	nameLength;

	/*
	 * The available field is used (abused)
	 * here to communicate the miner version number
	 * starting with CA 4.1. The field was set to zero
	 * prior to 4.1
	 */
	this->minor_version_number = mp->m_available;

	//
	// We shouldnt be receiving a connect message from 
	// an R3.11 client because we will not respond to their
	// search requests (if so we disconnect)
	//
        if (!CA_V44(CA_PROTOCOL_VERSION,this->minor_version_number)) {
                //
                // old connect protocol was dropped when the
                // new API was added to the server (they must
                // now use clients at EPICS 3.12 or higher)
                //
                status = this->sendErr(mp, ECA_DEFUNCT,
                        "R3.11 connect sequence from old client was ignored");
                return S_cas_badProtocol; // disconnect client
	}


	if (mp->m_postsize == 0u) {
		return S_cas_badProtocol; // disconnect client
	}

	nameLength = strlen(pName);
	if (nameLength>mp->m_postsize) {
		return S_cas_badProtocol; // disconnect client
	}

	if (nameLength>unreasonablePVNameSize) {
		return S_cas_badProtocol; // disconnect client
	}

	status = this->createChannel (pName);
	if (status == S_casApp_asyncCompletion) {
		//
		// asynchronous completion 
		//
		status = S_cas_success;
	}
	return status;
}


/*
 * casStrmClient::channelCreateFailed()
 *
 * If we are talking to an CA_V46 client then tell them when a channel
 * cant be created (instead of just disconnecting)
 */
caStatus casStrmClient::channelCreateFailed(
const caHdr     *mp,
caStatus        createStatus)
{
        caStatus        status;
        caHdr   	*reply;
 
	errMessage (createStatus, "- Server unable to create a new PV");
        if (CA_V46(CA_PROTOCOL_VERSION,this->minor_version_number)) {
 
                status = allocMsg (0u, &reply);
                if (status) {
                        return status;
                }
                *reply = nill_msg;
                reply->m_cmmd = CA_PROTO_CLAIM_CIU_FAILED;
                reply->m_cid = mp->m_cid;
                this->commitMsg();
                createStatus = S_cas_success;
        }
        else {
		this->sendErrWithEpicsStatus(mp, createStatus, ECA_ALLOCMEM);
        }
 
        return createStatus;
}


/*
 * casStrmClient::disconnectChan()
 *
 * If we are talking to an CA_V47 client then tell them when a channel
 * was deleted by the server tool 
 */
caStatus casStrmClient::disconnectChan(caResId id)
{
        caStatus        status;
	caStatus	createStatus;
        caHdr   	*reply;
 
        if (CA_V47(CA_PROTOCOL_VERSION,this->minor_version_number)) {
 
                status = allocMsg (0u, &reply);
                if (status) {
                        return status;
                }
                *reply = nill_msg;
                reply->m_cmmd = CA_PROTO_SERVER_DISCONN;
                reply->m_cid = id;
                this->commitMsg();
                createStatus = S_cas_success;
        }
        else {
		ca_printf(
"Disconnecting old client when server tool deleted its channel or PV\n");
		createStatus = S_cas_disconnect;
        }
 
        return createStatus;
}



//
// casStrmClient::eventsOnAction()
//
caStatus casStrmClient::eventsOnAction ()
{
	casChannelI		*pciu;
	tsDLIter<casChannelI>	iter(this->chanList);

	this->setEventsOff();

	//
	// perhaps this is to slow - perhaps there
	// should be a queue of modified events
	//
	this->lock();
	while ( (pciu = iter()) ) {
		pciu->postAllModifiedEvents();
	}
	this->unlock();

	return S_cas_success;
}

//
// casStrmClient::eventsOffAction()
//
caStatus casStrmClient::eventsOffAction()
{
	this->setEventsOn();
	return S_cas_success;
}



//
// eventAddAction()
//
caStatus casStrmClient::eventAddAction ()
{
	const caHdr 		*mp = this->ctx.getMsg();
	struct mon_info		*pMonInfo = (struct mon_info *) 
					this->ctx.getData();
	casClientMon		*pMonitor;
	casChannelI		*pciu;
	gdd			*pDD;
	caStatus		status;
	casEventMask		mask;

	pciu = this->resIdToChannel(mp->m_cid);
	if (!pciu) {
		logBadId(mp, (void *) pMonInfo);
		return S_cas_internal;
	}

	if (mp->m_count==0u) {
		this->sendErr(mp, ECA_BADCOUNT, "event add request");
		return S_cas_success;
	}

	if (pMonInfo->m_mask&DBE_VALUE) {
		mask |= this->getCAS().getAdapter()->valueEventMask;
	}

	if (pMonInfo->m_mask&DBE_LOG) {
		mask |= this->getCAS().getAdapter()->logEventMask;
	}
	
	if (pMonInfo->m_mask&DBE_ALARM) {
		mask |= this->getCAS().getAdapter()->alarmEventMask;
	}

	if (mask.noEventsSelected()) {
		this->sendErr(mp, ECA_BADMASK, "event add request");
		return S_cas_success;
	}

	pMonitor = new casClientMon(*pciu, mp->m_available, 
				mp->m_count, mp->m_type, mask, *this);
	if (!pMonitor) {
		this->sendErr(mp, ECA_ALLOCMEM, NULL);
		return S_cas_internal;
	}

	//
	// always send immediate monitor response at event add
	//
	pDD = NULL;
	status = this->read(pDD); 
	if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else {
		status = this->monitorResponse (pciu, *mp, pDD, status);
	}

	if (pDD) {
		int gddStatus;
		gddStatus = pDD->unreference();
		assert(gddStatus==0);
	}

	return status;
}



//
// casStrmClient::clearChannelAction()
//
caStatus casStrmClient::clearChannelAction ()
{
	const caHdr 	*mp = this->ctx.getMsg();
	void		*dp = this->ctx.getData();
        caHdr		*reply;
        casChannelI	*pciu;
        int        	status;

        /*
         * Verify the channel
         */
	pciu = this->resIdToChannel (mp->m_cid);
	if (!pciu) {
		logBadId (mp, dp);
		return S_cas_internal;
	}
	if (&pciu->getClient()!=this) {
		logBadId (mp, dp);
		return S_cas_internal;
	}

	/*
	 * send delete confirmed message
	 */
	status = this->allocMsg (0u, &reply);
	if (status) {
		return status;
	}
	*reply = *mp;
	this->commitMsg ();

	pciu->clientDestroy ();

	return S_cas_success;
}




//
// casStrmClient::eventCancelAction()
//
caStatus casStrmClient::eventCancelAction()
{
	const caHdr 	*mp = this->ctx.getMsg();
	void		*dp = this->ctx.getData();
        casChannelI	*pciu;
	caHdr		*reply;
	casMonitor	*pMon;
	int		status;

        /*
         * Verify the channel
         */
	pciu = this->resIdToChannel(mp->m_cid);
	if (!pciu) {
		logBadId(mp, dp);
		return S_cas_internal;
	}
	if (&pciu->getClient()!=this) {
		logBadId(mp, dp);
		return S_cas_internal;
	}
	pMon = pciu->findMonitor(mp->m_available);
	if (!pMon) {
		logBadId(mp, dp);
		return S_cas_internal;
	}

	/*
	 * allocate delete confirmed message
	 */
	status = allocMsg(0u, &reply);
	if (status) {
		return status;
	}

	reply->m_cmmd = CA_PROTO_EVENT_ADD;
	reply->m_postsize = 0u;
	reply->m_type = pMon->getType();
	reply->m_count = pMon->getCount();
	reply->m_cid = pciu->getCID();
	reply->m_available = pMon->getClientId();

	this->commitMsg();

	delete pMon;

	return S_cas_success;
}


#if 0
/*
 * casStrmClient::noReadAccessEvent()
 *
 * substantial complication introduced here by the need for backwards
 * compatibility
 */
caStatus casStrmClient::noReadAccessEvent(casClientMon *pMon)
{
	caHdr 		falseReply;
	unsigned	size;
	caHdr 		*reply;
	int		status;

	size = dbr_size_n (pMon->getType(), pMon->getCount());

	falseReply.m_cmmd = CA_PROTO_EVENT_ADD;
	falseReply.m_postsize = size;
	falseReply.m_type = pMon->getType();
	falseReply.m_count = pMon->getCount();
	falseReply.m_cid = pMon->getChannel().getCID();
	falseReply.m_available = pMon->getClientId();

	status = this->allocMsg(size, &reply);
	if (status) {
		if(status == S_cas_hugeRequest){
			status = this->sendErr(&falseReply, ECA_TOLARGE, NULL);
			return status;
		}
		return status;
	}
	else{
		/*
		 * New clients recv the status of the
		 * operation directly to the
		 * event/put/get callback.
		 *
		 * Fetched value is zerod in case they
		 * use it even when the status indicates 
		 * failure.
		 *
		 * The m_cid field in the protocol
		 * header is abused to carry the status
		 */
		*reply = falseReply;
		reply->m_postsize = size;
		reply->m_cid = ECA_NORDACCESS;
		memset((char *)(reply+1), 0, size);
		this->commitMsg();
	}
	
	return S_cas_success;
}
#endif


/*
 *	casStrmClient::readSyncAction()
 */
caStatus casStrmClient::readSyncAction()
{
	const caHdr 	*mp = this->ctx.getMsg();
	int		status;
	caHdr 		*reply;

	status = this->allocMsg(0u, &reply);
	if(status){
		return status;
	}

	*reply = *mp;

	this->commitMsg();

	return S_cas_success;
}



/*
 * caStatus casStrmClient::accessRightsResponse()
 */
caStatus casStrmClient::accessRightsResponse(casChannelI *pciu)
{
        caHdr 		*reply;
	unsigned	ar;
	int		v41;
	int		status;

	/*
	 * noop if this is an old client
	 */
	v41 = CA_V41(CA_PROTOCOL_VERSION, this->minor_version_number);
	if(!v41){
		return S_cas_success;
	}

	ar = 0; /* none */
	if ((*pciu)->readAccess()) {
		ar |= CA_PROTO_ACCESS_RIGHT_READ;
	}
	if ((*pciu)->writeAccess()) {
		ar |= CA_PROTO_ACCESS_RIGHT_WRITE;
	}

	status = this->allocMsg(0u, &reply);
	if(status){
		return status;
	}

	*reply = nill_msg;
        reply->m_cmmd = CA_PROTO_ACCESS_RIGHTS;
	reply->m_cid = pciu->getCID();
	reply->m_available = ar;
        this->commitMsg();

	return S_cas_success;
}


//
// casStrmClient::write()
//
caStatus casStrmClient::write()
{
	const caHdr	*pHdr = this->ctx.getMsg();
	casPVI		*pPV = this->ctx.getPV();
	caStatus	status;

	//
	// no puts via compound types (for now)
	//
	if (dbr_value_offset[pHdr->m_type]) {
		return S_cas_badType;
	}

	//
	// the PV state must not be modified during a transaction
	//
	status = (*pPV)->beginTransaction();
	if (status) {
		return status;
	}

	//
	// DBR_STRING is stored outside the DD so it
	// lumped in with arrays
	//
	if (pHdr->m_count > 1u || pHdr->m_type==DBR_STRING) {
		status = this->writeArrayData();
	}
	else {
		status = this->writeScalerData();
	}

	(*pPV)->endTransaction();

	return status;
}


//
// casStrmClient::writeScalerData()
//
caStatus casStrmClient::writeScalerData()
{
	gdd *pDD;
	const caHdr *pHdr = this->ctx.getMsg();
	gddStatus gddStat;
	caStatus status;
	aitEnum	type;

	type = gddDbrToAit[pHdr->m_type].type;
	if (type == aitEnumInvalid) {
		return S_cas_badType;
	}

	pDD = new gddScaler (gddAppType_value, type);
	if (!pDD) {
		return S_cas_noMemory;
	}

	gddStat = pDD->genCopy(type, this->ctx.getData());
	if (gddStat) {
		pDD->unreference();
		return S_cas_badType;
	}

	//
	// No suprises when multiple codes are looking
	// at the same data
	//
	pDD->markConstant ();

	//
	// call the server tool's virtual function
	//
	status = (*this->ctx.getPV())->write(this->ctx, *pDD);

	//
	// tell the DD that this code is finished with it
	//
	gddStat = pDD->unreference();
	assert(gddStat==0);

	return status;
}


//
// casStrmClient::writeArrayData()
//
caStatus casStrmClient::writeArrayData()
{
	gdd *pDD;
	const caHdr *pHdr = this->ctx.getMsg();
	gddDestructor *pDestructor;
	gddStatus gddStat;
	caStatus status;
	aitEnum	type;
	char *pData;
	size_t size;

	type = gddDbrToAit[pHdr->m_type].type;
	if (type == aitEnumInvalid) {
		return S_cas_badType;
	}

	pDD = new gddAtomic(gddAppType_value, type, 1, pHdr->m_count);
	if (!pDD) {
		return S_cas_noMemory;
	}

	size = dbr_size_n (pHdr->m_type, pHdr->m_count);
	pData = new char [size];
	if (!pData) {
		pDD->unreference();
		return S_cas_noMemory;
	}

	pDestructor = new gddDestructor;
	if (!pDestructor) {
		delete [] pData;
		pDD->unreference();
		return S_cas_noMemory;
	}

	//
	// move the data from the protocol buffer
	// to the allocated area so that they
	// will be allowed to ref the DD
	//
	memcpy (pData, this->ctx.getData(), size);

	//
	// install allocated area into the DD
	//
	pDD->putRef (pData, type, pDestructor);

	//
	// No suprises when multiple codes are looking
	// at the same data
	//
	pDD->markConstant ();

	//
	// call the server tool's virtual function
	//
	status = (*this->ctx.getPV())->write(this->ctx, *pDD);

	//
	// tell the DD that this code is finished with it
	//
	gddStat = pDD->unreference();
	assert(gddStat==0);

	return status;
}


//
// casStrmClient::read()
//
caStatus casStrmClient::read(gdd *&pDescRet)
{
	const caHdr	*pHdr = this->ctx.getMsg();
	caStatus	status;

	pDescRet = NULL;
	status = createDBRDD (pHdr->m_type, pHdr->m_count, pDescRet);
	if (status) {
		return status;
	}

	assert(pDescRet);

	//
	// the PV state must not be modified during a transaction
	//
	status = (*this->ctx.getPV())->beginTransaction();
	if (status) {
		return status;
	}

	//
	// call the server tool's virtual function
	//
	status = (*this->ctx.getPV())->read(this->ctx, *pDescRet);
	if (status) {
		pDescRet->unreference();
		pDescRet = NULL;
		if (status!=S_casApp_asyncCompletion) {
			errMessage(status, 
				" - response from server tool\n");
		}
	}

	(*this->ctx.getPV())->endTransaction();

	return status;
}

//
// createDBRDD ()
//
caStatus createDBRDD (unsigned dbrType, aitIndex dbrCount, gdd *&pDescRet)
{
	aitUint32	valIndex;
	aitUint32	gddStatus;
	aitUint16	appType;

	appType = gddDbrToAit[dbrType].app;

	//
	// create the descriptor
	//
	pDescRet = gddApplicationTypeTable::app_table.getDD (appType);
	if (!pDescRet) {
		return S_cas_noMemory;
	}
	
	//
	// set bounds for the value member
	//
	if (dbrCount != 1u) {
		if (pDescRet->isContainer()) {
			gddContainer *pCont = (gddContainer *) pDescRet;
			gdd *pVal;

			gddStatus = 
			gddApplicationTypeTable::app_table.mapAppToIndex
				(appType, gddAppType_value, valIndex);
			if (gddStatus) {
				pDescRet->unreference();
				pDescRet = NULL;
				return S_cas_badType;
			}
			pVal = pCont->getDD(valIndex);
			assert (pVal);
			gddStatus = pVal->setBound (0, 0u, dbrCount);
			assert (gddStatus==0)
		}
		else if (pDescRet->isAtomic()) {
			gddAtomic *pAtomic = (gddAtomic *) pDescRet;
			gddStatus = pAtomic->setBound(0, 0u, dbrCount);
			assert (gddStatus==0)
		}
		else {
			assert(dbrCount==1u);
		}
	}

	return S_cas_success;
}


//
// casStrmClient::userName() 
//
const char *casStrmClient::userName() const
{
	return this->pUserName?this->pUserName:"?";
}

//
// casStrmClient::hostName() 
//
const char *casStrmClient::hostName() const
{
	return this->pHostName?this->pHostName:"?";
}


//
// casStrmClient::createChanResponse()
//
caStatus casStrmClient::createChanResponse(casChannelI *, 
		const caHdr &msg, gdd *pCanonicalName, 
		const caStatus completionStatus)
{
	casChannel 	*pChan;
	casChannelI 	*pChanI;
	casPVI		*pPV;
	caHdr 		*claim_reply; 
	unsigned	dbrType;
	caStatus	status;

	this->ctx.getServer()->pvExistTestCompletion();

	if (completionStatus) {
		return this->channelCreateFailed(&msg, completionStatus);
	}

	if (!pCanonicalName) {
		return this->channelCreateFailed(&msg, S_cas_badParameter);
	}

	status = allocMsg (0u, &claim_reply);
	if (status) {
		return status;
	}

	//
	// prevent problems such as the PV being deleted before the
	// channel references it
	//
	this->lock();

	pCanonicalName->markConstant();

	pPV = this->ctx.getServer()->createPV(*pCanonicalName);
	if (!pPV) {
		this->unlock();
		return this->channelCreateFailed(&msg, S_cas_noMemory);
	}

	//
	// create server tool XXX derived from casChannel
	//
	this->ctx.setPV(pPV);
	pChan = (*pPV)->createChannel(this->ctx, 
			this->pUserName, this->pHostName);
	if (!pChan) {
		this->unlock();
		pPV->deleteSignal();
		return this->channelCreateFailed(&msg, S_cas_noMemory);
	}

	this->unlock();

	pChanI = (casChannelI *) pChan;

	status = pPV->bestDBRType(dbrType);
	if (status) {
		errMessage(status, "best external dbr type fetch failed");
		pChanI->clientDestroy();
		return this->channelCreateFailed(&msg, status);
	}

	*claim_reply = nill_msg;
	claim_reply->m_cmmd = CA_PROTO_CLAIM_CIU;
	claim_reply->m_type = dbrType;
	claim_reply->m_count = pPV->nativeCount();
	claim_reply->m_cid = msg.m_cid;
	claim_reply->m_available = pChanI->getSID();
	this->commitMsg();

	return status;
}

//
// caServerI::createPV ()
//
casPVI *caServerI::createPV (gdd &name)
{
	aitString	*pAITStr;
        casPVI          *pPVI;
        caStatus        status;

	//
	// dont proceed unless its a valid PV name
	//
	status = casPVI::verifyPVName (name);
	if (status) {
		return NULL;
	}

	name.getRef (pAITStr);
	stringId id (pAITStr->string());

        this->lock ();

        pPVI = this->stringResTbl.lookup (id);
	if (!pPVI) {
        	casPV	*pPV;
		pPV = (*this)->createPV (this->ctx, pAITStr->string());
		if (pPV) {
			pPVI = (casPVI *) pPV;
		}
        }

	//
	// lock shouldnt be released until we finish creating and
	// installing the PV
	//
        this->unlock ();

        return pPVI;
}

//
// caServerI::roomForNewChannel()
//
inline aitBool caServerI::roomForNewChannel() const
{
	return aitTrue;
}

//
// casStrmClient::installChannel()
//
void casStrmClient::installChannel(casChannelI &chan)
{
        this->lock();
        this->getCAS().installItem(chan);
        this->chanList.add(chan);
        this->unlock();
}
 
//
// casStrmClient::removeChannel()
//
void casStrmClient::removeChannel(casChannelI &chan)
{
        casRes *pRes;
 
        this->lock();
        pRes = this->getCAS().removeItem(chan);
        assert (&chan == (casChannelI *)pRes);
        this->chanList.remove(chan);
        this->unlock();
}

