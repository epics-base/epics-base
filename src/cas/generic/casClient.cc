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
bool casClient::msgHandlersInit;
casClient::pCASMsgHandler casClient::msgHandlers[CA_PROTO_LAST_CMMD+1u];

//
// casClient::casClient()
//
casClient::casClient ( caServerI & serverInternal, 
         clientBufMemoryManager & mgrIn, bufSizeT ioSizeMinIn ) : 
    casCoreClient ( serverInternal ), 
    in ( *this, mgrIn, ioSizeMinIn ), 
    out ( *this, mgrIn ),
	minor_version_number ( 0 ), 
    incommingBytesToDrain ( 0 )
{
    //
    // static member init 
    //
    casClient::loadProtoJumpTable();
}

//
// find the channel associated with a resource id
//
casChannelI *casClient::resIdToChannel(const caResId &idIn)
{
	casChannelI *pChan;

	//
	// look up the id in a hash table
	//
	pChan = this->ctx.getServer()->resIdToChannel(idIn);

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
	if ( casClient::msgHandlersInit ) {
		return;
	}

	//
	// Request Protocol Jump Table
	// (use of & here is more portable)
	//
	casClient::msgHandlers[CA_PROTO_VERSION] = 
			&casClient::versionAction;
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

	casClient::msgHandlersInit = true;
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
void casClient::show (unsigned level) const
{
	printf ( "casClient at %p\n", 
        static_cast <const void *> ( this ) );
	this->casCoreClient::show (level);
	this->in.show (level);
	this->out.show (level);
}

//
// casClient::processMsg ()
//
caStatus casClient::processMsg ()
{
	int status = S_cas_success;

    try {
        // drain message that does not fit
        if ( this->incommingBytesToDrain ) {
            unsigned bytesLeft = this->in.bytesPresent();
            if ( bytesLeft  < this->incommingBytesToDrain ) {
                this->in.removeMsg ( bytesLeft );
                this->incommingBytesToDrain -= bytesLeft;
                return S_cas_success;
            }
            else {
                this->in.removeMsg ( this->incommingBytesToDrain );
                this->incommingBytesToDrain = 0u;
            }
        }

	    //
	    // process any messages in the in buffer
	    //

	    unsigned bytesLeft;
	    while ( ( bytesLeft = this->in.bytesPresent() ) ) {
            caHdrLargeArray msgTmp;
            unsigned msgSize;
            ca_uint32_t hdrSize;
            char * rawMP;
            {
	            //
	            // copy as raw bytes in order to avoid
	            // alignment problems
	            //
                caHdr smallHdr;
                if ( bytesLeft < sizeof ( smallHdr ) ) {
                    break;
                }

                rawMP = this->in.msgPtr ();
	            memcpy ( & smallHdr, rawMP, sizeof ( smallHdr ) );

                ca_uint32_t payloadSize = epicsNTOH16 ( smallHdr.m_postsize );
                ca_uint32_t nElem = epicsNTOH16 ( smallHdr.m_count );
                if ( payloadSize != 0xffff && nElem != 0xffff ) {
                    hdrSize = sizeof ( smallHdr );
                }
                else {
                    ca_uint32_t LWA[2];
                    hdrSize = sizeof ( smallHdr ) + sizeof ( LWA );
                    if ( bytesLeft < hdrSize ) {
                        break;
                    }
                    //
                    // copy as raw bytes in order to avoid
                    // alignment problems
                    //
                    memcpy ( LWA, rawMP + sizeof ( caHdr ), sizeof( LWA ) );
                    payloadSize = epicsNTOH32 ( LWA[0] );
                    nElem = epicsNTOH32 ( LWA[1] );
                }

                msgTmp.m_cmmd = epicsNTOH16 ( smallHdr.m_cmmd );
                msgTmp.m_postsize = payloadSize;
                msgTmp.m_dataType = epicsNTOH16 ( smallHdr.m_dataType );
                msgTmp.m_count = nElem;
                msgTmp.m_cid = epicsNTOH32 ( smallHdr.m_cid );
                msgTmp.m_available = epicsNTOH32 ( smallHdr.m_available );


                msgSize = hdrSize + payloadSize;
                if ( bytesLeft < msgSize ) {
                    if ( msgSize > this->in.bufferSize() ) {
                        this->in.expandBuffer ();
                        // msg to large - set up message drain
                        if ( msgSize > this->in.bufferSize() ) {
                            this->dumpMsg ( & msgTmp, 0, 
                                "The client requested transfer is greater than available " 
                                "memory in server or EPICS_CA_MAX_ARRAY_BYTES\n" );
                            status = this->sendErr ( & msgTmp, ECA_TOLARGE, 
                                "request didnt fit within the CA server's message buffer" );
                            this->in.removeMsg ( bytesLeft );
                            this->incommingBytesToDrain = msgSize - bytesLeft;
                        }
                    }
                    break;
                }

                this->ctx.setMsg ( msgTmp, rawMP + hdrSize );

		        if ( this->getCAS().getDebugLevel() > 2u ) {
			        this->dumpMsg ( & msgTmp, rawMP + hdrSize, 0 );
		        }

            }

		    //
		    // Reset the context to the default
		    // (guarantees that previous message does not get mixed 
		    // up with the current message)
		    //
		    this->ctx.setChannel ( NULL );
		    this->ctx.setPV ( NULL );

		    //
		    // Call protocol stub
		    //
            pCASMsgHandler pHandler;
		    if ( msgTmp.m_cmmd < NELEMENTS ( casClient::msgHandlers ) ) {
                pHandler = this->casClient::msgHandlers[msgTmp.m_cmmd];
		    }
            else {
                pHandler = & casClient::uknownMessageAction;
            }
		    status = ( this->*pHandler ) ();
		    if ( status ) {
			    break;
		    }

            this->in.removeMsg ( msgSize );
	    }
    }
    catch ( std::bad_alloc & ) {
        status = this->sendErr ( 
            this->ctx.getMsg(), ECA_ALLOCMEM, 
            "inablility to allocate memory in "
            "the server disconnected client" );
        status = S_cas_noMemory;
    }
    catch ( std::exception & except ) {
		status = this->sendErr ( 
            this->ctx.getMsg(), ECA_INTERNAL, 
            "C++ exception \"%s\" in server "
            "diconnected client",
            except.what () );
        status = S_cas_internal;
    }
    catch (...) {
		status = this->sendErr ( 
            this->ctx.getMsg(), ECA_INTERNAL, 
            "unexpected C++ exception in server "
            "diconnected client" );
        status = S_cas_internal;
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
	const caHdrLargeArray * mp = this->ctx.getMsg();
	const void * dp = this->ctx.getData();
    void * pPayloadOut;

    epicsGuard < epicsMutex > guard ( this->mutex );
    caStatus status = this->out.copyInHeader ( mp->m_cmmd, mp->m_postsize, 
        mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available,
        & pPayloadOut );
    if ( ! status ) {
        memcpy ( pPayloadOut, dp, mp->m_postsize );
        this->out.commitMsg ();
    }
	return S_cas_success;
}

/*
 * casClient::versionAction()
 */
caStatus casClient::versionAction ()
{
	return S_cas_success;
}

// send minor protocol revision to the client
void casClient::sendVersion ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    caStatus status = this->out.copyInHeader ( CA_PROTO_VERSION, 0, 
        0, CA_MINOR_PROTOCOL_REVISION, 0, 0, 0 );
    if ( ! status ) {
        this->out.commitMsg ();
    }
}
//
//	casClient::sendErr()
//
caStatus casClient::sendErr ( const caHdrLargeArray *curp, const int reportedStatus,
    const char	*pformat, ... )
{
	unsigned stringSize;
	char msgBuf[1024]; /* allocate plenty of space for the message string */
	if ( pformat ) {
	    va_list args;
		va_start ( args, pformat );
		int status = vsprintf (msgBuf, pformat, args);
		if ( status < 0 ) {
			errPrintf (S_cas_internal, __FILE__, __LINE__,
				"bad sendErr(%s)", pformat);
			stringSize = 0u;
		}
		else {
			stringSize = 1u + (unsigned) status;
		}
	}
	else {
		stringSize = 0u;
	}

    unsigned hdrSize = sizeof ( caHdr );
    if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) && 
            CA_V49( this->minor_version_number ) ) {
        hdrSize += 2 * sizeof ( ca_uint32_t );
    }

    ca_uint32_t cid = 0u;
	switch ( curp->m_cmmd ) {
	case CA_PROTO_SEARCH:
		cid = curp->m_cid;
		break;

	case CA_PROTO_EVENT_ADD:
	case CA_PROTO_EVENT_CANCEL:
	case CA_PROTO_READ:
	case CA_PROTO_READ_NOTIFY:
	case CA_PROTO_WRITE:
	case CA_PROTO_WRITE_NOTIFY:
        {
		    /*
		    * Verify the channel
		    */
            casChannelI * pciu = this->resIdToChannel ( curp->m_cid );
		    if ( pciu ) {
			    cid = pciu->getCID();
		    }
		    else{
			    cid = ~0u;
		    }
		    break;
        }

	case CA_PROTO_EVENTS_ON:
	case CA_PROTO_EVENTS_OFF:
	case CA_PROTO_READ_SYNC:
	case CA_PROTO_SNAPSHOT:
	default:
		cid = (caResId) ~0UL;
		break;
	}

    caHdr * pReqOut;
    epicsGuard < epicsMutex > guard ( this->mutex );
    caStatus status = this->out.copyInHeader ( CA_PROTO_ERROR, 
        hdrSize + stringSize, 0, 0, cid, reportedStatus,
        reinterpret_cast <void **> ( & pReqOut ) );
    if ( ! status ) {
        char * pMsgString;

        /*
         * copy back the request protocol
         * (in network byte order)
         */
        if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) && 
                CA_V49( this->minor_version_number ) ) {
            ca_uint32_t *pLW = ( ca_uint32_t * ) ( pReqOut + 1 );
            pReqOut->m_cmmd = htons ( curp->m_cmmd );
            pReqOut->m_postsize = htons ( 0xffff );
            pReqOut->m_dataType = htons ( curp->m_dataType );
            pReqOut->m_count = htons ( 0u );
            pReqOut->m_cid = htonl ( curp->m_cid );
            pReqOut->m_available = htonl ( curp->m_available );
            pLW[0] = htonl ( curp->m_postsize );
            pLW[1] = htonl ( curp->m_count );
            pMsgString = ( char * ) ( pLW + 2 );
        }
        else {
            pReqOut->m_cmmd = htons (curp->m_cmmd);
            pReqOut->m_postsize = htons ( ( (ca_uint16_t) curp->m_postsize ) );
            pReqOut->m_dataType = htons (curp->m_dataType);
            pReqOut->m_count = htons ( ( (ca_uint16_t) curp->m_count ) );
            pReqOut->m_cid = htonl (curp->m_cid);
            pReqOut->m_available = htonl (curp->m_available);
            pMsgString = ( char * ) ( pReqOut + 1 );
         }

        /*
         * add their context string into the protocol
         */
        memcpy ( pMsgString, msgBuf, stringSize );

        this->out.commitMsg ();
    }

	return S_cas_success;
}

/*
 * casClient::sendErrWithEpicsStatus()
 *
 * same as sendErr() except that we convert epicsStatus
 * to a string and send that additional detail
 */
caStatus casClient::sendErrWithEpicsStatus ( const caHdrLargeArray * pMsg, 
	caStatus epicsStatus, caStatus clientStatus )
{
	char buf[0x1ff];
	errSymLookup ( epicsStatus, buf, sizeof(buf) );
	return this->sendErr ( pMsg, clientStatus, buf );
}

/*
 * casClient::logBadIdWithFileAndLineno()
 */
caStatus casClient::logBadIdWithFileAndLineno ( const caHdrLargeArray * mp,
    const void	* dp, const int cacStatus, const char * pFileName, 
    const unsigned lineno, const unsigned idIn
)
{
	int status;

	if ( pFileName) {
	    this-> dumpMsg ( mp, dp, 
            "bad resource id in \"%s\" at line %d\n",
			pFileName, lineno );
	}
    else {
	    this->dumpMsg ( mp, dp, 
            "bad resource id\n" );
    }

	status = this->sendErr (
			mp, cacStatus, "Bad Resource ID=%u detected at %s.%d",
			idIn, pFileName, lineno);

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
void casClient::dumpMsg ( const caHdrLargeArray *mp, 
                         const void *dp, const char *pFormat, ... )
{
    va_list theArgs;
    if ( pFormat ) {
        va_start ( theArgs, pFormat );
        errlogPrintf ( "CAS: " );
        errlogVprintf ( pFormat, theArgs );
        va_end ( theArgs );
    }

    char pName[64u];
    this->hostName (pName, sizeof (pName));

    casChannelI * pciu = this->resIdToChannel(mp->m_cid);

    char pPVName[64u];
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

    char pUserName[32];
    this->userName ( pUserName, sizeof ( pUserName) );
    char pHostName[32];
    this->hostName ( pHostName, sizeof ( pHostName) );

    fprintf ( stderr, 
"CAS Request: %s on %s at %s: cmd=%d cid=%s typ=%d cnt=%d psz=%d avail=%x\n",
        pUserName,
        pHostName,
        pName,
        mp->m_cmmd,
        pPVName,
        mp->m_dataType,
        mp->m_count,
        mp->m_postsize,
        mp->m_available);

    if ( mp->m_cmmd == CA_PROTO_WRITE && mp->m_dataType == DBR_STRING && dp ) {
        errlogPrintf("CAS: The string written: %s \n", (char *)dp);
    }
}
