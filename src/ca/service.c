/************************************************************************/
/*									*/
/*	        	      L O S  A L A M O S			*/
/*		        Los Alamos National Laboratory			*/
/*		         Los Alamos, New Mexico 87545			*/
/*									*/
/*	Copyright, 1986, The Regents of the University of California.	*/
/*									*/
/*									*/
/*	History								*/
/*	-------								*/
/*									*/
/*	Date	Person	Comments					*/
/*	----	------	--------					*/
/*	8/87	joh	Init Release					*/
/*	1/90	joh	Made cmmd for errors which			*/
/*					eliminated the status field	*/
/*	030791	joh	Fixed problem where pend count was decremented	*/
/*			prior to connecting chan (setting the type).	*/
/*	041591	joh	added call to channel exsits routine		*/
/*	060491  joh	fixed structure pass for SPARC cc		*/
/*	060691	joh	reworked to remove 4 byte count before each	*/
/*			macro message					*/
/*	071291	joh	now claiming channel in use block over TCP so	*/
/*			problems with duplicate port assigned to	*/
/*			client after reboot go away			*/
/*	072391	joh	added event locking for vxWorks			*/
/*    	100391  joh     added missing ntohs() for the VAX               */
/*	111991	joh	converted MACRO in iocinf.h to cac_io_done()	*/
/*			here						*/
/*	022692	joh	moved modify of chan->state inside lock to	*/
/*			provide sync necissary for new use of this 	*/
/*			field.						*/
/*	031692	joh	When bad cmd in message detected disconnect	*/
/*			instead of terminating the client		*/
/*	041692	joh	set state to cs_conn before caling 		*/
/* 			ca_request_event() so their channel 		*/
/*			connect tests wont fail				*/
/*	042892	joh	No longer checking the status from free() 	*/
/*			since it varies from os to os			*/
/*	040592	joh	took out extra cac_send_msg() calls		*/
/*	072792	joh	better messages					*/
/*	110592	joh	removed ptr dereference from test for valid	*/
/*			monitor handler function			*/
/*	111792	joh	reset retry delay for cast iiu when host	*/
/*			indicates the channel's address has changed	*/
/*	120992	joh	converted to dll list routines			*/
/*	122192	joh	now decrements the outstanding ack count	*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	channel access service routines				*/
/*	File:	atcs:[ca]service.c					*/
/*	Environment: VMS, UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	channel access service routines					*/
/*									*/
/*									*/
/*	Special comments						*/
/*	------- --------						*/
/*									*/
/************************************************************************/
/*_end									*/

static char *sccsId = "@(#) $Id$"; 

#include 	"iocinf.h"
#include 	"net_convert.h"

LOCAL int cacMsg(
struct ioc_in_use 	*piiu,
const struct in_addr  	*pnet_addr
);

LOCAL void perform_claim_channel(
IIU			*piiu,
const struct in_addr    *pnet_addr
);

LOCAL void verifyChanAndDisconnect(IIU *piiu, enum channel_state state);

#ifdef CONVERSION_REQUIRED 
extern CACVRTFUNC *cac_dbr_cvrt[];
#endif /*CONVERSION_REQUIRED*/


/*
 * post_msg()
 * 
 * 
 * LOCK should be applied when calling this routine
 * 
 */
int post_msg(
struct ioc_in_use 	*piiu,
const struct in_addr  	*pnet_addr,
char			*pInBuf,
unsigned long		blockSize
)
{
	int		status;
	unsigned long 	size;

	while (blockSize) {

		/*
		 * fetch a complete message header
		 */
		if(piiu->curMsgBytes < sizeof(piiu->curMsg)){
			char  *pHdr;

			size = sizeof (piiu->curMsg) - piiu->curMsgBytes;
			size = min (size, blockSize);
			
			pHdr = (char *) &piiu->curMsg;
			memcpy(	pHdr + piiu->curMsgBytes, 
				pInBuf, 
				size);
			
			piiu->curMsgBytes += size;
			if(piiu->curMsgBytes < sizeof(piiu->curMsg)){
#if 0 
				printf ("waiting for %d msg hdr bytes\n", 
					sizeof(piiu->curMsg)-piiu->curMsgBytes);
#endif
				return OK;
			}

			pInBuf += size;
			blockSize -= size;
		
			/* 
			 * fix endian of bytes 
			 */
			piiu->curMsg.m_postsize = 
				ntohs(piiu->curMsg.m_postsize);
			piiu->curMsg.m_cmmd = ntohs(piiu->curMsg.m_cmmd);
			piiu->curMsg.m_type = ntohs(piiu->curMsg.m_type);
			piiu->curMsg.m_count = ntohs(piiu->curMsg.m_count);
#if 0
			ca_printf("%s Cmd=%3d Type=%3d Count=%4d Size=%4d",
				piiu->host_name_str,
				piiu->curMsg.m_cmmd,
				piiu->curMsg.m_type,
				piiu->curMsg.m_count,
				piiu->curMsg.m_postsize);
			ca_printf(" Avail=%8x Cid=%6d\n",
				piiu->curMsg.m_available,
				piiu->curMsg.m_cid);
#endif
		}


		/*
		 * dont allow huge msg body until
		 * the server supports it
		 */
		if(piiu->curMsg.m_postsize>(unsigned)MAX_TCP){
			piiu->curMsgBytes = 0;
			piiu->curDataBytes = 0;
			return ERROR;
		}

		/*
		 * make sure we have a large enough message body cache
		 */
		if (piiu->curMsg.m_postsize>piiu->curDataMax) {
			void *pCurData;
			size_t size;

			/* 
			 * scalar DBR_STRING is sometimes clipped to the
			 * actual string size so make sure this cache is
			 * as large as one DBR_STRING so they will
			 * not page fault if they read MAX_STRING_SIZE
			 * bytes (instead of the actual string size).
			 */
			size = max(piiu->curMsg.m_postsize,MAX_STRING_SIZE);
			pCurData = (void *) calloc(1u, size);
			if(!pCurData){
				return ERROR;
			}
			if(piiu->pCurData){
				free(piiu->pCurData);
			}
			piiu->pCurData = pCurData;
			piiu->curDataMax = 
				piiu->curMsg.m_postsize;
		}

		/*
 		 * Fetch a complete message body
		 * (allows for arrays larger than than the
		 * ring buffer size)
		 */
		if(piiu->curMsg.m_postsize>piiu->curDataBytes){
			char *pBdy;

			size = piiu->curMsg.m_postsize - piiu->curDataBytes; 
			size = min(size, blockSize);
			pBdy = piiu->pCurData;
			memcpy(	pBdy+piiu->curDataBytes, 
				pInBuf, 
				size);
			piiu->curDataBytes += size;
			if(piiu->curDataBytes < piiu->curMsg.m_postsize){
#if 0
			printf ("waiting for %d msg bdy bytes\n", 
				piiu->curMsg.m_postsize-piiu->curDataBytes);
#endif
				return OK;
			}
			pInBuf += size;
			blockSize -= size;
		}	


		/*
 		 * execute the response message
		 */
		status = cacMsg(piiu, pnet_addr);
		piiu->curMsgBytes = 0;
		piiu->curDataBytes = 0;
		if(status != OK){
			return ERROR;
		}
	}
	return OK;
}


/*
 * cacMsg()
 */
LOCAL int cacMsg(
struct ioc_in_use 	*piiu,
const struct in_addr  	*pnet_addr
)
{
	miu	monix;
	ciu	pChan;

	switch (piiu->curMsg.m_cmmd) {

	case CA_PROTO_NOOP:
		break;

	case CA_PROTO_ECHO:
		piiu->echoPending = FALSE;
		break;

	case CA_PROTO_WRITE_NOTIFY:
	{
		struct event_handler_args args;

		/*
		 * run the user's event handler
		 */
		LOCK;
		monix = (miu) bucketLookupItemUnsignedId(
				pFastBucket, 
				&piiu->curMsg.m_available);
		UNLOCK;
		if(!monix){
			break;
		}

		/*
		 * 
		 * call handler, only if they did not clear the
		 * chid in the interim
		 */
		if (monix->usr_func) {
			args.usr = (void *) monix->usr_arg;
			args.chid = monix->chan;
			args.type = monix->type;
			args.count = monix->count;
			args.dbr = NULL;
			/*
			 * the channel id field is abused for
			 * write notify status
			 *
			 * write notify was added vo CA V4.1
			 */
			args.status = ntohl(piiu->curMsg.m_cid); 

			LOCKEVENTS;
			(*monix->usr_func) (args);
			UNLOCKEVENTS;
		}
		LOCK;
		ellDelete(&pend_write_list, &monix->node);
		caIOBlockFree(monix);
		UNLOCK;

		break;

	}	
	case CA_PROTO_READ_NOTIFY:
	{
		struct event_handler_args args;

		/*
		 * run the user's event handler
		 */

		LOCK;
		monix = (miu) bucketLookupItemUnsignedId(
				pFastBucket, 
				&piiu->curMsg.m_available);
		UNLOCK;
		if(!monix){
			break;
		}

		/*
		 * 
		 * call handler, only if they did not clear the
	 	 * chid in the interim
		 */
		if (monix->usr_func) {
			int v41;

			/*
			 * convert the data buffer from net
			 * format to host format
			 *
			 */
#                     	ifdef CONVERSION_REQUIRED 
                               if (piiu->curMsg.m_type<NELEMENTS(cac_dbr_cvrt)) {
                                       (*cac_dbr_cvrt[piiu->curMsg.m_type])(
                                               piiu->pCurData, 
                                               piiu->pCurData, 
                                               FALSE,
                                               piiu->curMsg.m_count);
                               }
                               else {
                                       piiu->curMsg.m_cid = htonl(ECA_BADTYPE);
                               }
#                     	endif

			args.usr = (void *) monix->usr_arg;
			args.chid = monix->chan;
			args.type = piiu->curMsg.m_type;
			args.count = piiu->curMsg.m_count;
			args.dbr = piiu->pCurData;
			/*
			 * the channel id field is abused for
			 * read notify status starting
			 * with CA V4.1
			 */
			v41 = CA_V41(
				CA_PROTOCOL_VERSION, 
				piiu->minor_version_number);
			if(v41){
				args.status = ntohl(piiu->curMsg.m_cid);
			}
			else{
				args.status = ECA_NORMAL;
			}

			LOCKEVENTS;
			(*monix->usr_func) (args);
			UNLOCKEVENTS;
		}
		LOCK;
		ellDelete(&pend_read_list, &monix->node);
		caIOBlockFree(monix);
		UNLOCK;

		break;
	}
	case CA_PROTO_EVENT_ADD:
	{
		int v41;
		struct event_handler_args args;

		/*
		 * run the user's event handler 
		 */
		LOCK;
		monix = (miu) bucketLookupItemUnsignedId(
				pFastBucket, 
				&piiu->curMsg.m_available);
		UNLOCK;
		if(!monix){
			break;
		}


		/*
		 * m_postsize = 0  is a confirmation of a
		 * monitor cancel
		 */
		if (!piiu->curMsg.m_postsize) {
			LOCK;
			pChan = (ciu) monix->chan; /* discard const */
			ellDelete(&pChan->eventq, &monix->node);
			caIOBlockFree(monix);
			UNLOCK;

			break;
		}
		/* only call if not disabled */
		if (!monix->usr_func)
			break;

		/*
		 * convert the data buffer from net
		 * format to host format
		 */
#               ifdef CONVERSION_REQUIRED 
                       if (piiu->curMsg.m_type<NELEMENTS(cac_dbr_cvrt)) {
                               (*cac_dbr_cvrt[piiu->curMsg.m_type])(
                                       piiu->pCurData, 
                                       piiu->pCurData, 
                                       FALSE,
                                       piiu->curMsg.m_count);
                       }
                       else {
                               piiu->curMsg.m_cid = htonl(ECA_BADTYPE);
                       }
#		endif

		/*
		 * Orig version of CA didnt use this
		 * strucure. This would run faster if I had
		 * decided to pass a pointer to this
		 * structure rather than the structure itself
		 * early on.
		 */
		args.usr = (void *) monix->usr_arg;
		args.chid = monix->chan;
		args.type = piiu->curMsg.m_type;
		args.count = piiu->curMsg.m_count;
		args.dbr = piiu->pCurData;
		/*
		 * the channel id field is abused for
		 * event status starting
		 * with CA V4.1
		 */
		v41 = CA_V41(
			CA_PROTOCOL_VERSION, 
			piiu->minor_version_number);
		if(v41){
			args.status = ntohl(piiu->curMsg.m_cid); 
		}
		else{
			args.status = ECA_NORMAL;
		}

		/* call their handler */
		LOCKEVENTS;
		(*monix->usr_func) (args);
		UNLOCKEVENTS;

		break;
	}
	case CA_PROTO_READ:
	{
		miu	pIOBlock;

		/*
		 * verify the event id
		 */
		LOCK;
		pIOBlock = (miu) bucketLookupItemUnsignedId(
					pFastBucket, 
					&piiu->curMsg.m_available);
		UNLOCK;
		if(!pIOBlock){
			break;
		}

		/*
		 * only count get returns if from the current
		 * read seq
		 */
		if (VALID_MSG(piiu)){
			/*
			 * convert the data buffer from net
			 * format to host format
			 */
                       	if (piiu->curMsg.m_type <= (unsigned) LAST_BUFFER_TYPE) {
#				ifdef CONVERSION_REQUIRED 
                                       (*cac_dbr_cvrt[piiu->curMsg.m_type])(
                                               piiu->pCurData, 
                                               (void *) pIOBlock->usr_arg, 
                                               FALSE,
                                               piiu->curMsg.m_count);
#				else
                                       if (piiu->curMsg.m_type == DBR_STRING &&
                                               piiu->curMsg.m_count == 1u) {
                                               strcpy ((char *)pIOBlock->usr_arg,
                                                       piiu->pCurData);
                                       }
                                       else {
                                               memcpy(
                                                       (char *)pIOBlock->usr_arg,
                                                       piiu->pCurData,
                                                       dbr_size_n (
                                                               piiu->curMsg.m_type, 
                                                               piiu->curMsg.m_count)
                                                       );
                                       }
#				endif
				/*
				 * decrement the outstanding IO count
				 */
				CLRPENDRECV;
			}
		}
		LOCK;
		ellDelete(&pend_read_list, &pIOBlock->node);
		caIOBlockFree(pIOBlock);
		UNLOCK;
		break;
	}
	case CA_PROTO_SEARCH:
		perform_claim_channel(piiu, pnet_addr);
		break;

	case CA_PROTO_READ_SYNC:
		piiu->read_seq++;
		break;

	case CA_PROTO_RSRV_IS_UP:
		LOCK;
		{
			struct in_addr ina;
			
			/* 
			 * this allows a fan-out server to potentially
			 * insert the true address of a server 
			 * (servers prior to 3.13 always set this
			 * field to one of the ip addresses of the host)
			 * (clients prior to 3.13 always expect that this
			 * field is set to the server's IP address).
			 */
			if (piiu->curMsg.m_available != INADDR_ANY) {
				ina.s_addr = piiu->curMsg.m_available;
			}
			else {
				ina = *pnet_addr;
			}
			mark_server_available(&ina);
		}
		UNLOCK;
		break;

	case REPEATER_CONFIRM:
		ca_static->ca_repeater_contacted = TRUE;
#ifdef DEBUG
		ca_printf("CAC: repeater confirmation recv\n");
#endif
		break;

	case CA_PROTO_NOT_FOUND:
		break;

	case CA_PROTO_CLEAR_CHANNEL:
		clearChannelResources (piiu->curMsg.m_available);
		break;

	case CA_PROTO_ERROR:
	{
		ELLLIST		*pList = NULL;
		miu		monix;
		char		nameBuf[64];
		char           	context[255];
		caHdr  		*req = piiu->pCurData;
		int             op;
		struct exception_handler_args args;


		/*
		 * dont process the message if they have
		 * disabled notification
		 */
		if (!ca_static->ca_exception_func){
			break;
		}

		caHostFromInetAddr(pnet_addr, nameBuf, sizeof(nameBuf));

		if (piiu->curMsg.m_postsize > sizeof(caHdr)){
			sprintf(context, 
				"detected by: %s for: %s", 
				nameBuf, 
				(char *)(req+1));
		}
		else{
			sprintf(context, "detected by: %s", nameBuf);
		}

		/*
		 * Map internal op id to external op id so I
		 * can freely change the protocol in the
		 * future. This is quite wasteful of space
		 * however.
		 */
		monix = NULL;
		args.addr = NULL;
		args.pFile = NULL;
		args.lineNo = 0u;
		LOCK;
		switch (ntohs(req->m_cmmd)) {
		case CA_PROTO_READ_NOTIFY:
			monix = (miu) bucketLookupItemUnsignedId(
					pFastBucket, 
					&req->m_available);
			pList = &pend_read_list;
			op = CA_OP_GET;
			break;
		case CA_PROTO_READ:
			monix = (miu) bucketLookupItemUnsignedId(
					pFastBucket, 
					&req->m_available);
			if(monix){
				args.addr = (void *) monix->usr_arg;
			}
			pList = &pend_read_list;
			op = CA_OP_GET;
			break;
		case CA_PROTO_WRITE_NOTIFY:
			monix = (miu) bucketLookupItemUnsignedId(
					pFastBucket, 
					&req->m_available);
			pList = &pend_write_list;
			op = CA_OP_PUT;
			break;
		case CA_PROTO_WRITE:
			op = CA_OP_PUT;
			break;
		case CA_PROTO_SEARCH:
			op = CA_OP_SEARCH;
			break;
		case CA_PROTO_EVENT_ADD:
			monix = (miu) bucketLookupItemUnsignedId(
					pFastBucket, 
					&req->m_available);
			op = CA_OP_ADD_EVENT;
			if (monix) {
				ciu pChan = (ciu) monix->chan;
				pList = &pChan->eventq;
			}
			break;
		case CA_PROTO_EVENT_CANCEL:
			monix = (miu) bucketLookupItemUnsignedId(
					pFastBucket, 
					&req->m_available);
			op = CA_OP_CLEAR_EVENT;
			break;
		default:
			op = CA_OP_OTHER;
			break;
		}

		if (monix) {
			if (pList) {
				ellDelete(pList, &monix->node);
			}
			caIOBlockFree(monix);
		}

		args.chid = bucketLookupItemUnsignedId
				(pSlowBucket, &piiu->curMsg.m_cid);
		UNLOCK;
		args.usr = (void *) ca_static->ca_exception_arg;
		args.type = ntohs (req->m_type);	
		args.count = ntohs (req->m_count);
		args.stat = ntohl (piiu->curMsg.m_available);
		args.op = op;
		args.ctx = context;

		LOCKEVENTS;
		(*ca_static->ca_exception_func) (args);
		UNLOCKEVENTS;
		break;
	}
	case CA_PROTO_ACCESS_RIGHTS:
	{
		int	ar;
		ciu	chan;

		LOCK;
		chan = (ciu) bucketLookupItemUnsignedId(
				pSlowBucket, &piiu->curMsg.m_cid);
		UNLOCK;
		if(!chan){
			/*
			 * end up here if they delete the channel
			 * prior to connecting
			 */
			break;
		}

		ar = ntohl (piiu->curMsg.m_available);
		chan->ar.read_access = (ar&CA_PROTO_ACCESS_RIGHT_READ)?1:0;
		chan->ar.write_access = (ar&CA_PROTO_ACCESS_RIGHT_WRITE)?1:0;

		if (chan->pAccessRightsFunc) {
			struct access_rights_handler_args 	args;

			args.chid = chan;
			args.ar = chan->ar;
			(*chan->pAccessRightsFunc)(args);
		}
		break;
	}
	case CA_PROTO_CLAIM_CIU:
	{
		ciu	chan;

		LOCK;
		chan = (ciu) bucketLookupItemUnsignedId(
				pSlowBucket, &piiu->curMsg.m_cid);
		UNLOCK;
		if(!chan){
			/*
			 * end up here if they delete the channel
			 * prior to connecting
			 */
			break;
		}

		if (CA_V44(CA_PROTOCOL_VERSION,piiu->minor_version_number)) {
        		chan->id.sid = piiu->curMsg.m_available;
		}
		cac_reconnect_channel(chan);
		break;
	}
	case CA_PROTO_CLAIM_CIU_FAILED:
	case CA_PROTO_SERVER_DISCONN:
		verifyChanAndDisconnect(piiu, cs_conn);
		break;
	default:
		ca_printf("CAC: post_msg(): Corrupt cmd in msg %x\n", 
			piiu->curMsg.m_cmmd);

		return ERROR;
	}

	return OK;
}

/*
 * verifyChanAndDisconnect()
 */
LOCAL void verifyChanAndDisconnect(IIU *piiu, enum channel_state state)
{
	ciu 	chan;

	LOCK;
	chan = (ciu) bucketLookupItemUnsignedId(
			pSlowBucket, &piiu->curMsg.m_cid);
	if (!chan) {
		/*
		 * end up here if they delete the channel
		 * prior to this response 
		 */
		UNLOCK;
		return;
	}

	/*
	 * need to move the channel back to the cast IIU
	 * (so we will be able to reconnect)
	 */
	cacDisconnectChannel(chan, chan->state);
	UNLOCK;
}


/*
 *
 *	perform_claim_channel()
 *
 */
LOCAL void perform_claim_channel(
IIU			*piiu,
const struct in_addr	*pnet_addr
)
{
	unsigned short	port;
	char		rej[64];
      	ciu		chan;
	int		status;
	IIU		*allocpiiu;
	unsigned short	*pMinorVersion;
	unsigned	minorVersion;

	if (piiu!=piiuCast) {
		return;
	}

	/*
	 * ignore broadcast replies for deleted channels
	 * 
	 * lock required around use of the sprintf buffer
	 */
	LOCK;
	chan = (ciu) bucketLookupItemUnsignedId(
			pSlowBucket, 
			&piiu->curMsg.m_available);
	if(!chan){
		UNLOCK;
		return;
	}

	if(!chan->piiu){
		ca_printf("cast reply to local channel??\n");
		UNLOCK;
		return;
	}

	/*
	 * Ignore search replies to closing channels 
	 */ 
	if(chan->state == cs_closed) {
		UNLOCK;
		return;
	}

	/*
	 * Ignore duplicate search replies
	 */
	if (piiuCast != (IIU *) chan->piiu) {
		caAddrNode	*pNode;
		IIU		*tcpPIIU = (IIU *) chan->piiu;

		pNode = (caAddrNode *) ellFirst(&tcpPIIU->destAddr);
		assert(pNode);
		if (pNode->destAddr.in.sin_addr.s_addr != pnet_addr->s_addr) {
			caHostFromInetAddr(pnet_addr,rej,sizeof(rej));
			sprintf(
				sprintf_buf,
		"Channel: %s Accepted: %s Rejected: %s ",
				(char *)(chan + 1),
				tcpPIIU->host_name_str,
				rej);
			genLocalExcep (ECA_DBLCHNL, sprintf_buf);
		}
		UNLOCK;
		return;
	}

	/*
	 * Starting with CA V4.1 the minor version number
	 * is appended to the end of each search reply.
	 * This value is ignored by earlier clients.
	 */
	if(piiu->curMsg.m_postsize >= sizeof(*pMinorVersion)){
		pMinorVersion = (unsigned short *)(piiu->pCurData);
        	minorVersion = ntohs(*pMinorVersion);      
	}
	else{
		minorVersion = CA_UKN_MINOR_VERSION;
	}

	/*
	 * the type field is abused to carry the port number
	 * so that we can have multiple servers on one host
	 */
	if (CA_V45 (CA_PROTOCOL_VERSION,minorVersion)) {
		port = piiu->curMsg.m_type;
	}
	else {
		port = ca_static->ca_server_port;
	}
        status = alloc_ioc (pnet_addr, port, &allocpiiu);
	switch (status) {

	case ECA_NORMAL:
		break;

	case ECA_DISCONN:
		/*
		 * This indicates that the connection is tagged
		 * for shutdown and we are waiting for 
		 * it to go away. Search replies are ignored
		 * in the interim.
		 */
		UNLOCK;
	  	return;

	default:
		caHostFromInetAddr(pnet_addr,rej,sizeof(rej));
	  	ca_printf("CAC: ... %s ...\n", ca_message(status));
	 	ca_printf("CAC: for %s on %s\n", chan+1, rej);
	 	ca_printf("CAC: ignored search reply- proceeding\n");
		UNLOCK;
	  	return;

	}

	allocpiiu->minor_version_number = minorVersion;

	/*
	 * If this is the first channel to be
	 * added to this IIU then communicate
	 * the client's name to the server. 
	 * (CA V4.1 or higher)
	 */
	if (ellCount(&allocpiiu->chidlist)==0) {
		issue_identify_client(allocpiiu);
		issue_client_host_name(allocpiiu);
	}

	if (ca_static->ca_search_responses<ULONG_MAX) {
		ca_static->ca_search_responses++;
	}

	/*
	 * remove it from the broadcast IIU
	 */
	removeFromChanList(chan);

	/*
	 * chan->piiu must be correctly set prior to issuing the
	 * claim request
	 *
	 * add to the beginning of the list until we
	 * have sent the claim message (after which we
	 * move it to the end of the list)
	 *
	 * claim pending flag is set here
	 */
	addToChanList(chan, allocpiiu);
	
	/*
	 * Assume that we have access once connected briefly
	 * until the server is able to tell us the correct
	 * state for backwards compatibility.
	 *
	 * Their access rights call back does not get
	 * called for the first time until the information 
	 * arrives however.
	 */
	chan->ar.read_access = TRUE;
	chan->ar.write_access = TRUE;

	/*
	 * Reset the delay to the next search request if we get
	 * at least one response. This may result in an over 
	 * run of the UDP input queue of the server in some 
	 * cases (and therefore in redundant search requests) 
	 * but it does significantly reduce the connect delays 
	 * when 1000's of channels must be connected.
	 */
	ca_static->ca_conn_next_retry = ca_static->currentTime;

	/*
	 * claim the resource in the IOC
	 * over TCP so problems with duplicate UDP port
	 * after reboot go away
	 *
	 * if we cant immediately get buffer space then we
	 * attempt to flush once and then try again.
	 * If this fails then we will wait for the
	 * next search response.
	 */
        chan->id.sid = piiu->curMsg.m_cid;
	UNLOCK
	issue_claim_channel(chan);
}


/*
 * cac_reconnect_channel()
 *
 * LOCK must _not_ be applied
 */
void cac_reconnect_channel(ciu chan)
{
	IIU			*piiu = (IIU *) chan->piiu;
      	evid			pevent;
	enum channel_state	prev_cs;
	int			v41;

	prev_cs = chan->state;
	if (prev_cs == cs_conn) {
		ca_printf("CAC: Ignored conn resp to conn chan CID=%u SID=%u?\n",
			chan->cid, chan->id.sid);
		return;
	}

	LOCK;

	v41 = CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number);

        /*	Update rmt chid fields from caHdr fields	*/
        chan->type  = piiu->curMsg.m_type;      
        chan->count = piiu->curMsg.m_count;      


	/*
	 * set state to cs_conn before caling 
	 * ca_request_event() so their channel 
	 * connect tests wont fail
 	 */
	chan->state = cs_conn;

	/*
	 * NOTE: monitor and callback reissue must occur prior to calling
	 * their connection routine otherwise they could be requested twice.
	 */
#ifdef CALLBACK_REISSUE
        /* reissue any outstanding get callbacks for this channel */
	if(pend_read_list.count){
          	for(	pevent = (evid) pend_read_list.node.next; 
			pevent; 
			pevent = (evid) pevent->node.next){
            		if(pevent->chan == chan){
	      			issue_get_callback(pevent);
	    		}
		}
        }
#endif

        /* reissue any events (monitors) for this channel */
	if(chan->eventq.count){
          	for( 	pevent = (evid)chan->eventq.node.next; 
			pevent; 
			pevent = (evid)pevent->node.next)
	    		ca_request_event(pevent);
 	}

      	UNLOCK;

	/*
	 * if less than v4.1 then the server will never
	 * send access rights and we know that there
	 * will always be access and call their call back
	 * here
	 */
	if (chan->pAccessRightsFunc && !v41) {
		struct access_rights_handler_args 	args;

		args.chid = chan;
		args.ar = chan->ar;
		(*chan->pAccessRightsFunc)(args);
	}

      	if(chan->pConnFunc){
		struct connection_handler_args	args;

		args.chid = chan;
		args.op = CA_OP_CONN_UP;
		LOCKEVENTS;
        	(*chan->pConnFunc)(args);
		UNLOCKEVENTS;
	}
	else if(prev_cs==cs_never_conn){
          	/*  
		 * decrement the outstanding IO count 
		 */
          	CLRPENDRECV;
	}
}

