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

#include <stdarg.h>

#include "server.h"
#include "casEventSysIL.h" // inline func for casEventSys
#include "casCtxIL.h" // inline func for casCtx
#include "inBufIL.h" // inline func for inBuf 
#include "outBufIL.h" // inline func for outBuf
#include "casPVIIL.h" // inline func for casPVI 
#include "db_access.h"

static const caHdr nill_msg = {0u,0u,0u,0u,0u,0u};


//
// static declartions for class casClient
//
int casClient::msgHandlersInit;
casClient::pCASMsgHandler casClient::msgHandlers[CA_PROTO_LAST_CMMD+1u];

//
// casClient::casClient()
//
casClient::casClient(caServerI &serverInternal, bufSizeT ioSizeMinIn) : 
    casCoreClient (serverInternal),
    inBuf (MAX_MSG_SIZE, ioSizeMinIn), outBuf (MAX_MSG_SIZE)
{
    //
    // static member init 
    //
    casClient::loadProtoJumpTable();
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

//
// casClient::processMsg ()
//
caStatus casClient::processMsg ()
{
	unsigned    msgsize;
	unsigned    bytesLeft;
	int         status;
	const caHdr *mp;
	const char  *rawMP;

	//
	// process any messages in the in buffer
	//
	status = S_cas_success;

	while ( (bytesLeft = this->inBuf::bytesPresent()) ) {

		/*
		 * incomplete message - return success and
		 * wait for more bytes to arrive
		 */
		if (bytesLeft < sizeof(*mp)) {
			status = S_cas_success;
			break;
		}

        rawMP = this->inBuf::msgPtr();
		this->ctx.setMsg(rawMP);

		//
		// get pointer to msg copy in local byte order
		//
		mp = this->ctx.getMsg();

		msgsize = mp->m_postsize + sizeof(*mp);

		/*
		 * incomplete message - return success and
		 * wait for more bytes to arrive
		 */
		if (msgsize > bytesLeft) { 
			status = S_cas_success;
			break;
		}

		this->ctx.setData((void *)(rawMP+sizeof(*mp)));

		if (this->getCAS().getDebugLevel()> 2u) {
			this->dumpMsg (mp, (void *)(mp+1));
		}

		//
		// Reset the context to the default
		// (guarantees that previous message does not get mixed 
		// up with the current message)
		//
		this->ctx.setChannel (NULL);
		this->ctx.setPV (NULL);

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
        try {
		    status = (this->*casClient::msgHandlers[mp->m_cmmd]) ();
		    if (status) {
			    break;
		    }
        }
        catch (...) {
            this->dumpMsg (mp, this->ctx.getData() );
            epicsPrintf ("Attempt to execute client request failed (C++ exception)\n");
            status = this->sendErr (mp, ECA_INTERNAL, "request resulted in C++ exception");
	        if (status) {
		        break;
	        }
            break;
        }

        this->inBuf::removeMsg (msgsize);
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

    status = this->outBuf::allocMsg (mp->m_postsize, &reply);
	if (status) {
		if (status==S_cas_hugeRequest) {
			status = sendErr(mp, ECA_TOLARGE, NULL);
		}
		return status;
	}
	*reply = *mp;
	memcpy((char *) (reply+1), (char *) dp, mp->m_postsize);
    this->outBuf::commitMsg();

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
	va_list args;
	casChannelI *pciu;
	unsigned size;
	caHdr *reply;
	char *pMsgString;
	int status;
	char msgBuf[1024]; /* allocate plenty of space for the message string */

	if (pformat) {
		va_start (args, pformat);
		status = vsprintf (msgBuf, pformat, args);
		if (status<0) {
			errPrintf (S_cas_internal, __FILE__, __LINE__,
				"bad sendErr(%s)", pformat);
			size = 0u;
		}
		else {
			size = 1u + (unsigned) status;
		}
	}
	else {
		size = 0u;
	}

	/*
	 * allocate plenty of space for a sprintf() buffer
	 */
    status = this->outBuf::allocMsg (size+sizeof(caHdr), &reply);
	if (status) {
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
		 * Verify the channel
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
	 * (in network byte order)
	 */
	reply[1].m_postsize = htons (curp->m_postsize);
	reply[1].m_cmmd = htons (curp->m_cmmd);
	reply[1].m_dataType = htons (curp->m_dataType);
	reply[1].m_count = htons (curp->m_count);
	reply[1].m_cid = curp->m_cid;
	reply[1].m_available = curp->m_available;

	/*
	 * add their optional context string into the protocol
	 */
	if (size>0u) {
		pMsgString = (char *) (reply+2u);
		strncpy (pMsgString, msgBuf, size-1u);
		pMsgString[size-1u] = '\0';
	}

	reply->m_postsize = size + sizeof(caHdr);

    this->outBuf::commitMsg();

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
const int cacStatus,
const char	*pFileName,
const unsigned	lineno,
const unsigned id
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
			cacStatus, 
			"Bad Resource ID=%u detected at %s.%d",
			id,
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
		strncpy(pPVName, pciu->getPVI().getName(), sizeof(pPVName));
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
		mp->m_dataType,
		mp->m_count,
		mp->m_postsize,
		mp->m_available);

  	if (mp->m_cmmd==CA_PROTO_WRITE && mp->m_dataType==DBR_STRING && dp) {
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
