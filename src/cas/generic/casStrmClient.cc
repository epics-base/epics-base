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
 *
 */

#include "dbMapper.h"		// ait to dbr types
#include "gddAppTable.h"    // EPICS application type table
#include "gddApps.h"		// gdd predefined application type codes
typedef unsigned long arrayElementCount;
#include "net_convert.h"	// byte order conversion from libca

#include "server.h"
#include "caServerIIL.h"	// caServerI inline functions
#include "casChannelIIL.h"	// casChannelI inline functions
#include "casCtxIL.h"		// casCtx inline functions
#include "casEventSysIL.h"	// casEventSys inline functions
#include "inBufIL.h"		// inBuf inline functions
#include "outBufIL.h"		// outBuf inline functions

static const caHdr nill_msg = {0u,0u,0u,0u,0u,0u};

//
// casStrmClient::casStrmClient()
//
casStrmClient::casStrmClient ( caServerI &serverInternal ) :
	casClient ( serverInternal, 1 )
{
	this->lock ();

	this->ctx.getServer()->installClient ( this );

    this->pHostName = new char [1u];
    if ( ! this->pHostName ) {
        throw S_cas_noMemory;
    }
    *this->pHostName = '\0';

    this->pUserName = new char [1u];
    if ( ! this->pUserName ) {
        free ( this->pHostName );
        throw S_cas_noMemory;
    }
    *this->pUserName= '\0';

	this->unlock ();
}

//
// casStrmClient::~casStrmClient()
//
casStrmClient::~casStrmClient()
{
	this->lock();

	//
	// remove this from the list of connected clients
	//
	this->ctx.getServer()->removeClient(this);

	delete [] this->pUserName;

	delete [] this->pHostName;

	//
	// delete all channel attached
	//
	tsDLIterBD <casChannelI> iter = this->chanList.firstIter ();
	while ( iter.valid () ) {
		//
		// destroying the channel removes it from the list
		//
		tsDLIterBD<casChannelI> tmp = iter;
		++tmp;
		iter->destroyNoClientNotify();
		iter = tmp;
	}

	this->unlock();
}

//
// casStrmClient::uknownMessageAction()
//
caStatus casStrmClient::uknownMessageAction ()
{
	const caHdr *mp = this->ctx.getMsg();
	caStatus status;

	this->dumpMsg (mp, this->ctx.getData(),
        "bad request code from virtual circuit=%u\n", mp->m_cmmd);

	/* 
	 *	most clients dont recover from this
	 */
	status = this->sendErr (mp, ECA_INTERNAL, "Invalid Request Code");
	if (status) {
		return status;
	}

	/*
	 * returning S_cas_internal here disconnects
	 * the client with the bad message
	 */
	return S_cas_internal;
}

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
		return ECA_BADCHID;
	}

	//
	// data type out of range ?
	//
	if (mp->m_dataType>((unsigned)LAST_BUFFER_TYPE)) {
		return ECA_BADTYPE;
	}

	//
	// element count out of range ?
	//
	if (mp->m_count > pChan->getPVI().nativeCount() || mp->m_count==0u) {
		return ECA_BADCOUNT;
	}

	return ECA_NORMAL;
}

//
// find the monitor associated with a resource id
//
inline casClientMon *caServerI::resIdToClientMon (const caResId &idIn)
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
	printf ( "casStrmClient at %p\n", 
        static_cast <const void *> ( this ) );
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
	if (status != ECA_NORMAL) {
		return this->sendErr(mp, status, "get request");
	}

	/*
	 * verify read access
	 */
	if (!pChan->readAccess()) {
		int	v41;

		v41 = CA_V41(this->minor_version_number);
		if(v41){
			status = ECA_NORDACCESS;
		}
		else{
			status = ECA_GETFAIL;
		}

		return this->sendErr(mp, status, "read access denied");
	}

	status = this->read (pDesc); 
	if (status==S_casApp_success) {
		status = this->readResponse(pChan, *mp, *pDesc, S_cas_success);
	}
	else if (status == S_casApp_asyncCompletion) {
		status = S_cas_success;
	}
	else if (status == S_casApp_postponeAsyncIO) {
		pChan->getPVI().addItemToIOBLockedList(*this);
	}
	else {
		status = this->sendErrWithEpicsStatus (mp, status, ECA_GETFAIL);
	}

	return status;
}

//
// casStrmClient::readResponse()
//
caStatus casStrmClient::readResponse (casChannelI *pChan, const caHdr &msg, 
					const smartConstGDDPointer &pDesc, const caStatus status)
{
	caHdr 		*reply;
	unsigned	size;
	caStatus	localStatus;
	int			mapDBRStatus;
	int        	strcnt;

	if (status!=S_casApp_success) {
		return this->sendErrWithEpicsStatus(&msg, status, ECA_GETFAIL);
	}

	size = dbr_size_n (msg.m_dataType, msg.m_count);
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
	assert ( size <= 0xffff );
	reply->m_postsize = static_cast <ca_uint16_t> ( size );
	reply->m_cid = pChan->getCID();

	//
	// convert gdd to db_access type
	// (places the data in network format)
	//
	mapDBRStatus = gddMapDbr[msg.m_dataType].conv_dbr((reply+1), msg.m_count, *pDesc, pChan->enumStringTable());
	if (mapDBRStatus<0) {
		pDesc->dump();
		errPrintf (S_cas_badBounds, __FILE__, __LINE__, "- get with PV=%s type=%u count=%u",
				pChan->getPVI().getName(), msg.m_dataType, msg.m_count);
		return this->sendErrWithEpicsStatus(&msg, S_cas_badBounds, ECA_GETFAIL);
	}
#ifdef CONVERSION_REQUIRED
	/* use type as index into conversion jumptable */
	(* cac_dbr_cvrt[msg.m_dataType])
		( reply + 1,
		  reply + 1,
		  TRUE,       /* host -> net format */
		  msg.m_count);
#endif
	//
	// force string message size to be the true size rounded to even
	// boundary
	//
	if (msg.m_dataType == DBR_STRING && msg.m_count == 1u) {
		/* add 1 so that the string terminator will be shipped */
		strcnt = strlen((char *)(reply + 1u)) + 1u;
		assert ( strcnt <= 0xffff );
		reply->m_postsize = static_cast <ca_uint16_t> (strcnt);
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
	if (status != ECA_NORMAL) {
		return this->readNotifyResponseECA_XXX (NULL, *mp, NULL, status);
	}

	//
	// verify read access
	// 
	if (!pChan->readAccess()) {
		if (CA_V41(this->minor_version_number)) {
			return this->readNotifyResponseECA_XXX (NULL, *mp, NULL, ECA_NORDACCESS);
		}
		else {
			return this->readNotifyResponse (NULL, *mp, NULL, S_cas_noRead);
		}
	}

	status = this->read (pDesc); 
	if (status == S_casApp_success) {
		status = this->readNotifyResponse (pChan, *mp, pDesc, status);
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
		const caHdr &msg, const smartConstGDDPointer &pDesc, const caStatus completionStatus)
{
	caStatus ecaStatus;

	if (completionStatus!=S_cas_success) {
		ecaStatus = ECA_GETFAIL;
	}
	else {
		ecaStatus = ECA_NORMAL;
	}
	ecaStatus = this->readNotifyResponseECA_XXX (pChan, msg, pDesc, ecaStatus);
	if (ecaStatus) {
		return ecaStatus;
	}

	//
	// send independent warning exception to the client so that they
	// will see the error string associated with this error code 
	// since the error string cant be sent with the get call back 
	// response (hopefully this is useful information)
	//
	// order is very important here because it determines that the get 
	// call back response is always sent, and that this warning exception
	// message will be sent at most one time (in rare instances it will
	// not be sent, but at least it will not be sent multiple times).
	// The message is logged to the console in the rare situations when
	// we are unable to send.
	//
	if (completionStatus!=S_cas_success) {
		ecaStatus = this->sendErrWithEpicsStatus (&msg, completionStatus, ECA_NOCONVERT);
		if (ecaStatus) {
			errMessage (completionStatus, "<= get callback failure detail not passed to client");
		}
	}
	return S_cas_success;
}

//
// casStrmClient::readNotifyResponseECA_XXX ()
//
caStatus casStrmClient::readNotifyResponseECA_XXX (casChannelI *pChan, 
		const caHdr &msg, const smartConstGDDPointer &pDesc, const caStatus ecaStatus)
{
	caHdr 		*reply;
	unsigned	size;
	caStatus	status;

	size = dbr_size_n (msg.m_dataType, msg.m_count);
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
	assert ( size <= 0xffff );
	reply->m_postsize = static_cast<ca_uint16_t> (size);

	//
	// cid field abused to store the status here
	//
	if (ecaStatus == ECA_NORMAL) {
		if (!pDesc) {
			errMessage(S_cas_badParameter, 
			"because no data in server tool asynch read resp");
			reply->m_cid = ECA_GETFAIL;
		}
		else {
			int mapDBRStatus;
			//
			// convert gdd to db_access type
			// (places the data in network format)
			//
			mapDBRStatus = gddMapDbr[msg.m_dataType].conv_dbr((reply+1), msg.m_count, *pDesc, pChan->enumStringTable());
			if (mapDBRStatus<0) {
				pDesc->dump();
				errPrintf (S_cas_badBounds, __FILE__, __LINE__, "- get notify with PV=%s type=%u count=%u",
					pChan->getPVI().getName(), msg.m_dataType, msg.m_count);
				reply->m_cid = ECA_GETFAIL;
			}
			else {
				reply->m_cid = ECA_NORMAL;
			}
		}
	}
	else {
		reply->m_cid = ecaStatus;
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
	else {

		/* use type as index into conversion jumptable */
		(* cac_dbr_cvrt[msg.m_dataType])
			( reply + 1,
			  reply + 1,
			  TRUE,       /* host -> net format */
			  msg.m_count);
	}
#endif

	//
	// force string message size to be the true size rounded to even
	// boundary
	//
	if (msg.m_dataType == DBR_STRING && msg.m_count == 1u) {
		/* add 1 so that the string terminator will be shipped */
		size_t strcnt = strlen((char *)(reply + 1u)) + 1u;
		assert ( strcnt < 0xffff );
		reply->m_postsize = static_cast <ca_uint16_t> ( strcnt );
	}

	this->commitMsg ();

	return S_cas_success;
}

//
// createDBRDD ()
//
static smartGDDPointer createDBRDD (unsigned dbrType, aitIndex dbrCount)
{
    smartGDDPointer pDescRet;
	aitUint32 valIndex;
	aitUint32 gddStatus;
	aitUint16 appType;
    gdd *pVal;
	
	/*
	 * DBR type has already been checked, but it is possible
	 * that "gddDbrToAit" will not track with changes in
	 * the DBR_XXXX type system
	 */
	if (dbrType>=NELEMENTS(gddDbrToAit)) {
		return pDescRet;
	}
	if (gddDbrToAit[dbrType].type==aitEnumInvalid) {
		return pDescRet;
	}
	appType = gddDbrToAit[dbrType].app;
	
	//
	// create the descriptor
	//
	pDescRet = gddApplicationTypeTable::app_table.getDD (appType);
	if ( ! pDescRet.valid () ) {
		return pDescRet;
	}

	//
	// smart pointer class maintains the ref count from here down
	//
	gddStatus = pDescRet->unreference();
	assert (!gddStatus);
	
	if ( pDescRet->isContainer () ) {
		
		//
		// unable to change the bounds on the managed GDD that is
		// returned for DBR types
		//
		if ( dbrCount > 1 ) {
			pDescRet = new gdd (*pDescRet);
			//
			// smart pointer class maintains the ref count from here down
			//
			gddStatus = pDescRet->unreference();
			assert (!gddStatus);
		}

		//
		// All DBR types have a value member 
		//
		gddStatus = 
			gddApplicationTypeTable::app_table.mapAppToIndex
			(appType, gddAppType_value, valIndex);
		if ( gddStatus ) {
			pDescRet = NULL;
			return pDescRet;
		}
		pVal = pDescRet->getDD ( valIndex );
		if ( ! pVal ) {
            pDescRet = NULL;
			return pDescRet;
		}
	}
	else {
		pVal = & ( *pDescRet );
	}
	
	if ( pVal->isScalar () ) {
		if (dbrCount<=1u) {
			return pDescRet;
		}
		
		//
		// scalar and managed (and need to set the bounds)
		//	=> out of luck (cant modify bounds)
		//
		if ( pDescRet->isManaged () ) {
			pDescRet = NULL;
			return pDescRet;
		}
		
		//
		// convert to atomic
		//
		gddBounds bds;
		bds.setSize ( dbrCount );
		bds.setFirst ( 0u );
		pVal->setDimension ( 1u, &bds );
	}
	else if ( pVal->isAtomic () ) {
		const gddBounds* pB = pVal->getBounds ();
		aitIndex bound = dbrCount;
		unsigned dim;
		int modAllowed;
		
		if ( pDescRet->isManaged () || pDescRet->isFlat () ) {
			modAllowed = FALSE;
		}
		else {
			modAllowed = TRUE;
		}
		
		for ( dim=0u; dim < pVal->dimension (); dim++ ) {
			if ( pB[dim].first () != 0u && pB[dim].size() != bound ) {
				if ( modAllowed ) {
					pVal->setBound( dim, 0u, bound );
				}
				else {
					pDescRet = NULL;
					return pDescRet;
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
		return pDescRet;
	}
	
	return pDescRet;
}

//
// casStrmClient::monitorResponse ()
//
caStatus casStrmClient::monitorResponse (casChannelI &chan, const caHdr &msg, 
		const smartConstGDDPointer &pDesc, const caStatus completionStatus)
{
	caStatus completionStatusCopy = completionStatus;
	smartGDDPointer pDBRDD;
	caHdr *pReply;
	unsigned size;
	caStatus status;
	gddStatus gdds;

	size = dbr_size_n (msg.m_dataType, msg.m_count);
	status = this->allocMsg(size, &pReply);
	if (status) {
		if (status==S_cas_hugeRequest) {
			//
			// If we cant include the data - it is a proto
			// violation - so we generate an exception
			// instead
			//
			status = sendErr (&msg, ECA_TOLARGE, 
					"unable to xmit event");
		}
		return status;
	}

	//
	// setup response message
	//
	*pReply = msg;
	assert ( size <= 0xffff );
	pReply->m_postsize = static_cast<ca_uint16_t> (size);

	//
	// verify read access
	//
	if (!chan.readAccess()) {
		completionStatusCopy = S_cas_noRead;
	}

	//
	// cid field abused to store the status here
	//
	if (completionStatusCopy == S_cas_success) {

		if (!pDesc) {
            completionStatusCopy = S_cas_badParameter;
		}
		else {
			pDBRDD = createDBRDD (msg.m_dataType, msg.m_count);
            if (!pDBRDD) {
                completionStatusCopy = S_cas_noMemory;
            }
            else {
				gdds = gddApplicationTypeTable::
					app_table.smartCopy ( & (*pDBRDD), & (*pDesc) );
				if (gdds) {
					errPrintf (status, __FILE__, __LINE__,
"no conversion between event app type=%d and DBR type=%d Element count=%d",
						pDesc->applicationType(),
						msg.m_dataType,
						msg.m_count);
					completionStatusCopy = S_cas_noConvert;
				}
			}
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
		gddMapDbr[msg.m_dataType].conv_dbr ((pReply+1), msg.m_count, *pDBRDD, chan.enumStringTable());

#ifdef CONVERSION_REQUIRED
		/* use type as index into conversion jumptable */
		(* cac_dbr_cvrt[msg.m_dataType])
			( pReply + 1,
			  pReply + 1,
			  TRUE,       /* host -> net format */
			  msg.m_count);
#endif
		//
		// force string message size to be the true size 
		//
		if (msg.m_dataType == DBR_STRING && msg.m_count == 1u) {
			// add 1 so that the string terminator 
			// will be shipped 
			size_t strcnt = strlen((char *)(pReply + 1u)) + 1u;
			assert ( strcnt < 0xffff );
			pReply->m_postsize = static_cast <ca_uint16_t> ( strcnt );
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
	if (status != ECA_NORMAL) {
		return this->sendErr(mp, status, "put request");
	}

	//
	// verify write access
	// 
	if (!pChan->writeAccess()) {
		int	v41;

		v41 = CA_V41(this->minor_version_number);
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
caStatus casStrmClient::writeResponse ( 
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
	if (status != ECA_NORMAL) {
		return casStrmClient::writeNotifyResponseECA_XXX(*mp, status);
	}

	//
	// verify write access
	// 
	if (!pChan->writeAccess()) {
		if (CA_V41(this->minor_version_number)) {
			return this->casStrmClient::writeNotifyResponseECA_XXX(
					*mp, ECA_NOWTACCESS);
		}
		else {
			return this->casStrmClient::writeNotifyResponse(
					*mp, S_cas_noWrite);
		}
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
		status = casStrmClient::writeNotifyResponse(*mp, status);
	}

	return status;
}

/* 
 * casStrmClient::writeNotifyResponse()
 */
caStatus casStrmClient::writeNotifyResponse(
		const caHdr &msg, const caStatus completionStatus)
{
	caStatus ecaStatus;

	if (completionStatus==S_cas_success) {
		ecaStatus = ECA_NORMAL;
	}
	else {
		ecaStatus = ECA_PUTFAIL;	
	}

	ecaStatus = this->casStrmClient::writeNotifyResponseECA_XXX(msg, ecaStatus);
	if (ecaStatus) {
		return ecaStatus;
	}

	//
	// send independent warning exception to the client so that they
	// will see the error string associated with this error code 
	// since the error string cant be sent with the put call back 
	// response (hopefully this is useful information)
	//
	// order is very important here because it determines that the put 
	// call back response is always sent, and that this warning exception
	// message will be sent at most one time. In rare instances it will
	// not be sent, but at least it will not be sent multiple times.
	// The message is logged to the console in the rare situations when
	// we are unable to send.
	//
	if (completionStatus!=S_cas_success) {
		ecaStatus = this->sendErrWithEpicsStatus (&msg, completionStatus, ECA_NOCONVERT);
		if (ecaStatus) {
			errMessage (completionStatus, "<= put callback failure detail not passed to client");
		}
	}
	return S_cas_success;
}

/* 
 * casStrmClient::writeNotifyResponseECA_XXX()
 */
caStatus casStrmClient::writeNotifyResponseECA_XXX(
		const caHdr &msg, const caStatus ecaStatus)
{
	caHdr		*preply;
	caStatus	opStatus;

	opStatus = this->allocMsg(0u, &preply);
	if (opStatus) {
		return opStatus;
	}

	*preply = msg;
	preply->m_postsize = 0u;
	preply->m_cid = ecaStatus;

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
	caStatus		status;

	size = strlen(pName)+1u;
	/*
	 * user name will not change if there isnt enough memory
	 */
	pMalloc = new char [size];
	if(!pMalloc){
		status = this->sendErr(mp, ECA_ALLOCMEM, pName);
		if (status) {
			return status;
		}
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

	tsDLIterBD <casChannelI> iter = this->chanList.firstIter ();
	while ( iter.valid () ) {
		iter->setOwner(this->pUserName, this->pHostName);
		++iter;
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
	unsigned		size;
	char 			*pMalloc;
	caStatus		status;

	size = strlen(pName)+1;

	/*
	 * user name will not change if there isnt enough memory
	 */
	pMalloc = new char [size];
	if(!pMalloc){
		status = this->sendErr(mp, ECA_ALLOCMEM, pName);
		if (status) {
			return status;
		}
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

	tsDLIterBD <casChannelI> iter = this->chanList.firstIter ();
	while ( iter.valid () ) {
		iter->setOwner ( this->pUserName, this->pHostName );
		++iter;
	}
	this->unlock();

	return S_cas_success;
}

/*
 * casStrmClientMon::claimChannelAction()
 */
caStatus casStrmClient::claimChannelAction()
{
	const caHdr *mp = this->ctx.getMsg();
	char *pName = (char *) this->ctx.getData();
	caServerI &cas = *this->ctx.getServer();
	caStatus status;

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
	if (!CA_V44(this->minor_version_number)) {
		//
		// old connect protocol was dropped when the
		// new API was added to the server (they must
		// now use clients at EPICS 3.12 or higher)
		//
		status = this->sendErr(mp, ECA_DEFUNCT,
				"R3.11 connect sequence from old client was ignored");
		if (status) {
			return status;
		}
		return S_cas_badProtocol; // disconnect client
	}

	if (mp->m_postsize <= 1u) {
		return S_cas_badProtocol; // disconnect client
	}

    pName[mp->m_postsize-1u] = '\0';

	if ( ( mp->m_postsize - 1u ) > unreasonablePVNameSize ) {
		return S_cas_badProtocol; // disconnect client
	}

	//
	// prevent problems such as the PV being deleted before the
	// channel references it
	//
	this->lock();
	this->asyncIOFlag = 0u;

	//
	// attach to the PV
	//
	pvAttachReturn pvar = cas->pvAttach (this->ctx, pName);

	//
	// prevent problems when they initiate
	// async IO but dont return status
	// indicating so (and vise versa)
	//
	if (this->asyncIOFlag) {
		status = S_cas_success;	
	}
	else if (pvar.getStatus() == S_casApp_asyncCompletion) {
		status = this->createChanResponse(*mp, S_cas_badParameter);
		errMessage(S_cas_badParameter, 
		"- expected asynch IO creation from caServer::pvAttach()");
	}
	else if (pvar.getStatus() == S_casApp_postponeAsyncIO) {
		status = S_casApp_postponeAsyncIO;
		this->ctx.getServer()->addItemToIOBLockedList(*this);
	}
	else {
		status = this->createChanResponse(*mp, pvar);
	}
	this->unlock();
	return status;
}

//
// casStrmClient::createChanResponse()
//
// LOCK must be applied
//
caStatus casStrmClient::createChanResponse(const caHdr &hdr, const pvAttachReturn &pvar)
{
	casPVI		*pPV;
	casChannel 	*pChan;
	casChannelI 	*pChanI;
	caHdr 		*claim_reply; 
    bufSizeT    nBytes;
	caStatus	status;

	if (pvar.getStatus() != S_cas_success) {
		return this->channelCreateFailed (&hdr, pvar.getStatus());
	}

	pPV = pvar.getPV();

	//
	// If status is ok and the PV isnt set then guess that the
	// pv isnt in this server
	//
	if (pPV == NULL) {
		return this->channelCreateFailed (&hdr, S_casApp_pvNotFound);
	}

    //
    // fetch the native type
    //
	unsigned	nativeType;
	status = pPV->bestDBRType(nativeType);
	if (status) {
		errMessage(status, "best external dbr type fetch failed");
		return this->channelCreateFailed (&hdr, status);
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
	// response and the access rights response so that we know for
	// certain that they will both be sent together.
	//
    void *pRaw;
    const outBufCtx outctx = this->outBuf::pushCtx 
                    (0, 2*sizeof(caHdr), pRaw);
    if (outctx.pushResult()!=outBufCtx::pushCtxSuccess) {
        return S_cas_sendBlocked;
    }

	//
	// create server tool XXX derived from casChannel
	//
	this->ctx.setPV (pPV);
	pChan = pPV->createChannel (this->ctx, this->pUserName, this->pHostName);
	if (!pChan) {
        this->outBuf::popCtx (outctx);
		pPV->deleteSignal();
		return this->channelCreateFailed (&hdr, S_cas_noMemory);
	}

	pChanI = (casChannelI *) pChan;

	//
	// NOTE:
	// We are certain that the request will complete
	// here because we allocated enough space for this
	// and the claim response above.
	//
	status = casStrmClient::accessRightsResponse(pChanI);
	if (status) {
        this->outBuf::popCtx (outctx);
		errMessage(status, "incomplete channel create?");
		pChanI->destroyNoClientNotify();
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
	status = this->allocMsg (0u, &claim_reply);
    if (status!=S_cas_success) {
        this->outBuf::popCtx (outctx);
		errMessage(status, "incomplete channel create?");
		pChanI->destroyNoClientNotify();
		return this->channelCreateFailed(&hdr, status);
	}

	*claim_reply = nill_msg;
	claim_reply->m_cmmd = CA_PROTO_CLAIM_CIU;
	assert ( nativeType <= 0xffff );
	claim_reply->m_dataType = static_cast <ca_uint16_t> (nativeType);
	unsigned nativeCount = pPV->nativeCount();
	assert ( nativeType <= 0xffff );
	claim_reply->m_count = static_cast <ca_uint16_t> (nativeCount);
	claim_reply->m_cid = hdr.m_cid;
	claim_reply->m_available = pChanI->getSID();
    this->commitMsg();

    //
    // commit the message
    //
    nBytes = this->outBuf::popCtx (outctx);
    assert ( nBytes == 2*sizeof(caHdr) );
    this->outBuf::commitRawMsg (nBytes);

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
    caStatus status;
    caHdr *reply;
 
	if (createStatus == S_casApp_asyncCompletion) {
		errMessage(S_cas_badParameter, 
	"- no asynchronous IO create in pvAttach() ?");
		errMessage(S_cas_badParameter, 
	"- or S_casApp_asyncCompletion was async IO competion code ?");
	}
	else {
		errMessage (createStatus, "- Server unable to create a new PV");
	}
	if (CA_V46(this->minor_version_number)) {

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
		status = this->sendErrWithEpicsStatus(mp, createStatus, ECA_ALLOCMEM);
		if (status) {
			return status;
		}
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

	if (CA_V47(this->minor_version_number)) {

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
		errlogPrintf(
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
	this->casEventSys::eventsOn();
	return S_cas_success;
}

//
// casStrmClient::eventsOffAction()
//
caStatus casStrmClient::eventsOffAction()
{
	return this->casEventSys::eventsOff();
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
	if (status != ECA_NORMAL) {
		return this->sendErr(mp, status, NULL);
	}

	//
	// place monitor mask in correct byte order
	//
	caProtoMask = epicsNTOH16 (pMonInfo->m_mask);
	if (caProtoMask&DBE_VALUE) {
		mask |= this->getCAS().valueEventMask();
	}

	if (caProtoMask&DBE_LOG) {
		mask |= this->getCAS().logEventMask();
	}
	
	if (caProtoMask&DBE_ALARM) {
		mask |= this->getCAS().alarmEventMask();
	}

	if (mask.noEventsSelected()) {
		char errStr[40];
		sprintf(errStr, "event add req with mask=0X%X\n", caProtoMask);
		return this->sendErr(mp, ECA_BADMASK, errStr);
	}

	//
	// Attempt to read the first monitored value prior to creating
	// the monitor object so that if the server tool chooses
	// to postpone asynchronous IO we can safely restart this
	// request later.
	//
	status = this->read (pDD); 
	//
	// always send immediate monitor response at event add
	//
	if (status == S_casApp_success) {
		status = this->monitorResponse (*pciu, *mp, pDD, status);
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
		pciu->destroyClientNotify ();
		return S_cas_success;
	}
	else {
		status = this->monitorResponse (*pciu, *mp, pDD, status);
	}

	if (status==S_cas_success) {

		pMonitor = new casClientMon(*pciu, mp->m_available, 
					mp->m_count, mp->m_dataType, mask, *this);
		if (!pMonitor) {
			status = this->sendErr(mp, ECA_ALLOCMEM, NULL);
			if (status==S_cas_success) {
				//
				// If we cant allocate space for a monitor then
				// delete (disconnect) the channel
				//
				pciu->destroyClientNotify ();
			}
			return status;
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
		/*
		 * it is possible that the channel delete arrives just 
		 * after the server tool has deleted the PV so we will
		 * not disconnect the client in this case. Nevertheless,
		 * we send a warning message in case either the client 
		 * or server has become corrupted
		 *
		 * return early here if we are unable to send the warning
		 * so that send block conditions will be handled
		 */
		status = logBadId (mp, dp, ECA_BADCHID, mp->m_cid);
		if (status) {
			return status;
		}
		//
		// after sending the warning then go ahead and send the
		// delete confirm message even if the channel couldnt be
		// located so that the client can finish cleaning up
		//
	}

	/*
	 * send delete confirmed message
	 */
	status = this->allocMsg (0u, &reply);
	if (status) {
		return status;
	}

	//
	// only execute the request after we have allocated
	// space for the response
	//
	if (pciu) {
		pciu->destroyNoClientNotify ();
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
	const caHdr *mp = this->ctx.getMsg ();
	void *dp = this->ctx.getData ();
	casChannelI *pciu;
	caHdr *reply;
	int status;
	
	/*
	 * Verify the channel
	 */
	pciu = this->resIdToChannel (mp->m_cid);
	if (!pciu) {
		/*
		 * it is possible that the event delete arrives just 
		 * after the server tool has deleted the PV. In this
		 * rare situation we are unable to look up the client's
		 * resource id for the return message and so we must force
		 * the client to reconnect.
		 */
		return logBadId (mp, dp, ECA_BADCHID, mp->m_cid);
	}

	/*
	 * verify the event (monitor)
	 */
    tsDLIterBD <casMonitor> pMon = pciu->findMonitor ( mp->m_available );
	if ( ! pMon.valid () ) {
		//
		// this indicates client or server library corruption
		//
		return logBadId ( mp, dp, ECA_BADMONID, mp->m_cid );
	}

	/*
	 * allocate delete confirmed message
	 */
	status = allocMsg ( 0u, &reply );
	if ( status ) {
		return status;
	}
	
	reply->m_cmmd = CA_PROTO_EVENT_ADD;
	reply->m_postsize = 0u;
	unsigned type = pMon->getType ();
	assert ( type <= 0xff );
	reply->m_dataType = static_cast <unsigned char> ( type );
	reply->m_count = (unsigned short) pMon->getCount ();
	reply->m_cid = pciu->getCID ();
	reply->m_available = pMon->getClientId ();
	
	this->commitMsg ();
	
	pMon->destroy ();
	
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
	falseReply.m_dataType = pMon->getType();
	falseReply.m_count = pMon->getCount();
	falseReply.m_cid = pMon->getChannel().getCID();
	falseReply.m_available = pMon->getClientId();

	status = this->allocMsg(size, &reply);
	if (status) {
		if(status == S_cas_hugeRequest){
			return this->sendErr(&falseReply, ECA_TOLARGE, NULL);
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
	this->lock();
	tsDLIterBD <casChannelI> iter = this->chanList.firstIter ();
	while ( iter.valid () ) {
		iter->clearOutstandingReads ();
		++iter;
	}
	this->unlock();

	status = this->allocMsg ( 0u, &reply );
	if ( status ) {
		return status;
	}

	*reply = *mp;

	this->commitMsg ();

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
    caHdr       *reply;
    unsigned    ar;
    int         v41;
    int         status;
    
    /*
    * noop if this is an old client
    */
    v41 = CA_V41(this->minor_version_number);
    if(!v41){
        return S_cas_success;
    }
    
    ar = 0; /* none */
    if (pciu->readAccess()) {
        ar |= CA_PROTO_ACCESS_RIGHT_READ;
    }
    if (pciu->writeAccess()) {
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
	if (dbr_value_offset[pHdr->m_dataType]) {
		return S_cas_badType;
	}

#ifdef CONVERSION_REQUIRED
	/* use type as index into conversion jumptable */
	(* cac_dbr_cvrt[pHdr->m_dataType])
		( this->ctx.getData(),
		  this->ctx.getData(),
		  FALSE,       /* net -> host format */
		  pHdr->m_count);
#endif

	//
	// the PV state must not be modified during a transaction
	//
	status = pPV->beginTransaction();
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

	pPV->endTransaction();

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

	/*
	 * DBR type has already been checked, but it is possible
	 * that "gddDbrToAit" will not track with changes in
	 * the DBR_XXXX type system
	 */
	if (pHdr->m_dataType>=NELEMENTS(gddDbrToAit)) {
		return S_cas_badType;
	}
	type = gddDbrToAit[pHdr->m_dataType].type;
	if (type==aitEnumInvalid) {
		return S_cas_badType;
	}

    aitEnum	bestExternalType = this->ctx.getPV()->bestExternalType ();

	pDD = new gddScalar (gddAppType_value, bestExternalType);
	if (!pDD) {
		return S_cas_noMemory;
	}

	//
	// reference count is managed by smart pointer class
	// from here down
	//
	gddStat = pDD->unreference();
	assert (!gddStat);

    //
    // copy in, and convert to native type, the incoming data
    //
    gddStat = aitConvert (pDD->primitiveType(), pDD->dataAddress(), type, 
        this->ctx.getData(), 1, &this->ctx.getPV()->enumStringTable());
    if (gddStat<0) { 
        status = S_cas_noConvert;
    }
    else {
        //
        // set the status and severity to normal
        //
	    pDD->setStat (epicsAlarmNone);
	    pDD->setSevr (epicsSevNone);

        //
        // set the time stamp to the last time that
        // we added bytes to the in buf
        //
        aitTimeStamp gddts = this->lastRecvTS;
        pDD->setTimeStamp (&gddts);

	    //
	    // call the server tool's virtual function
	    //
	    status = this->ctx.getPV()->write (this->ctx, *pDD);
    }
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

	/*
	 * DBR type has already been checked, but it is possible
	 * that "gddDbrToAit" will not track with changes in
	 * the DBR_XXXX type system
	 */
	if (pHdr->m_dataType>=NELEMENTS(gddDbrToAit)) {
		return S_cas_badType;
	}
	type = gddDbrToAit[pHdr->m_dataType].type;
	if (type==aitEnumInvalid) {
		return S_cas_badType;
	}

    aitEnum	bestExternalType = this->ctx.getPV()->bestExternalType ();

	pDD = new gddAtomic(gddAppType_value, bestExternalType, 1, pHdr->m_count);
	if (!pDD) {
		return S_cas_noMemory;
	}

	//
	// GDD ref count is managed by smart pointer class from here down
	//
	gddStat = pDD->unreference();
	assert (!gddStat);

    size = aitSize[bestExternalType] * pHdr->m_count;
	pData = new char [size];
	if (!pData) {
		return S_cas_noMemory;
	}

	//
	// ok to use the default gddDestructor here because
	// an array of characters was allocated above
	//
	pDestructor = new gddDestructor;
	if (!pDestructor) {
		delete [] pData;
		return S_cas_noMemory;
	}

	//
	// convert the data from the protocol buffer
	// to the allocated area so that they
	// will be allowed to ref the DD
	//
    gddStat = aitConvert (bestExternalType, pData, type, this->ctx.getData(), 
        pHdr->m_count, &this->ctx.getPV()->enumStringTable() );
    if (gddStat<0) { 
        status = S_cas_noConvert;
        delete pDestructor;
        delete [] pData;
    }
    else {
	    //
	    // install allocated area into the DD
	    //
	    pDD->putRef (pData, type, pDestructor);

        //
        // set the status and severity to normal
        //
        pDD->setStat (epicsAlarmNone);
	    pDD->setSevr (epicsSevNone);

        //
        // set the time stamp to the last time that
        // we added bytes to the in buf
        //
        aitTimeStamp gddts = this->lastRecvTS;
        pDD->setTimeStamp (&gddts);

	    //
	    // call the server tool's virtual function
	    //
	    status = this->ctx.getPV()->write(this->ctx, *pDD);
    }
	return status;
}

//
// casStrmClient::read()
//
caStatus casStrmClient::read (smartGDDPointer &pDescRet)
{
	const caHdr	    *pHdr = this->ctx.getMsg();
	caStatus        status;

	pDescRet = createDBRDD (pHdr->m_dataType, pHdr->m_count);
	if ( ! pDescRet ) {
		return S_cas_noMemory;
	}

	//
	// the PV state must not be modified during a transaction
	//
	status = this->ctx.getPV()->beginTransaction();
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
	status = this->ctx.getPV()->read (this->ctx, *pDescRet);

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

	this->ctx.getPV()->endTransaction();

	return status;
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
inline bool caServerI::roomForNewChannel() const
{
	return true;
}

//
// casStrmClient::installChannel()
//
void casStrmClient::installChannel(casChannelI &chan)
{
	this->lock();
	this->getCAS().installItem (chan);
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

//
//  casStrmClient::xSend()
//
outBuf::flushCondition casStrmClient::xSend (char *pBufIn,
                                             bufSizeT nBytesAvailableToSend,
                                             bufSizeT nBytesNeedToBeSent,
                                             bufSizeT &nActualBytes)
{
    outBuf::flushCondition stat;
    bufSizeT nActualBytesDelta;
    bufSizeT totalBytes;

    assert (nBytesAvailableToSend>=nBytesNeedToBeSent);
	
    totalBytes = 0u;
    while ( true ) {
        stat = this->osdSend (&pBufIn[totalBytes],
                              nBytesAvailableToSend-totalBytes, nActualBytesDelta);
        if (stat != outBuf::flushProgress) {
            if (totalBytes>0) {
                nActualBytes = totalBytes;
		        //
		        // !! this time fetch may be slowing things down !!
		        //
		        //this->lastSendTS = epicsTime::getCurrent();
                return outBuf::flushProgress;
            }
            else {
                return stat;
            }
        }
		
        totalBytes += nActualBytesDelta;
		
        if (totalBytes>=nBytesNeedToBeSent) {
		    //
		    // !! this time fetch may be slowing things down !!
		    //
		    //this->lastSendTS = epicsTime::getCurrent();
            nActualBytes = totalBytes;
            return outBuf::flushProgress;
        }
    }
	return outBuf::flushDisconnect; // happy compiler 
}

//
// casStrmClient::xRecv()
//
inBuf::fillCondition casStrmClient::xRecv(char *pBufIn, bufSizeT nBytes,
                                 enum inBuf::fillParameter, bufSizeT &nActualBytes)
{
	inBuf::fillCondition stat;
	
	stat = this->osdRecv (pBufIn, nBytes, nActualBytes);
    //
    // this is used to set the time stamp for write GDD's
    //
    this->lastRecvTS = epicsTime::getCurrent();
	return stat;
}

//
// casStrmClient::getDebugLevel()
//
unsigned casStrmClient::getDebugLevel() const
{
	return this->getCAS().getDebugLevel();
}

