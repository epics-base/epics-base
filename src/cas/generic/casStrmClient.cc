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
 * Revision 1.22  1998/05/05 16:32:17  jhill
 * cleaned up file format
 *
 * Revision 1.21  1998/04/15 00:04:05  jhill
 * cosmetic
 *
 * Revision 1.20  1998/04/14 23:51:10  jhill
 * improved diagnostic
 *
 * Revision 1.19  1998/02/18 22:46:25  jhill
 * improved message
 *
 * Revision 1.18  1997/08/05 00:47:13  jhill
 * fixed warnings
 *
 * Revision 1.17  1997/06/30 22:54:28  jhill
 * use %p with pointers
 *
 * Revision 1.16  1997/06/13 09:16:00  jhill
 * connect proto changes
 *
 * Revision 1.15  1997/04/10 19:34:18  jhill
 * API changes
 *
 * Revision 1.14  1996/12/12 18:56:27  jhill
 * doc
 *
 * Revision 1.13  1996/12/11 01:03:52  jhill
 * removed redundant bad client attach detect
 *
 * Revision 1.12  1996/12/06 22:36:22  jhill
 * use destroyInProgress flag now functional nativeCount()
 *
 * Revision 1.11  1996/11/02 00:54:24  jhill
 * many improvements
 *
 * Revision 1.10  1996/09/16 18:24:05  jhill
 * vxWorks port changes
 *
 * Revision 1.9  1996/09/04 20:25:53  jhill
 * use correct app type for exist test gdd, correct byte 
 * oder for mon mask, and efficient use of PV name gdd
 *
 * Revision 1.8  1996/08/05 23:22:57  jhill
 * gddScaler => gddScalar
 *
 * Revision 1.7  1996/08/05 19:26:51  jhill
 * doc
 *
 * Revision 1.5  1996/07/09 22:53:33  jhill
 * init stat and sev in gdd
 *
 * Revision 1.4  1996/07/01 19:56:14  jhill
 * one last update prior to first release
 *
 * Revision 1.3  1996/06/26 21:18:59  jhill
 * now matches gdd api revisions
 *
 * Revision 1.2  1996/06/21 02:30:57  jhill
 * solaris port
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#include "dbMapper.h"		// ait to dbr types
#include "gddAppTable.h"    // EPICS application type table
#include "gddApps.h"		// gdd predefined application type codes
#include "net_convert.h"	// byte order conversion from libca

#include "server.h"
#include "caServerIIL.h"	// caServerI inline functions
#include "casChannelIIL.h"	// casChannelI inline functions
#include "casPVIIL.h"		// casPVI inline functions
#include "casCtxIL.h"		// casCtx inline functions
#include "casEventSysIL.h"	// casEventSys inline functions
#include "inBufIL.h"		// inBuf inline functions
#include "outBufIL.h"		// outBuf inline functions

static const caHdr nill_msg = {0u,0u,0u,0u,0u,0u};

//
// casStrmClient::verifyRequest()
//
caStatus casStrmClient::verifyRequest (casChannelI *&pChan)
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
	if (mp->m_count > pChan->getPVI().nativeCount() || mp->m_count==0u) {
		return this->sendErr(mp, ECA_BADCOUNT, NULL);
	}

	return S_cas_validRequest;
}




//
// casStrmClient::casStrmClient()
//
casStrmClient::casStrmClient(caServerI &serverInternal) :
	inBuf(*(osiMutex *)this, casStreamIO::optimumBufferSize()),
	outBuf(*(osiMutex *)this, casStreamIO::optimumBufferSize()),
	casClient(serverInternal, *this, *this),
	pUserName(NULL), pHostName(NULL)
{
	this->ctx.getServer()->installClient(this);
}

//
// casStrmClient::init()
//
caStatus casStrmClient::init()
{
	caStatus status;

	//
	// call base class initializers
	//
	status = casClient::init();
	if (status) {
		return status;
	}
    status = this->inBuf::init();
    if (status) {
            return status;
    }
    status = this->outBuf::init();
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
	this->osiLock();

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

	//
	// delete all channel attached
	//
	tsDLIterBD<casChannelI> iter(this->chanList.first());
	const tsDLIterBD<casChannelI> eol;
	tsDLIterBD<casChannelI> tmp;
	while (iter!=eol) {
		//
		// destroying the channel removes it from the list
		//
		tmp = iter;
		++tmp;
		iter->clientDestroy();
		iter = tmp;
	}

	this->osiUnlock();
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
void casStrmClient::show (unsigned level) const
{
	this->casClient::show (level);
	printf ("casStrmClient at %p\n", this);
	if (level > 1u) {
		printf ("\tuser %s at %s\n", this->pUserName, this->pHostName);
	}
	this->inBuf::show(level);
	this->outBuf::show(level);
}


/*
 * casStrmClient::readAction()
 */
caStatus casStrmClient::readAction ()
{
	const caHdr *mp = this->ctx.getMsg();
	caStatus status;
	casChannelI *pChan;
	smartGDDPointer pDesc;

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

	status = this->read(pDesc); 
	if (status==S_casApp_success) {
		status = this->readResponse(pChan, *mp, pDesc, S_cas_success);
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status == S_casApp_postponeAsyncIO) {
		pChan->getPVI().addItemToIOBLockedList(*this);
	}
	else {
		status = this->sendErrWithEpicsStatus(mp, status, ECA_GETFAIL);
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
	caStatus	localStatus;
	int			mapDBRStatus;
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
	mapDBRStatus = gddMapDbr[msg.m_type].conv_dbr((reply+1), msg.m_count, pDesc);
	if (mapDBRStatus<0) {
		pDesc->dump();
		errPrintf (S_cas_badBounds, __FILE__, __LINE__, "- get notify with PV=%s type=%u count=%u",
				pChan->getPVI()->getName(), msg.m_type, msg.m_count);
		return this->sendErrWithEpicsStatus(&msg, S_cas_badBounds, ECA_GETFAIL);
	}
#ifdef CONVERSION_REQUIRED
	/* use type as index into conversion jumptable */
	(* cac_dbr_cvrt[msg.m_type])
		( reply + 1,
		  reply + 1,
		  TRUE,       /* host -> net format */
		  msg.m_count);
#endif
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
	const caHdr *mp = this->ctx.getMsg();
	int status;
	casChannelI *pChan;
	smartGDDPointer pDesc;

	status = this->verifyRequest (pChan);
	if (status != S_cas_validRequest) {
		return casStrmClient::readNotifyResponse(NULL, *mp, NULL,
				S_cas_badRequest);
	}

	//
	// verify read access
	// 
	if ((*pChan)->readAccess()!=aitTrue) {
		return this->readNotifyResponse(pChan, *mp, NULL, S_cas_noRead);
	}

	status = this->read(pDesc); 
	if (status == S_casApp_success) {
		status = this->readNotifyResponse(pChan, *mp, pDesc, status);
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status == S_casApp_postponeAsyncIO) {
		pChan->getPVI().addItemToIOBLockedList(*this);
	}
	else {
		status = this->readNotifyResponse(pChan, *mp, pDesc, status);
	}

	return status;
}



//
// casStrmClient::readNotifyResponse()
//
caStatus casStrmClient::readNotifyResponse (casChannelI *pChan, 
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
			int mapDBRStatus;
			//
			// convert gdd to db_access type
			// (places the data in network format)
			//
			mapDBRStatus = gddMapDbr[msg.m_type].conv_dbr((reply+1), msg.m_count, pDesc);
			if (mapDBRStatus<0) {
				pDesc->dump();
				errPrintf (S_cas_badBounds, __FILE__, __LINE__, "- get notify with PV=%s type=%u count=%u",
					pChan->getPVI()->getName(), msg.m_type, msg.m_count);
				reply->m_cid = ECA_GETFAIL;
			}
			else {
				reply->m_cid = ECA_NORMAL;
			}
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
			"<= get callback failure detail not passed to client");
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

#ifdef CONVERSION_REQUIRED
	/* use type as index into conversion jumptable */
	(* cac_dbr_cvrt[msg.m_type])
		( reply + 1,
		  reply + 1,
		  TRUE,       /* host -> net format */
		  msg.m_count);
#endif
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
	caStatus completionStatusCopy = completionStatus;
	smartGDDPointer pDBRDD;
	caHdr *pReply;
	unsigned size;
	caStatus status;
	int strcnt;
	gddStatus gdds;

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
					"unable to xmit event");
		}
		return status;
	}

	//
	// setup response message
	//
	*pReply = msg; 
	pReply->m_postsize = size;

	//
	// verify read access
	//
	if (!(*pChan)->readAccess()) {
		completionStatusCopy = S_cas_noRead;
	}

	//
	// cid field abused to store the status here
	//
	if (completionStatusCopy == S_cas_success) {
		if (pDesc) {

			completionStatusCopy = createDBRDD(msg.m_type, 
							msg.m_count, pDBRDD);
			if (completionStatusCopy==S_cas_success) {
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
		gddMapDbr[msg.m_type].conv_dbr ((pReply+1), msg.m_count, pDBRDD);

#ifdef CONVERSION_REQUIRED
		/* use type as index into conversion jumptable */
		(* cac_dbr_cvrt[msg.m_type])
			( pReply + 1,
			  pReply + 1,
			  TRUE,       /* host -> net format */
			  msg.m_count);
#endif
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
		errMessage(completionStatusCopy, "- in monitor response");

		if (completionStatusCopy== S_cas_noRead) {
			pReply->m_cid = ECA_NORDACCESS;
		}
		else if (completionStatusCopy==S_cas_noMemory) {
			pReply->m_cid = ECA_ALLOCMEM;
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
	if (status==S_casApp_success || status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status==S_casApp_postponeAsyncIO) {
		pChan->getPVI().addItemToIOBLockedList(*this);
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
		const caHdr &msg, const caStatus completionStatus)
{
	caStatus status;

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
		return casStrmClient::writeNotifyResponse(NULL, *mp,
				S_cas_badRequest);
	}

	//
	// verify write access
	// 
	if ((*pChan)->writeAccess()!=aitTrue) {
		return casStrmClient::writeNotifyResponse(pChan, *mp,
				S_cas_noWrite);
	}

	//
	// initiate the  write operation
	//
	status = this->write(); 
	if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status==S_casApp_postponeAsyncIO) {
		pChan->getPVI().addItemToIOBLockedList(*this);
	}
	else {
		status = casStrmClient::writeNotifyResponse(pChan, *mp,
				status);
	}

	return status;
}


/* 
 * casStrmClient::writeNotifyResponse()
 */
caStatus casStrmClient::writeNotifyResponse(casChannelI *, 
		const caHdr &msg, const caStatus completionStatus)
{
	caHdr		*preply;
	caStatus	opStatus;

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

	this->osiLock();

	if (this->pHostName) {
		delete [] this->pHostName;
	}
	this->pHostName = pMalloc;

	tsDLIterBD<casChannelI> iter(this->chanList.first());
	const tsDLIterBD<casChannelI> eol;
	while ( iter!=eol ) {
		(*iter)->setOwner(this->pUserName, this->pHostName);
		++iter;
	}

	this->osiUnlock();

	return S_cas_success;
}


/*
 * casStrmClient::clientNameAction()
 */
caStatus casStrmClient::clientNameAction()
{
	const caHdr 		*mp = this->ctx.getMsg();
	char 			*pName = (char *) this->ctx.getData();
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

	this->osiLock();

	if (this->pUserName) {
		delete [] this->pUserName;
	}
	this->pUserName = pMalloc;

	tsDLIterBD<casChannelI>	iter(this->chanList.first());
	const tsDLIterBD<casChannelI> eol;
	while ( iter!=eol ) {
		(*iter)->setOwner(this->pUserName, this->pHostName);
		++iter;
	}
	this->osiUnlock();

	return S_cas_success;
}



/*
 * casStrmClientMon::claimChannelAction()
 */
caStatus casStrmClient::claimChannelAction()
{
	const caHdr 	*mp = this->ctx.getMsg();
	char		*pName = (char *) this->ctx.getData();
	unsigned	nameLength;
	caStatus	status;

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
		this->sendErr(mp, ECA_DEFUNCT,
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

	//
	// prevent problems such as the PV being deleted before the
	// channel references it
	//
	this->osiLock();
	this->asyncIOFlag = 0u;
	const pvCreateReturn pvcr =  
			(*this->ctx.getServer())->createPV (this->ctx, pName);
	//
	// prevent problems when they initiate
	// async IO but dont return status
	// indicating so (and vise versa)
	//
	if (this->asyncIOFlag) {
		status = S_cas_success;	
	}
	else if (pvcr.getStatus() == S_casApp_asyncCompletion) {
		status = this->createChanResponse(*mp, 
			pvCreateReturn(S_cas_badParameter));
		errMessage(S_cas_badParameter, 
		"- expected asynch IO creation from caServer::createPV()");
	}
	else if (pvcr.getStatus() == S_casApp_postponeAsyncIO) {
		status = S_casApp_postponeAsyncIO;
		this->ctx.getServer()->addItemToIOBLockedList(*this);
	}
	else {
		status = this->createChanResponse(*mp, pvcr);
	}
	this->osiUnlock();
	return status;
}

//
// casStrmClient::createChanResponse()
//
// LOCK must be applied
//
caStatus casStrmClient::createChanResponse(const caHdr &hdr, const pvCreateReturn &pvcr)
{
	casPVI		*pPV;
	casChannel 	*pChan;
	casChannelI 	*pChanI;
	caHdr 		*claim_reply; 
	caHdr 		*dummy; 
	unsigned	dbrType;
	caStatus	status;

	if (pvcr.getStatus() != S_cas_success) {
		return this->channelCreateFailed(&hdr, pvcr.getStatus());
	}

	pPV = pvcr.getPV();

	//
	// If status is ok and the PV isnt set then guess that the
	// pv isnt in this server
	//
	if (pPV == NULL) {
		return this->channelCreateFailed(&hdr, S_casApp_pvNotFound);
	}

	//
	// attach the PV to this server
	//
	status = pPV->attachToServer (this->getCAS());
	if (status) {
		return this->channelCreateFailed (&hdr, status);
	}

	//
	// NOTE:
	// We are allocating enough space for both the claim
	// response and the access response so that we know for
	// certain that they will both be sent together.
	//
	status = this->allocMsg (sizeof(caHdr), &dummy);
	if (status) {
		return status;
	}

	//
	// create server tool XXX derived from casChannel
	//
	this->ctx.setPV(pPV);
	pChan = (*pPV)->createChannel(this->ctx, 
			this->pUserName, this->pHostName);
	if (!pChan) {
		pvcr.getPV()->deleteSignal();
		return this->channelCreateFailed(&hdr, S_cas_noMemory);
	}

	this->osiUnlock();

	pChanI = (casChannelI *) pChan;

	//
	// NOTE:
	// We are certain that the request will complete
	// here because we allocated enough space for this
	// and the claim response above.
	//
	status = casStrmClient::accessRightsResponse(pChanI);
	if (status) {
		errMessage(status, "incompplete channel create?");
		pChanI->clientDestroy();
		return this->channelCreateFailed(&hdr, status);
	}

	status = pPV->bestDBRType(dbrType);
	if (status) {
		errMessage(status, "best external dbr type fetch failed");
		pChanI->clientDestroy();
		return this->channelCreateFailed(&hdr, status);
	}

	//
	// NOTE:
	// We are allocated enough space for both the claim
	// response and the access response so that we know for
	// certain that they will both be sent together.
	// Nevertheles, some (old) clients do not receive
	// an access rights response so we allocate again
	// here to be certain that we are at the correct place in
	// the protocol buffer.
	//
	// The 3rd arg indicates - dont lock the buffer again
	//
	status = this->allocMsg (0u, &claim_reply, FALSE);
	//
	// Not sending the access rights response and the claim
	// response is a severe error which is avoided by
	// the first (oversize) allocMsg
	//
	assert (status==S_cas_success);

	*claim_reply = nill_msg;
	claim_reply->m_cmmd = CA_PROTO_CLAIM_CIU;
	claim_reply->m_type = dbrType;
	claim_reply->m_count = pPV->nativeCount();
	claim_reply->m_cid = hdr.m_cid;
	claim_reply->m_available = pChanI->getSID();

	//
	// Unlock the buffer (and convert it to network format
	//
	this->commitMsg();

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
 
	if (createStatus == S_casApp_asyncCompletion) {
		errMessage(S_cas_badParameter, 
	"- no asynchronous IO create in createPV() ?");
		errMessage(S_cas_badParameter, 
	"- or S_casApp_asyncCompletion was async IO competion code ?");
	}
	else {
		errMessage (createStatus, "- Server unable to create a new PV");
	}
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
	caStatus status;
	caStatus createStatus;
	caHdr *reply;

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
		"Disconnecting old client because of internal channel or PV delete\n");
		createStatus = S_cas_disconnect;
	}

	return createStatus;
}



//
// casStrmClient::eventsOnAction()
//
caStatus casStrmClient::eventsOnAction ()
{
	this->setEventsOn();

	//
	// perhaps this is to slow - perhaps there
	// should be a queue of modified events
	//
	this->osiLock();
	tsDLIterBD<casChannelI> iter(this->chanList.first());
	const tsDLIterBD<casChannelI> eol;
	while ( iter!=eol ) {
		iter->postAllModifiedEvents();
		++iter;
	}
	this->osiUnlock();

	return S_cas_success;
}

//
// casStrmClient::eventsOffAction()
//
caStatus casStrmClient::eventsOffAction()
{
	this->setEventsOff();
	return S_cas_success;
}



//
// eventAddAction()
//
caStatus casStrmClient::eventAddAction ()
{
	const caHdr *mp = this->ctx.getMsg();
	struct mon_info *pMonInfo = (struct mon_info *) 
					this->ctx.getData();
	casClientMon *pMonitor;
	casChannelI *pciu;
	smartGDDPointer pDD;
	caStatus status;
	casEventMask mask;
	unsigned short caProtoMask;

	status = casStrmClient::verifyRequest (pciu);
	if (status != S_cas_validRequest) {
		return status;
	}

	//
	// place monitor mask in correct byte order
	//
	caProtoMask = ntohs (pMonInfo->m_mask);
	if (caProtoMask&DBE_VALUE) {
		mask |= this->getCAS().getAdapter()->valueEventMask;
	}

	if (caProtoMask&DBE_LOG) {
		mask |= this->getCAS().getAdapter()->logEventMask;
	}
	
	if (caProtoMask&DBE_ALARM) {
		mask |= this->getCAS().getAdapter()->alarmEventMask;
	}

	if (mask.noEventsSelected()) {
		char errStr[40];
		sprintf(errStr, "event add req with mask=0X%X\n", caProtoMask);
		this->sendErr(mp, ECA_BADMASK, errStr);
		return S_cas_success;
	}

	//
	// Attempt to read the first monitored value prior to creating
	// the monitor object so that if the server tool chooses
	// to postpone asynchronous IO we can safely restart this
	// request later.
	//
	status = this->read(pDD); 
	//
	// always send immediate monitor response at event add
	//
	if (status == S_casApp_success) {
		status = this->monitorResponse (pciu, *mp, pDD, status);
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status == S_casApp_postponeAsyncIO) {
		//
		// try again later
		//
		pciu->getPVI().addItemToIOBLockedList(*this);
	}
	else if (status == S_casApp_noMemory) {
		//
		// If we cant send the first monitor value because
		// there isnt pool space for a gdd then delete 
		// (disconnect) the channel
		//
		(*pciu)->destroy();
		return S_cas_success;
	}
	else {
		status = this->monitorResponse (pciu, *mp, pDD, status);
	}

	if (status==S_cas_success) {

		pMonitor = new casClientMon(*pciu, mp->m_available, 
					mp->m_count, mp->m_type, mask, *this);
		if (!pMonitor) {
			this->sendErr(mp, ECA_ALLOCMEM, NULL);
			//
			// If we cant allocate space for a monitor then
			// delete (disconnect) the channel
			//
			(*pciu)->destroy();
			status = S_cas_success;
		}
	}

	return status;
}



//
// casStrmClient::clearChannelAction()
//
caStatus casStrmClient::clearChannelAction ()
{
	const caHdr *mp = this->ctx.getMsg();
	void *dp = this->ctx.getData();
	caHdr *reply;
	casChannelI *pciu;
	int status;

	/*
	 * Verify the channel
	 */
	pciu = this->resIdToChannel (mp->m_cid);
	if (pciu==NULL) {
		logBadId (mp, dp, ECA_BADCHID);
	}
	else {
		pciu->clientDestroy ();
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

	return S_cas_success;
}




//
// casStrmClient::eventCancelAction()
//
caStatus casStrmClient::eventCancelAction ()
{
	const caHdr 	*mp = this->ctx.getMsg ();
	void		*dp = this->ctx.getData ();
	casChannelI *pciu;
	caHdr		*reply;
	casMonitor	*pMon;
	int 	status;
	
	/*
	 * Verify the channel
	 *
	 * if the monitor delete arrives just after the server tool
	 * has deleted the PV then the client will deallocate the 
	 * monitor structure when it receives the PV disconnect message.
	 *
	 * otherwise the client or server ha become corrupted
	 */
	pciu = this->resIdToChannel (mp->m_cid);
	if (!pciu) {
		logBadId (mp, dp, ECA_BADCHID);
		return S_cas_success;
	}

	/*
	 * verify the event (monitor)
	 */
	pMon = pciu->findMonitor (mp->m_available);
	if (!pMon) {
		logBadId (mp, dp, ECA_BADMONID);
		return S_cas_success;
	}
	
	/*
	 * allocate delete confirmed message
	 */
	status = allocMsg (0u, &reply);
	if (status) {
		return status;
	}
	
	reply->m_cmmd = CA_PROTO_EVENT_ADD;
	reply->m_postsize = 0u;
	reply->m_type = pMon->getType ();
	reply->m_count = (unsigned short) pMon->getCount ();
	reply->m_cid = pciu->getCID ();
	reply->m_available = pMon->getClientId ();
	
	this->commitMsg ();
	
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


//
// casStrmClient::readSyncAction()
//
caStatus casStrmClient::readSyncAction()
{
	const caHdr 	*mp = this->ctx.getMsg();
	int		status;
	caHdr 		*reply;

	//
	// This messages indicates that the client
	// timed out on a read so we must clear out 
	// any pending asynchronous IO associated with 
	// a read.
	//
	this->osiLock();
	tsDLIterBD<casChannelI> iter(this->chanList.first());
	const tsDLIterBD<casChannelI> eol;
	while ( iter!=eol ) {
		iter->clearOutstandingReads();
		++iter;
	}
	this->osiUnlock();

	status = this->allocMsg(0u, &reply);
	if(status){
		return status;
	}

	*reply = *mp;

	this->commitMsg();

	return S_cas_success;
}



 //
 // casStrmClient::accessRightsResponse()
 //
 // NOTE:
 // Do not change the size of this response without making
 // parallel changes in createChanResp
 //
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

#ifdef CONVERSION_REQUIRED
	/* use type as index into conversion jumptable */
	(* cac_dbr_cvrt[pHdr->m_type])
		( this->ctx.getData(),
		  this->ctx.getData(),
		  FALSE,       /* net -> host format */
		  pHdr->m_count);
#endif

	//
	// the PV state must not be modified during a transaction
	//
	status = (*pPV)->beginTransaction();
	if (status) {
		return status;
	}

	//
	// clear async IO flag
	//
	this->asyncIOFlag = 0u;

	//
	// DBR_STRING is stored outside the DD so it
	// lumped in with arrays
	//
	if (pHdr->m_count > 1u) {
		status = this->writeArrayData();
	}
	else {
		status = this->writeScalarData();
	}

	//
	// prevent problems when they initiate
	// async IO but dont return status
	// indicating so (and vise versa)
	//
	if (this->asyncIOFlag) {
		if (status!=S_casApp_asyncCompletion) {
			fprintf(stderr, 
"Application returned %d from casPV::write() - expected S_casApp_asyncCompletion\n",
				status);
			status = S_casApp_asyncCompletion;
		}
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_badParameter;
		errMessage(status, 
		"- expected asynch IO creation from casPV::write()");
	}

	(*pPV)->endTransaction();

	return status;
}


//
// casStrmClient::writeScalarData()
//
caStatus casStrmClient::writeScalarData()
{
	smartGDDPointer pDD;
	const caHdr *pHdr = this->ctx.getMsg();
	gddStatus gddStat;
	caStatus status;
	aitEnum	type;

	type = gddDbrToAit[pHdr->m_type].type;
	if (type == aitEnumInvalid) {
		return S_cas_badType;
	}

	pDD = new gddScalar (gddAppType_value, type);
	if (pDD==NULL) {
		return S_cas_noMemory;
	}

	//
	// reference count is managed by smart pointer class
	// from here down
	//
	gddStat = pDD->unreference();
	assert (!gddStat);

	if (type==aitEnumFixedString) {
		aitFixedString *pFStr = 
			(aitFixedString *) this->ctx.getData();
		gddStat = pDD->put(*pFStr);
	}
	else {
		gddStat = pDD->genCopy(type, this->ctx.getData());
	}
	if (gddStat) {
		return S_cas_badType;
	}

	pDD->setStat(epicsAlarmNone);
	pDD->setSevr(epicsSevNone);

	//
	// No suprises when multiple codes are looking
	// at the same data
	//
	pDD->markConstant ();

	//
	// call the server tool's virtual function
	//
	status = (*this->ctx.getPV())->write(this->ctx, *pDD);

	return status;
}


//
// casStrmClient::writeArrayData()
//
caStatus casStrmClient::writeArrayData()
{
	smartGDDPointer pDD;
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

	//
	// GDD ref count is managed by smart pointer class from here down
	//
	gddStat = pDD->unreference();
	assert (!gddStat);

	size = dbr_size_n (pHdr->m_type, pHdr->m_count);
	pData = new char [size];
	if (!pData) {
		return S_cas_noMemory;
	}

	pDestructor = new gddDestructor;
	if (!pDestructor) {
		delete [] pData;
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

	pDD->setStat(epicsAlarmNone);
	pDD->setSevr(epicsSevNone);

	//
	// No suprises when multiple codes are looking
	// at the same data
	//
	pDD->markConstant ();

	//
	// call the server tool's virtual function
	//
	status = (*this->ctx.getPV())->write(this->ctx, *pDD);

	return status;
}


//
// casStrmClient::read()
//
caStatus casStrmClient::read(smartGDDPointer &pDescRet)
{
	const caHdr	*pHdr = this->ctx.getMsg();
	caStatus	status;

	pDescRet = NULL;
	status = createDBRDD (pHdr->m_type, pHdr->m_count, pDescRet);
	if (status) {
		return status;
	}

	if (pDescRet==NULL) {
		return S_cas_noMemory;
	}

	//
	// the PV state must not be modified during a transaction
	//
	status = (*this->ctx.getPV())->beginTransaction();
	if (status) {
		return status;
	}

	//
	// clear the async IO flag
	//
	this->asyncIOFlag = 0u;

	//
	// call the server tool's virtual function
	//
	status = (*this->ctx.getPV())->read(this->ctx, *pDescRet);

	//
	// prevent problems when they initiate
	// async IO but dont return status
	// indicating so (and vise versa)
	//
	if (this->asyncIOFlag) {
		if (status!=S_casApp_asyncCompletion) {
			fprintf(stderr, 
"Application returned %d from casPV::read() - expected S_casApp_asyncCompletion\n",
				status);
			status = S_casApp_asyncCompletion;
		}
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_badParameter;
		errMessage(status, 
			"- expected asynch IO creation from casPV::read()");
	}

	if (status) {
		pDescRet = NULL;
	}

	(*this->ctx.getPV())->endTransaction();

	return status;
}

//
// createDBRDD ()
//
caStatus createDBRDD (unsigned dbrType, aitIndex dbrCount, smartGDDPointer &pDescRet)
{
	aitUint32 valIndex;
	aitUint32 gddStatus;
	aitUint16 appType;
	gdd *pVal;

	appType = gddDbrToAit[dbrType].app;

	//
	// create the descriptor
	//
	pDescRet = gddApplicationTypeTable::app_table.getDD (appType);
	if (!pDescRet) {
		return S_cas_noMemory;
	}

	//
	// smart pointer class maintains the ref count from here down
	//
	gddStatus = pDescRet->unreference();
	assert (!gddStatus);
	
	if (pDescRet->isContainer()) {
		gdd *pGdd = pDescRet;
		//
		// this cast is ugly and dangerous
		// (Jim should have used virtual functions in gdd)
		//
		gddContainer *pCont = (gddContainer *) pGdd;

		//
		// All DBR types have a value member 
		//
		gddStatus = 
			gddApplicationTypeTable::app_table.mapAppToIndex
				(appType, gddAppType_value, valIndex);
		if (gddStatus) {
			pDescRet = NULL;
			return S_cas_internal;
		}
		pVal = pCont->getDD(valIndex);
		if (!pVal) {
			pDescRet = NULL;
			return S_cas_internal;
		}
	}
	else {
		pVal = pDescRet;
	}

	if (pVal->isScalar()) {
		if (dbrCount<=1u) {
			return S_cas_success;
		}

		//
		// scaler and managed (and need to set the bounds)
		//	=> out of luck (cant modify bounds)
		//
		if (pDescRet->isManaged()) {
			pDescRet = NULL;
			return S_cas_internal;
		}

		//
		// convert to atomic
		//
		gddBounds bds;
		bds.setSize(dbrCount);
		bds.setFirst(0u);
		pVal->setDimension(1u, &bds);
	}
	else if (pVal->isAtomic()) {
		const gddBounds* pB = pVal->getBounds();
		aitIndex bound = dbrCount;
		unsigned dim;
		int modAllowed;

		if (pDescRet->isManaged() || pDescRet->isFlat()) {
			modAllowed = FALSE;
		}
		else {
			modAllowed = TRUE;
		}

		for (dim=0u; dim<(unsigned)pVal->dimension(); dim++) {
			if (pB[dim].first()!=0u && pB[dim].size()!=bound) {
				if (modAllowed) {
					pVal->setBound(dim, 0u, bound);
				}
				else {
					pDescRet = NULL;
					return S_cas_internal;
				}
			}
			bound = 1u;
		}
	}
	else {
		//
		// the GDD is container or isnt any of the normal types
		//
		pDescRet = NULL;
		return S_cas_internal;
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
        this->osiLock();
        this->getCAS().installItem(chan);
        this->chanList.add(chan);
        this->osiUnlock();
}
 
//
// casStrmClient::removeChannel()
//
void casStrmClient::removeChannel(casChannelI &chan)
{
        casRes *pRes;
 
        this->osiLock();
        pRes = this->getCAS().removeItem(chan);
        assert (&chan == (casChannelI *)pRes);
        this->chanList.remove(chan);
        this->osiUnlock();
}

//
//  casStrmClient::xSend()
//
xSendStatus casStrmClient::xSend(char *pBufIn, bufSizeT nBytesAvailableToSend,
        bufSizeT nBytesNeedToBeSent, bufSizeT &nActualBytes)
{
        xSendStatus stat;
        bufSizeT nActualBytesDelta;
 
        assert (nBytesAvailableToSend>=nBytesNeedToBeSent);
 
        nActualBytes = 0u;
        if (this->blockingState() == xIsntBlocking) {
		//
		// !! this time fetch may be slowing things down !!
		//
                stat = this->osdSend(pBufIn, nBytesAvailableToSend, 
				nActualBytes);
                if (stat == xSendOK) {
                        this->elapsedAtLastSend = osiTime::getCurrent();
                }
                return stat;
        }
 
	nActualBytes = 0u;
        while (TRUE) {
                stat = this->osdSend(&pBufIn[nActualBytes], 
				nBytesAvailableToSend, nActualBytesDelta);
                if (stat != xSendOK) {
                        return stat;
                }
 
		//
		// !! this time fetch may be slowing things down !!
		//
                this->elapsedAtLastSend = osiTime::getCurrent();
                nActualBytes += nActualBytesDelta;

		if (nBytesNeedToBeSent <= nActualBytesDelta) {
			break;
		}
		nBytesAvailableToSend -= nActualBytesDelta;
                nBytesNeedToBeSent -= nActualBytesDelta;
        }
        return xSendOK;
}

//
// casStrmClient::xRecv()
//
xRecvStatus casStrmClient::xRecv(char *pBufIn, bufSizeT nBytes,
        bufSizeT &nActualBytes)
{
        xRecvStatus stat;
 
        stat = this->osdRecv(pBufIn, nBytes, nActualBytes);
        if (stat==xRecvOK) {
		//
		// !! this time fetch may be slowing things down !!
		//
                this->elapsedAtLastRecv = osiTime::getCurrent();
        }
        return stat;
}

//
// casStrmClient::getDebugLevel()
//
unsigned casStrmClient::getDebugLevel() const
{
	return this->getCAS().getDebugLevel();
}

