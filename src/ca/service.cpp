
/*
 * $Id$
 *
 *                    L O S  A L A M O S 
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *  
 *  Author: Jeff Hill
 *
 *  Copyright, 1986, The Regents of the University of California.
 * 
 */

#include    "iocinf.h"

#ifdef CONVERSION_REQUIRED 
extern CACVRTFUNC *cac_dbr_cvrt[];
#endif /*CONVERSION_REQUIRED*/

typedef void (*pProtoStubTCP) (tcpiiu *piiu);
typedef void (*pProtoStubUDP) (udpiiu *piiu, const struct sockaddr_in *pnet_addr);

/*
 * tcp_noop_action ()
 */
LOCAL void tcp_noop_action (tcpiiu * /* piiu */)
{
    return;
}

/*
 * udp_noop_action ()
 */
LOCAL void udp_noop_action (udpiiu * /* piiu */, const struct sockaddr_in * /* pnet_addr */)
{
    return;
}


/*
 * echo_resp_action ()
 */
LOCAL void echo_resp_action (tcpiiu *piiu)
{
    piiu->echoPending = FALSE;
    piiu->beaconAnomaly = FALSE;
    return;
}

/*
 * write_notify_resp_action ()
 */
LOCAL void write_notify_resp_action (tcpiiu *piiu)
{
    struct event_handler_args args;
    nmiu *monix;

    /*
     * run the user's event handler
     */
    LOCK (piiu->niiu.iiu.pcas);
    monix = (nmiu *) bucketLookupItemUnsignedId (piiu->niiu.iiu.pcas->ca_pFastBucket, 
                &piiu->niiu.curMsg.m_available);
    if (!monix) {
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    /*
     * 
     * call handler, only if they did not clear the
     * chid in the interim
     */
    if (monix->miu.usr_func) {
        args.usr = (void *) monix->miu.usr_arg;
        args.chid = monix->miu.pChan;
        args.type = monix->miu.type;
        args.count = monix->miu.count;
        args.dbr = NULL;
        /*
         * the channel id field is abused for
         * write notify status
         *
         * write notify was added vo CA V4.1
         */
        args.status = ntohl (piiu->niiu.curMsg.m_cid); 

        (*monix->miu.usr_func) (args);
    }
    caIOBlockFree (piiu->niiu.iiu.pcas, monix->id);
    UNLOCK (piiu->niiu.iiu.pcas);

    return;
}

/*
 * read_notify_resp_action ()
 */
LOCAL void read_notify_resp_action (tcpiiu *piiu)
{
    nmiu *monix;
    struct event_handler_args args;

    /*
     * run the user's event handler
     */
    LOCK (piiu->niiu.iiu.pcas);
    monix = (nmiu *) bucketLookupItemUnsignedId(
            piiu->niiu.iiu.pcas->ca_pFastBucket, 
            &piiu->niiu.curMsg.m_available);
    if(!monix){
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    /*
     * call handler, only if they did not clear the
     * chid in the interim
     */
    if (monix->miu.usr_func) {
        int v41;

        /*
         * convert the data buffer from net
         * format to host format
         */
#       ifdef CONVERSION_REQUIRED 
            if (piiu->niiu.curMsg.m_dataType<NELEMENTS(cac_dbr_cvrt)) {
                (*cac_dbr_cvrt[piiu->niiu.curMsg.m_dataType])(
                     piiu->niiu.pCurData, 
                     piiu->niiu.pCurData, 
                     FALSE,
                     piiu->niiu.curMsg.m_count);
            }
            else {
                piiu->niiu.curMsg.m_cid = htonl(ECA_BADTYPE);
            }
#       endif

        args.usr = (void *)monix->miu.usr_arg;
        args.chid = monix->miu.pChan;
        args.type = piiu->niiu.curMsg.m_dataType;
        args.count = piiu->niiu.curMsg.m_count;
        args.dbr = piiu->niiu.pCurData;
        /*
         * the channel id field is abused for
         * read notify status starting
         * with CA V4.1
         */
        v41 = CA_V41(
            CA_PROTOCOL_VERSION, 
            piiu->minor_version_number);
        if(v41){
            args.status = ntohl(piiu->niiu.curMsg.m_cid);
        }
        else{
            args.status = ECA_NORMAL;
        }

        (*monix->miu.usr_func) (args);
    }
    caIOBlockFree (piiu->niiu.iiu.pcas, monix->id);
    UNLOCK (piiu->niiu.iiu.pcas);

    return;
}

/*
 * event_resp_action ()
 */
LOCAL void event_resp_action (tcpiiu *piiu)
{
    struct event_handler_args args;
    nmiu *monix;
    int v41;


    /*
     * run the user's event handler 
     */
    LOCK (piiu->niiu.iiu.pcas);
    monix = (nmiu *) bucketLookupItemUnsignedId(
            piiu->niiu.iiu.pcas->ca_pFastBucket, &piiu->niiu.curMsg.m_available);
    if (!monix) {
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    /*
     * m_postsize = 0 used to be a confirmation, but is
     * now a noop because the above hash lookup will 
     * not find a matching IO block
     */
    if (!piiu->niiu.curMsg.m_postsize) {
        caIOBlockFree (piiu->niiu.iiu.pcas, monix->id);
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }
    /* only call if not disabled */
    if (!monix->miu.usr_func) {
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    /*
     * convert the data buffer from net
     * format to host format
     */
#   ifdef CONVERSION_REQUIRED 
        if (piiu->niiu.curMsg.m_dataType<NELEMENTS(cac_dbr_cvrt)) {
             (*cac_dbr_cvrt[piiu->niiu.curMsg.m_dataType])(
                   piiu->niiu.pCurData, 
                   piiu->niiu.pCurData, 
                   FALSE,
                   piiu->niiu.curMsg.m_count);
        }
        else {
             piiu->niiu.curMsg.m_cid = htonl(ECA_BADTYPE);
        }
#   endif

    /*
     * Orig version of CA didnt use this
     * strucure. This would run faster if I had
     * decided to pass a pointer to this
     * structure rather than the structure itself
     * early on.
     */
    args.usr = (void *) monix->miu.usr_arg;
    args.chid = monix->miu.pChan;
    args.type = piiu->niiu.curMsg.m_dataType;
    args.count = piiu->niiu.curMsg.m_count;
    args.dbr = piiu->niiu.pCurData;
    /*
     * the channel id field is abused for
     * event status starting
     * with CA V4.1
     */
    v41 = CA_V41(
        CA_PROTOCOL_VERSION, 
        piiu->minor_version_number);
    if(v41){
        args.status = ntohl(piiu->niiu.curMsg.m_cid); 
    }
    else{
        args.status = ECA_NORMAL;
    }

    /* call their handler */
    (*monix->miu.usr_func) (args);
    UNLOCK (piiu->niiu.iiu.pcas);

    return;
}

/*
 * read_resp_action ()
 */
LOCAL void read_resp_action (tcpiiu *piiu)
{
    nmiu *pIOBlock;

    /*
     * verify the event id
     */
    LOCK (piiu->niiu.iiu.pcas);
    pIOBlock = (nmiu *) bucketLookupItemUnsignedId (piiu->niiu.iiu.pcas->ca_pFastBucket, 
                        &piiu->niiu.curMsg.m_available);
    if (!pIOBlock) {
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
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
        if (piiu->niiu.curMsg.m_dataType <= (unsigned) LAST_BUFFER_TYPE) {
#               ifdef CONVERSION_REQUIRED 
                (*cac_dbr_cvrt[piiu->niiu.curMsg.m_dataType])(
                     piiu->niiu.pCurData, 
                     pIOBlock->miu.usr_arg, 
                     FALSE,
                     piiu->niiu.curMsg.m_count);
#               else
                if (piiu->niiu.curMsg.m_dataType == DBR_STRING &&
                     piiu->niiu.curMsg.m_count == 1u) {
                     strcpy ( (char *) pIOBlock->miu.usr_arg,
                         (char *) piiu->niiu.pCurData );
                }
                else {
                     memcpy(
                         (char *)pIOBlock->miu.usr_arg,
                         piiu->niiu.pCurData,
                         dbr_size_n (
                               piiu->niiu.curMsg.m_dataType, 
                               piiu->niiu.curMsg.m_count)
                         );
                }
#               endif
            /*
             * decrement the outstanding IO count
             */
            if (--piiu->niiu.iiu.pcas->ca_pndrecvcnt==0u) {
                semBinaryGive (piiu->niiu.iiu.pcas->ca_io_done_sem);
            }
        }
    }
    caIOBlockFree (piiu->niiu.iiu.pcas, pIOBlock->id);
    UNLOCK (piiu->niiu.iiu.pcas);

    return;
}

/*
 * search_resp_action ()
 */
LOCAL void search_resp_action (udpiiu *piiu, const struct sockaddr_in *pnet_addr)
{
    struct sockaddr_in  ina;
    char                rej[64];
    nciu                *chan;
    tcpiiu              *allocpiiu;
    unsigned short      *pMinorVersion;
    unsigned            minorVersion;

    /*
     * ignore broadcast replies for deleted channels
     * 
     * lock required around use of the sprintf buffer
     */
    LOCK (piiu->niiu.iiu.pcas);
    chan = (nciu *) bucketLookupItemUnsignedId (piiu->niiu.iiu.pcas->ca_pSlowBucket, 
                    &piiu->niiu.curMsg.m_available);
    if (!chan) {
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each search reply.
     * This value is ignored by earlier clients.
     */
    if(piiu->niiu.curMsg.m_postsize >= sizeof(*pMinorVersion)){
        pMinorVersion = (unsigned short *)(piiu->niiu.pCurData);
        minorVersion = ntohs(*pMinorVersion);      
    }
    else{
        minorVersion = CA_UKN_MINOR_VERSION;
    }

    /*
     * the type field is abused to carry the port number
     * so that we can have multiple servers on one host
     */
    ina.sin_family = AF_INET;
    if (CA_V48 (CA_PROTOCOL_VERSION,minorVersion)) {
        if (piiu->niiu.curMsg.m_cid != INADDR_BROADCAST) {
            /*
             * Leave address in network byte order (m_cid has not been 
             * converted to the local byte order)
             */
            ina.sin_addr.s_addr = piiu->niiu.curMsg.m_cid;
        }
        else {
            ina.sin_addr = pnet_addr->sin_addr;
        }
        ina.sin_port = htons (piiu->niiu.curMsg.m_dataType);
    }
    else if (CA_V45 (CA_PROTOCOL_VERSION,minorVersion)) {
        ina.sin_port = htons(piiu->niiu.curMsg.m_dataType);
        ina.sin_addr = pnet_addr->sin_addr;
    }
    else {
        ina.sin_port = htons(piiu->niiu.iiu.pcas->ca_server_port);
        ina.sin_addr = pnet_addr->sin_addr;
    }

    /*
     * Ignore duplicate search replies
     */
    if (&piiu->niiu.iiu.pcas->pudpiiu->niiu.iiu != chan->ciu.piiu) {
        tcpiiu *ptcpiiu = iiuToTCPIIU (chan->ciu.piiu);

        if (ptcpiiu->dest.ia.sin_addr.s_addr != ina.sin_addr.s_addr ||
            ptcpiiu->dest.ia.sin_port != ina.sin_port) {
            ipAddrToA (pnet_addr, rej, sizeof(rej));
            sprintf (piiu->niiu.iiu.pcas->ca_sprintf_buf, 
                    "Channel: %s Accepted: %s Rejected: %s ",
                    ca_name (&chan->ciu), ptcpiiu->host_name_str, rej);
            genLocalExcep (piiu->niiu.iiu.pcas, ECA_DBLCHNL, piiu->niiu.iiu.pcas->ca_sprintf_buf);
        }
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    allocpiiu = constructTCPIIU (piiu->niiu.iiu.pcas, &ina, minorVersion);
    if (!allocpiiu) {
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    /*
     * If this is the first channel to be
     * added to this niiu then communicate
     * the client's name to the server. 
     * (CA V4.1 or higher)
     */
    if (ellCount(&allocpiiu->niiu.chidList)==0) {
        issue_identify_client (allocpiiu);
        issue_client_host_name (allocpiiu);
    }

    piiu->searchTmr.notifySearchResponse (chan);

    /*
     * remove it from the broadcast niiu
     */
    removeFromChanList (chan);

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
    addToChanList (chan, &allocpiiu->niiu);
    
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
     * claim the resource in the IOC
     * over TCP so problems with duplicate UDP port
     * after reboot go away
     *
     * if we cant immediately get buffer space then we
     * attempt to flush once and then try again.
     * If this fails then we will wait for the
     * next search response.
     */
    chan->sid = piiu->niiu.curMsg.m_cid;

    if (!CA_V42(CA_PROTOCOL_VERSION, minorVersion)) {
        chan->type  = piiu->niiu.curMsg.m_dataType;      
        chan->count = piiu->niiu.curMsg.m_count;
    }

    issue_claim_channel (chan);
    cacRingBufferWriteFlush (&allocpiiu->send);
    UNLOCK (piiu->niiu.iiu.pcas);
}

/*
 * read_sync_resp_action ()
 */
LOCAL void read_sync_resp_action (tcpiiu *piiu)
{
    piiu->read_seq++;
    return;
}

/*
 * beacon_action ()
 */
LOCAL void beacon_action (udpiiu *piiu, const struct sockaddr_in *pnet_addr)
{
    struct sockaddr_in ina;

    LOCK (piiu->niiu.iiu.pcas);
        
    /* 
     * this allows a fan-out server to potentially
     * insert the true address of the CA server 
     *
     * old servers:
     *   1) set this field to one of the ip addresses of the host _or_
     *   2) set this field to htonl(INADDR_ANY)
     * new servers:
     *      always set this field to htonl(INADDR_ANY)
     *
     * clients always assume that if this
     * field is set to something that isnt htonl(INADDR_ANY)
     * then it is the overriding IP address of the server.
     */
    ina.sin_family = AF_INET;
    if (piiu->niiu.curMsg.m_available != htonl(INADDR_ANY)) {
        ina.sin_addr.s_addr = piiu->niiu.curMsg.m_available;
    }
    else {
        ina.sin_addr = pnet_addr->sin_addr;
    }
    if (piiu->niiu.curMsg.m_count != 0) {
        ina.sin_port = htons (piiu->niiu.curMsg.m_count);
    }
    else {
        /*
         * old servers dont supply this and the
         * default port must be assumed
         */
        ina.sin_port = htons (piiu->niiu.iiu.pcas->ca_server_port);
    }
    mark_server_available (piiu->niiu.iiu.pcas, &ina);

    UNLOCK (piiu->niiu.iiu.pcas);

    return;
}

/*
 * repeater_ack_action ()
 */
LOCAL void repeater_ack_action (udpiiu *piiu, const struct sockaddr_in * /* pnet_addr */)
{
    piiu->repeaterContacted = 1u;
#   ifdef DEBUG
        ca_printf (piiu->niiu.iiu.pcas, "CAC: repeater confirmation recv\n");
#   endif
    return;
}

/*
 * not_here_resp_action ()
 */
LOCAL void not_here_resp_action (udpiiu * /* piiu */, const struct sockaddr_in * /* pnet_addr */)
{
    return;
}

/*
 * clear_channel_resp_action ()
 */
LOCAL void clear_channel_resp_action (tcpiiu * /* piiu */)
{
    /* presently a noop */
    return;
}

/*
 * exception_resp_action ()
 */
LOCAL void exception_resp_action (tcpiiu *piiu)
{
    nmiu *monix;
    nciu *pChan;
    char context[255];
    caHdr *req = (caHdr *) piiu->niiu.pCurData;
    int op;
    struct exception_handler_args args;

    /*
     * dont process the message if they have
     * disabled notification
     */
    if (!piiu->niiu.iiu.pcas->ca_exception_func){
        return;
    }

    if (piiu->niiu.curMsg.m_postsize > sizeof(caHdr)){
        sprintf (context, "detected by: %s for: %s", 
            piiu->host_name_str, (char *)(req+1));
    }
    else{
        sprintf (context, "detected by: %s", piiu->host_name_str);
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
    args.chid = NULL;

    LOCK (piiu->niiu.iiu.pcas);
    switch (ntohs(req->m_cmmd)) {
    case CA_PROTO_READ_NOTIFY:
        monix = (nmiu *) bucketLookupItemUnsignedId(
                piiu->niiu.iiu.pcas->ca_pFastBucket, &req->m_available);
        op = CA_OP_GET;
        break;
    case CA_PROTO_READ:
        monix = (nmiu *) bucketLookupItemUnsignedId(
                piiu->niiu.iiu.pcas->ca_pFastBucket, &req->m_available);
        if (monix) {
            args.addr = monix->miu.usr_arg;
        }
        op = CA_OP_GET;
        break;
    case CA_PROTO_WRITE_NOTIFY:
        monix = (nmiu *) bucketLookupItemUnsignedId(
                piiu->niiu.iiu.pcas->ca_pFastBucket, &req->m_available);
        op = CA_OP_PUT;
        break;
    case CA_PROTO_WRITE:
        op = CA_OP_PUT;
        pChan = (nciu *) bucketLookupItemUnsignedId
                (piiu->niiu.iiu.pcas->ca_pSlowBucket, &piiu->niiu.curMsg.m_cid);
        args.chid = (chid) &pChan->ciu;
        break;
    case CA_PROTO_SEARCH:
        op = CA_OP_SEARCH;
        pChan = (nciu *) bucketLookupItemUnsignedId
                (piiu->niiu.iiu.pcas->ca_pSlowBucket, &piiu->niiu.curMsg.m_cid);
        args.chid = (chid) &pChan->ciu;
        break;
    case CA_PROTO_EVENT_ADD:
        monix = (nmiu *) bucketLookupItemUnsignedId(
                piiu->niiu.iiu.pcas->ca_pFastBucket, &req->m_available);
        op = CA_OP_ADD_EVENT;
        break;
    case CA_PROTO_EVENT_CANCEL:
        monix = (nmiu *) bucketLookupItemUnsignedId(
                piiu->niiu.iiu.pcas->ca_pFastBucket, &req->m_available);
        op = CA_OP_CLEAR_EVENT;
        break;
    default:
        op = CA_OP_OTHER;
        break;
    }

    if (monix) {
        args.chid = monix->miu.pChan;
        caIOBlockFree (piiu->niiu.iiu.pcas, monix->id);
    }

    args.usr = piiu->niiu.iiu.pcas->ca_exception_arg;
    args.type = ntohs (req->m_dataType);    
    args.count = ntohs (req->m_count);
    args.stat = ntohl (piiu->niiu.curMsg.m_available);
    args.op = op;
    args.ctx = context;

    (*piiu->niiu.iiu.pcas->ca_exception_func) (args);
    UNLOCK (piiu->niiu.iiu.pcas);

    return;
}

/*
 * access_rights_resp_action ()
 */
LOCAL void access_rights_resp_action (tcpiiu *piiu)
{
    int ar;
    nciu *chan;

    LOCK (piiu->niiu.iiu.pcas);
    chan = (nciu *) bucketLookupItemUnsignedId(
            piiu->niiu.iiu.pcas->ca_pSlowBucket, &piiu->niiu.curMsg.m_cid);
    if (!chan) {
        /*
         * end up here if they delete the channel
         * prior to connecting
         */
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    ar = ntohl (piiu->niiu.curMsg.m_available);
    chan->ar.read_access = (ar&CA_PROTO_ACCESS_RIGHT_READ)?1:0;
    chan->ar.write_access = (ar&CA_PROTO_ACCESS_RIGHT_WRITE)?1:0;

    if (chan->ciu.pAccessRightsFunc) {
        struct access_rights_handler_args args;

        args.chid = (chid) &chan->ciu;
        args.ar = chan->ar;
        (*chan->ciu.pAccessRightsFunc)(args);
    }
    UNLOCK (piiu->niiu.iiu.pcas);
    return;
}

/*
 * claim_ciu_resp_action ()
 */
LOCAL void claim_ciu_resp_action (tcpiiu *piiu)
{
    cac_reconnect_channel (piiu, piiu->niiu.curMsg.m_cid, 
        piiu->niiu.curMsg.m_dataType, piiu->niiu.curMsg.m_count);
    return;
}

/*
 * verifyAndDisconnectChan ()
 */
LOCAL void verifyAndDisconnectChan (tcpiiu *piiu)
{
    nciu *chan;

    LOCK (piiu->niiu.iiu.pcas);
    chan = (nciu *) bucketLookupItemUnsignedId(
            piiu->niiu.iiu.pcas->ca_pSlowBucket, &piiu->niiu.curMsg.m_cid);
    if (!chan) {
        /*
         * end up here if they delete the channel
         * prior to this response 
         */
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    /*
     * need to move the channel back to the cast niiu
     * (so we will be able to reconnect)
     *
     * this marks the niiu for disconnect if the channel 
     * count goes to zero
     */
    cacDisconnectChannel (chan);
    UNLOCK (piiu->niiu.iiu.pcas);
    return;
}

/*
 * bad_tcp_resp_action ()
 */
LOCAL void bad_tcp_resp_action (tcpiiu *piiu)
{
    ca_printf (piiu->niiu.iiu.pcas, "CAC: Bad response code in TCP message from %s was %u\n", 
        piiu->host_name_str, piiu->niiu.curMsg.m_cmmd);
}

/*
 * bad_udp_resp_action ()
 */
LOCAL void bad_udp_resp_action (udpiiu *piiu, const struct sockaddr_in *pnet_addr)
{
    char buf[256];
    ipAddrToA ( pnet_addr, buf, sizeof (buf) );
    ca_printf (piiu->niiu.iiu.pcas, "CAC: Bad response code in UDP message from %s was %u\n", 
        buf, piiu->niiu.curMsg.m_cmmd);
}

/*
 * cac_reconnect_channel()
 */
void cac_reconnect_channel (tcpiiu *piiu, caResId cid, unsigned short type, unsigned long count)
{
    nmiu *pevent;
    unsigned prevConn;
    int v41;
    nciu *chan;

    LOCK (piiu->niiu.iiu.pcas);
    chan = (nciu *) bucketLookupItemUnsignedId (piiu->niiu.iiu.pcas->ca_pSlowBucket, &cid);
    /*
     * this test will fail if they delete the channel
     * prior to the reply from the claim message
     */
    if (!chan) {
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    if (chan->connected) {
        ca_printf(piiu->niiu.iiu.pcas,
            "CAC: Ignored conn resp to conn chan CID=%u SID=%u?\n",
            chan->cid, chan->sid);
        UNLOCK (piiu->niiu.iiu.pcas);
        return;
    }

    v41 = CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number);

    if (CA_V44(CA_PROTOCOL_VERSION, piiu->minor_version_number)) {
        chan->sid = piiu->niiu.curMsg.m_available;
    }

    /* 
     * Update rmt chid fields from caHdr fields 
     * if they are valid
     */
    if (type != USHRT_MAX) {
        chan->type  = type;      
        chan->count = count;
    }

    /*
     * set state to connected before caling 
     * ca_request_event() so their channel 
     * connect tests wont fail
     */
    chan->connected = 1;
    prevConn = chan->previousConn;
    chan->previousConn = 1;

    /*
     * NOTE: monitor and callback reissue must occur prior to calling
     * their connection routine otherwise they could be requested twice.
     * 
     * resubscribe for monitors from this channel 
     */
    if ( ellCount (&chan->eventq) ) {
        for (pevent = (nmiu *) ellFirst (&chan->eventq); 
            pevent; pevent = (nmiu *) ellNext (&pevent->node) ) {
            ca_request_event (chan, pevent);
        }
    }

    /*
     * if less than v4.1 then the server will never
     * send access rights and we know that there
     * will always be access and call their call back
     * here
     */
    if (chan->ciu.pAccessRightsFunc && !v41) {
        struct access_rights_handler_args   args;

        args.chid = (chid) &chan->ciu;
        args.ar = chan->ar;
        (*chan->ciu.pAccessRightsFunc)(args);
    }

    if (chan->ciu.pConnFunc) {
        struct connection_handler_args  args;

        args.chid = (chid) &chan->ciu;
        args.op = CA_OP_CONN_UP;

        (*chan->ciu.pConnFunc)(args);
        
    }
    else if (!prevConn) {
        /*  
         * decrement the outstanding IO count 
         */
        if (--piiu->niiu.iiu.pcas->ca_pndrecvcnt==0u) {
            semBinaryGive (piiu->niiu.iiu.pcas->ca_io_done_sem);
        }
    }
    UNLOCK (piiu->niiu.iiu.pcas);
}   


/*
 * TCP protocol jump table
 */
LOCAL const pProtoStubTCP tcpJumpTableCAC[] = 
{
    tcp_noop_action,
    event_resp_action,
    bad_tcp_resp_action,
    read_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    read_sync_resp_action,
    exception_resp_action,
    clear_channel_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    read_notify_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    claim_ciu_resp_action,
    write_notify_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    access_rights_resp_action,
    echo_resp_action,
    bad_tcp_resp_action,
    bad_tcp_resp_action,
    verifyAndDisconnectChan,
    verifyAndDisconnectChan
};

/*
 * UDP protocol jump table
 */
LOCAL const pProtoStubUDP udpJumpTableCAC[] = 
{
    udp_noop_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    search_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    beacon_action,
    not_here_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    repeater_ack_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action
};

/*
 * post_msg()
 *
 * LOCK should be applied when calling this routine
 *
 */
int post_msg (netIIU *piiu, const struct sockaddr_in *pnet_addr, 
              char *pInBuf, unsigned long blockSize)
{
    unsigned long size;

    while (blockSize) {

        /*
         * fetch a complete message header
         */
        if ( piiu->curMsgBytes < sizeof (piiu->curMsg) ) {
            char  *pHdr;

            size = sizeof (piiu->curMsg) - piiu->curMsgBytes;
            size = min (size, blockSize);
            
            pHdr = (char *) &piiu->curMsg;
            memcpy( pHdr + piiu->curMsgBytes, pInBuf, size);
            
            piiu->curMsgBytes += size;
            if (piiu->curMsgBytes < sizeof(piiu->curMsg)) {
#if 0 
                printf ("waiting for %d msg hdr bytes\n", 
                    sizeof(piiu->curMsg)-piiu->curMsgBytes);
#endif
                return ECA_NORMAL;
            }

            pInBuf += size;
            blockSize -= size;
        
            /* 
             * fix endian of bytes 
             */
            piiu->curMsg.m_postsize = 
                ntohs(piiu->curMsg.m_postsize);
            piiu->curMsg.m_cmmd = ntohs(piiu->curMsg.m_cmmd);
            piiu->curMsg.m_dataType = ntohs(piiu->curMsg.m_dataType);
            piiu->curMsg.m_count = ntohs(piiu->curMsg.m_count);
#if 0
            ca_printf (piiu->niiu.iiu.pcas,
                "%s Cmd=%3d Type=%3d Count=%4d Size=%4d",
                piiu->host_name_str,
                piiu->curMsg.m_cmmd,
                piiu->curMsg.m_dataType,
                piiu->curMsg.m_count,
                piiu->curMsg.m_postsize);
            ca_printf (piiu->niiu.iiu.pcas,
                " Avail=%8x Cid=%6d\n",
                piiu->curMsg.m_available,
                piiu->curMsg.m_cid);
#endif
        }


        /*
         * dont allow huge msg body until
         * the server supports it
         */
        if (piiu->curMsg.m_postsize>(unsigned)MAX_TCP) {
            piiu->curMsgBytes = 0;
            piiu->curDataBytes = 0;
            return ECA_TOLARGE;
        }

        /*
         * make sure we have a large enough message body cache
         */
        if (piiu->curMsg.m_postsize>piiu->curDataMax) {
            void *pCurData;
            size_t cacheSize;

            /* 
             * scalar DBR_STRING is sometimes clipped to the
             * actual string size so make sure this cache is
             * as large as one DBR_STRING so they will
             * not page fault if they read MAX_STRING_SIZE
             * bytes (instead of the actual string size).
             */
            cacheSize = max (piiu->curMsg.m_postsize, MAX_STRING_SIZE);
            pCurData = (void *) calloc(1u, cacheSize);
            if (!pCurData) {
                return ECA_ALLOCMEM;
            }
            if (piiu->pCurData) {
                free (piiu->pCurData);
            }
            piiu->pCurData = pCurData;
            piiu->curDataMax = piiu->curMsg.m_postsize;
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
            pBdy = (char *) piiu->pCurData;
            memcpy ( pBdy+piiu->curDataBytes, pInBuf, size);
            piiu->curDataBytes += size;
            if (piiu->curDataBytes < piiu->curMsg.m_postsize) {
#if 0
                printf ("waiting for %d msg bdy bytes\n", 
                piiu->curMsg.m_postsize-piiu->curDataBytes);
#endif
                return ECA_NORMAL;
            }
            pInBuf += size;
            blockSize -= size;
        }   

        /*
         * execute the response message
         */
        if (piiu == &piiu->iiu.pcas->pudpiiu->niiu) {
            pProtoStubUDP      pStub;
            if (piiu->curMsg.m_cmmd>=NELEMENTS(udpJumpTableCAC)) {
                pStub = bad_udp_resp_action;
            }
            else {
                pStub = udpJumpTableCAC [piiu->curMsg.m_cmmd];
            }
            (*pStub) (piiu->iiu.pcas->pudpiiu, pnet_addr);

        }
        else {
            pProtoStubTCP      pStub;
            if (piiu->curMsg.m_cmmd>=NELEMENTS(tcpJumpTableCAC)) {
                pStub = bad_tcp_resp_action;
            }
            else {
                pStub = tcpJumpTableCAC [piiu->curMsg.m_cmmd];
            }
            (*pStub) (iiuToTCPIIU(&piiu->iiu));
        }
         
        piiu->curMsgBytes = 0;
        piiu->curDataBytes = 0;

    }
    return ECA_NORMAL;
}


