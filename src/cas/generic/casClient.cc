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
 * Revision 1.8  1997/06/30 22:54:25  jhill
 * use %p with pointers
 *
 * Revision 1.7  1997/04/10 19:34:01  jhill
 * API changes
 *
 * Revision 1.6  1996/12/11 00:58:35  jhill
 * better diagnostic
 *
 * Revision 1.5  1996/11/02 00:54:04  jhill
 * many improvements
 *
 * Revision 1.4  1996/09/04 20:19:02  jhill
 * include db_access.h
 *
 * Revision 1.3  1996/08/13 22:56:13  jhill
 * added init for mutex class
 *
 * Revision 1.2  1996/07/09 22:54:31  jhill
 * store msg copy in the ctx
 *
 * Revision 1.1.1.1  1996/06/20 00:28:14  jhill
 * ca server installation
 *
 *
 */

#include <stdarg.h>

#include "server.h"
#include "casEventSysIL.h" // inline func for casEventSys
#include "casCtxIL.h" // inline func for casCtx
#include "inBufIL.h" // inline func for inBuf 
#include "casPVIIL.h" // inline func for casPVI 
#include "db_access.h"

static const caHdr nill_msg = {0u,0u,0u,0u,0u,0u};


//
// static declartions for class casClient
//
int casClient::msgHandlersInit;
pCASMsgHandler casClient::msgHandlers[CA_PROTO_LAST_CMMD+1u];



//
// casClient::casClient()
//
casClient::casClient(caServerI &serverInternal, inBuf &inBufIn, outBuf &outBufIn) : 
		casCoreClient(serverInternal), 
		inBufRef(inBufIn), outBufRef(outBufIn)
{
	//
	// static member init 
	//
	casClient::loadProtoJumpTable();
}


//
// casClient::init()
//
caStatus casClient::init() 
{
	unsigned serverDebugLevel;
	caStatus status;

	//
	// call base class initializers
	//
	status = this->casCoreClient::init();
	if (status) {
		return status;
	}

	serverDebugLevel = this->ctx.getServer()->getDebugLevel();
        if (serverDebugLevel>0u) {
		char pName[64u];

		this->clientHostName (pName, sizeof (pName));
                ca_printf("CAS: created a new client for %s\n", pName);
        }

	return S_cas_success;
}

//
// find the channel associated with a resource id
//
casChannelI *casClient::resIdToChannel(const caResId &id)
{
	casChannelI *pChan;

	//
	// look up the id in a hash table
	//
	pChan = this->ctx.getServer()->resIdToChannel(id);

	//
	// update the context
	//
	this->ctx.setChannel(pChan);
	if (!pChan) {
		return NULL;
	}

	//
	// If the channel isnt attached to this client then
	// something has gone wrong
	//
	if (&pChan->getClient()!=this) {
		return NULL;
	}

	//
	// update the context
	//
	this->ctx.setPV(&pChan->getPVI());
	return pChan;
}


//
// casClient::loadProtoJumpTable()
//
void casClient::loadProtoJumpTable()
{
	//
	// Load the static protocol handler tables
	//
	if (casClient::msgHandlersInit) {
		return;
	}

	//
	// Request Protocol Jump Table
	// (use of & here is more portable)
	//
	casClient::msgHandlers[CA_PROTO_NOOP] = 
			&casClient::noopAction;
	casClient::msgHandlers[CA_PROTO_EVENT_ADD] = 
			&casClient::eventAddAction;
	casClient::msgHandlers[CA_PROTO_EVENT_CANCEL] = 
			&casClient::eventCancelAction;
	casClient::msgHandlers[CA_PROTO_READ] = 
			&casClient::readAction;
	casClient::msgHandlers[CA_PROTO_WRITE] = 	
			&casClient::writeAction;
	casClient::msgHandlers[CA_PROTO_SNAPSHOT] = 
			&casClient::uknownMessageAction;
	casClient::msgHandlers[CA_PROTO_SEARCH] = 
			&casClient::searchAction;
	casClient::msgHandlers[CA_PROTO_BUILD] = 
			&casClient::ignoreMsgAction;
	casClient::msgHandlers[CA_PROTO_EVENTS_OFF] = 
			&casClient::eventsOffAction;
	casClient::msgHandlers[CA_PROTO_EVENTS_ON] = 
			&casClient::eventsOnAction;
	casClient::msgHandlers[CA_PROTO_READ_SYNC] = 
			&casClient::readSyncAction;
	casClient::msgHandlers[CA_PROTO_ERROR] = 
			&casClient::uknownMessageAction;
	casClient::msgHandlers[CA_PROTO_CLEAR_CHANNEL] = 
			&casClient::clearChannelAction;
	casClient::msgHandlers[CA_PROTO_RSRV_IS_UP] = 
			&casClient::uknownMessageAction;
	casClient::msgHandlers[CA_PROTO_NOT_FOUND] = 
			&casClient::uknownMessageAction;
	casClient::msgHandlers[CA_PROTO_READ_NOTIFY] = 
			&casClient::readNotifyAction;
	casClient::msgHandlers[CA_PROTO_READ_BUILD] = 
			&casClient::ignoreMsgAction;
	casClient::msgHandlers[REPEATER_CONFIRM] = 
			&casClient::uknownMessageAction;
	casClient::msgHandlers[CA_PROTO_CLAIM_CIU] = 
			&casClient::claimChannelAction;
	casClient::msgHandlers[CA_PROTO_WRITE_NOTIFY] = 
			&casClient::writeNotifyAction;
	casClient::msgHandlers[CA_PROTO_CLIENT_NAME] = 
			&casClient::clientNameAction;
	casClient::msgHandlers[CA_PROTO_HOST_NAME] = 
			&casClient::hostNameAction;
	casClient::msgHandlers[CA_PROTO_ACCESS_RIGHTS] = 
			&casClient::uknownMessageAction;
	casClient::msgHandlers[CA_PROTO_ECHO] = 
			&casClient::echoAction;
	casClient::msgHandlers[REPEATER_REGISTER] = 
			&casClient::uknownMessageAction;
	casClient::msgHandlers[CA_PROTO_CLAIM_CIU_FAILED] = 
			&casClient::uknownMessageAction;

	casClient::msgHandlersInit = TRUE;
}


//
// casClient::~casClient ()
//
casClient::~casClient ()
{
}


//
// casClient::show()
//
void casClient::show(unsigned level) const
{
	printf ("casClient at %p\n", this);
	this->casCoreClient::show(level);
}


/*
 * casClient::processMsg()
 */
caStatus casClient::processMsg()
{
	unsigned	msgsize;
	unsigned	bytesLeft;
	int            	status;
	const caHdr 	*mp;
	const char	*rawMP;

	//
	// process any messages in the in buffer
	//
	status = S_cas_success;

	bytesLeft = this->inBufRef.bytesPresent();
	while (bytesLeft) {

		if (bytesLeft < sizeof(*mp)) {
			status = S_cas_partialMessage;
			break;
		}

		rawMP = this->inBufRef.msgPtr();
		this->ctx.setMsg(rawMP);

		//
		// get pointer to msg copy in local byte order
		//
		mp = this->ctx.getMsg();

		msgsize = mp->m_postsize + sizeof(*mp);

		if (msgsize > bytesLeft) { 
			status = S_cas_partialMessage;
			break;
		}

		this->ctx.setData((void *)(rawMP+sizeof(*mp)));

		if (this->getCAS().getDebugLevel()> 2u) {
			this->dumpMsg(mp, (void *)(mp+1));
		}

		//
		// Reset the context to the default
		// (guarantees that previous message does not get mixed 
		// up with the current message)
		//
		this->ctx.setChannel(NULL);
		this->ctx.setPV(NULL);

		//
		// Check for bad protocol element
		//
		if (mp->m_cmmd >= NELEMENTS(casClient::msgHandlers)){
			status = this->uknownMessageAction ();
			break;
		}

		//
		// Call protocol stub 
		//
		status = (this->*casClient::msgHandlers[mp->m_cmmd]) ();
		if (status) {
			break;
		}

		this->inBufRef.removeMsg(msgsize);
		bytesLeft = this->inBufRef.bytesPresent();
	}

	return status;
}


/*
 * casClient::ignoreMsgAction()
 */
caStatus casClient::ignoreMsgAction ()
{
	return S_cas_success;
}


/*
 * casClient::uknownMessageAction()
 */
caStatus casClient::uknownMessageAction ()
{
	const caHdr *mp = this->ctx.getMsg();

	ca_printf ("CAS: bad message type=%u\n", mp->m_cmmd);
	this->dumpMsg (mp, this->ctx.getData() );

	/* 
	 *	most clients dont recover from this
	 */
	this->sendErr (mp, ECA_INTERNAL, "Invalid Msg Type");

	/*
	 * returning S_cas_internal here disconnects
	 * the client with the bad message
	 */
	return S_cas_internal;
}

//
// what gets called if the derived class does not supply a
// message handler for the message type (and it isnt a generic
// message)
// 
caStatus casClient::eventAddAction ()
{return this->uknownMessageAction ();}

caStatus casClient::eventCancelAction ()
{return this->uknownMessageAction ();}

caStatus casClient::readAction ()
{return this->uknownMessageAction ();}

caStatus casClient::readNotifyAction ()
{return this->uknownMessageAction ();}

caStatus casClient::writeAction ()
{return this->uknownMessageAction ();}

caStatus casClient::searchAction ()
{return this->uknownMessageAction ();}

caStatus casClient::eventsOffAction ()
{return this->uknownMessageAction ();}

caStatus casClient::eventsOnAction ()
{return this->uknownMessageAction ();}

caStatus casClient::readSyncAction ()
{return this->uknownMessageAction ();}

caStatus casClient::clearChannelAction ()
{return this->uknownMessageAction ();}

caStatus casClient::claimChannelAction ()
{return this->uknownMessageAction ();}

caStatus casClient::writeNotifyAction ()
{return this->uknownMessageAction ();}

caStatus casClient::clientNameAction ()
{return this->uknownMessageAction ();}

caStatus casClient::hostNameAction ()
{return this->uknownMessageAction ();}


//
// echoAction()
//
caStatus casClient::echoAction ()
{
	const caHdr 	*mp = this->ctx.getMsg();
	void 		*dp = this->ctx.getData();
	int		status;
	caHdr 		*reply; 

	status = this->outBufRef.allocMsg(mp->m_postsize, &reply);
	if (status) {
		if (status==S_cas_hugeRequest) {
			status = sendErr(mp, ECA_TOLARGE, NULL);
		}
		return status;
	}
	*reply = *mp;
	memcpy((char *) (reply+1), (char *) dp, mp->m_postsize);
	this->outBufRef.commitMsg();

	return S_cas_success;
}


/*
 * casClient::noopAction()
 */
caStatus casClient::noopAction ()
{
	return S_cas_success;
}




//
//	casClient::sendErr()
//
caStatus casClient::sendErr(
const caHdr  	*curp,
const int	reportedStatus,
const char	*pformat,
		...
)
{
	va_list		args;
        casChannelI	*pciu;
	int        	size;
	caHdr 		*reply;
	char		*pMsgString;
	int		status;

	va_start(args, pformat);

	/*
	 * allocate plenty of space for a sprintf() buffer
	 */
	status = this->outBufRef.allocMsg(1024u, &reply);
	if(status){
		return status;
	}

	reply[0] = nill_msg;
	reply[0].m_cmmd = CA_PROTO_ERROR;
	reply[0].m_available = reportedStatus;

	switch (curp->m_cmmd) {
	case CA_PROTO_SEARCH:
		reply->m_cid = curp->m_cid;
		break;

	case CA_PROTO_EVENT_ADD:
	case CA_PROTO_EVENT_CANCEL:
	case CA_PROTO_READ:
	case CA_PROTO_READ_NOTIFY:
	case CA_PROTO_WRITE:
	case CA_PROTO_WRITE_NOTIFY:
	        /*
		 *
		 * Verify the channel
		 *
		 */
		pciu = this->resIdToChannel(curp->m_cid);
		if(pciu){
			reply->m_cid = pciu->getCID();
		}
		else{
			reply->m_cid = ~0u;
		}
		break;

	case CA_PROTO_EVENTS_ON:
	case CA_PROTO_EVENTS_OFF:
	case CA_PROTO_READ_SYNC:
	case CA_PROTO_SNAPSHOT:
	default:
		reply->m_cid = (caResId) ~0UL;
		break;
	}

	/*
	 * copy back the request protocol
	 */
	reply[1] = *curp;

	/*
	 * add their optional context string into the protocol
	 */
	if (pformat) {
		pMsgString = (char *) (reply+2);
		vsprintf(pMsgString, pformat, args);
		size = strlen(pMsgString)+1;
	}
	else {
		size = 0;
	}
	size += sizeof(*curp);
	reply->m_postsize = size;

	this->outBufRef.commitMsg();

	return S_cas_success;
}


/*
 * casClient::sendErrWithEpicsStatus()
 *
 * same as sendErr() except that we convert epicsStatus
 * to a string and send that additional detail
 */
caStatus casClient::sendErrWithEpicsStatus(const caHdr *pMsg, 
	caStatus epicsStatus, caStatus clientStatus)
{
	long	status;
	char	buf[0x1ff];

	status = errSymFind(epicsStatus, buf);
	if (status) {
		sprintf(buf, "UKN error code = 0X%u\n",
			epicsStatus);
	}
	
	return this->sendErr(pMsg, clientStatus, buf);
}


/*
 * casClient::logBadIdWithFileAndLineno()
 */
caStatus casClient::logBadIdWithFileAndLineno(
const caHdr 	*mp,
const void	*dp,
const char	*pFileName,
const unsigned	lineno
)
{
	int status;

	if (pFileName) {
		ca_printf("Bad client request detected in \"%s\" at line %d\n",
			pFileName, lineno);
	}
	this->dumpMsg(mp, dp);

	status = this->sendErr(
			mp, 
			ECA_INTERNAL, 
			"Bad Resource ID at %s.%d",
			pFileName,
			lineno);

	return status;
}


//
//	casClient::dumpMsg()
//
//	Debug aid - print the header part of a message.
//
//	dp arg allowed to be null
//
//
void casClient::dumpMsg(const caHdr *mp, const void *dp)
{
        casChannelI	*pciu;
	char		pName[64u];
	char		pPVName[64u];

	this->clientHostName (pName, sizeof (pName));

	pciu = this->resIdToChannel(mp->m_cid);

	if (pciu) {
		strncpy(pPVName, pciu->getPVI()->getName(), sizeof(pPVName));
		if (&pciu->getClient()!=this) {
			strncat(pPVName, "!Bad Client!", sizeof(pPVName));
		}
		pPVName[sizeof(pPVName)-1]='\0';
	}
	else {
		sprintf(pPVName,"%u", mp->m_cid);
	}
	ca_printf(	
"CAS %s on %s at %s: cmd=%d cid=%s typ=%d cnt=%d psz=%d avail=%x\n",
		this->userName(),
		this->hostName(),
		pName,
		mp->m_cmmd,
		pPVName,
		mp->m_type,
		mp->m_count,
		mp->m_postsize,
		mp->m_available);

  	if (mp->m_cmmd==CA_PROTO_WRITE && mp->m_type==DBR_STRING && dp) {
    		ca_printf("CAS: The string written: %s \n", (char *)dp);
	}
}

//
// 
//
const char *casClient::hostName() const 
{
        return "?";
}
const char *casClient::userName() const 
{
        return "?";
}

