

/* * $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "iocinf.h"
#include "inetAddrID_IL.h"
#include "bhe_IL.h"

tsFreeList < class tcpiiu, 16 > tcpiiu::freeList;

#ifdef DEBUG
#   define debugPrintf(argsInParen) printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

#ifdef CONVERSION_REQUIRED 
extern CACVRTFUNC *cac_dbr_cvrt[];
#endif /*CONVERSION_REQUIRED*/

typedef void (*pProtoStubTCP) (tcpiiu *piiu);

const static char nullBuff[32] = {
    0,0,0,0,0,0,0,0,0,0, 
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0
};

/*
 * constructTCPIIU ()
 * (lock must be applied by caller)
 */
tcpiiu * constructTCPIIU (cac *pcac, const struct sockaddr_in *pina, 
                                     unsigned minorVersion)
{
    bhe *pBHE;
    tcpiiu *piiu;

    /*
     * look for an existing virtual circuit
     */
    pBHE = pcac->lookupBeaconInetAddr ( *pina );
    if ( ! pBHE ) {
        pBHE = pcac->createBeaconHashEntry ( *pina, osiTime () );
        if ( ! pBHE ) {
            return NULL;
        }
    }
    
    piiu = pBHE->getIIU ();
    if ( piiu ) {
        if ( piiu->state == iiu_connecting || 
            piiu->state == iiu_connected ) {
            return piiu;
        }
        else {
            return NULL;
        }
    }

    piiu = new tcpiiu (pcac, *pina, minorVersion, *pBHE);
    if (!piiu) {
        return NULL;
    }

    if ( piiu->fullyConstructed () ) {
        return piiu;
    }
    else {
        delete piiu;
        return NULL;
    }
}


/*
 * tcpiiu::connect ()
 */
void tcpiiu::connect ()
{
    int status;
        
    /* 
     * attempt to connect to a CA server
     */
    this->armSendWatchdog ();
    while (1) {
        int errnoCpy;

        status = ::connect ( this->sock, &this->dest.sa,
                sizeof ( this->dest.sa ) );
        if ( status == 0 ) {
            this->cancelSendWatchdog ();
            // put the iiu into the connected state
            this->state = iiu_connected;
            // start connection activity watchdog
            this->connectNotify (); 
            return;
        }

        errnoCpy = SOCKERRNO;

        if ( errnoCpy == SOCK_EINTR ) {
            if ( this->state == iiu_disconnected ) {
                return;
            }
            else {
                continue;
            }
        }
        else if ( errnoCpy == SOCK_SHUTDOWN ) {
            return;
        }
        else {  
            this->cancelSendWatchdog ();
            ca_printf ( "Unable to connect because %d=\"%s\"\n", 
                errnoCpy, SOCKERRSTR (errnoCpy) );
            this->shutdown ();
            return;
        }
    }
}

/*
 * retryPendingClaims()
 *
 * This assumes that all channels with claims pending are at the 
 * front of the list (and that the channel is moved to the end of
 * the list when a claim message has been sent for it)
 *
 * We send claim messages here until the outgoing message buffer 
 * will not accept any more messages
 */
LOCAL void retryPendingClaims (tcpiiu *piiu)
{
    bool success;

    LOCK (piiu->pcas);
    tsDLIterBD<nciu> chan ( piiu->chidList.first () );
    while ( chan != chan.eol () ) {
        if (!chan->claimPending) {
            piiu->claimRequestsPending = false;
            piiu->flush ();
            break;
        }
        // this moves the channel to the end of the list
        success = chan->claimMsg (piiu);
        if ( ! success ) {
            piiu->flush ();
            break;
        }
        chan = piiu->chidList.first ();
    }
    UNLOCK (piiu->pcas);
}

/*
 *  cacSendThreadTCP ()
 */
extern "C" void cacSendThreadTCP (void *pParam)
{
    tcpiiu *piiu = (tcpiiu *) pParam;

    while ( true ) {
        unsigned sendCnt;
        char *pOutBuf;
        int status;

        pOutBuf = static_cast <char *> ( cacRingBufferReadReserveNoBlock (&piiu->send, &sendCnt) );
        while ( ! pOutBuf ) {
            piiu->cancelSendWatchdog ();
            if ( piiu->state != iiu_connected ) {
                semBinaryGive ( piiu->sendThreadExitSignal );
                return;
            }
            pOutBuf = (char *) cacRingBufferReadReserve (&piiu->send, &sendCnt);
        }

        assert ( sendCnt <= INT_MAX );
        status = send ( piiu->sock, pOutBuf, (int) sendCnt, 0 );
        if ( status > 0 ) {
            cacRingBufferReadCommit ( &piiu->send, (unsigned long) status );
            cacRingBufferReadFlush ( &piiu->send );
            if ( piiu->claimRequestsPending ) {
                retryPendingClaims ( piiu );
            }
            if ( piiu->echoRequestPending ) {
                piiu->echoRequestMsg ();
            }
        }
        else {
            int localError = SOCKERRNO;

            cacRingBufferReadCommit (&piiu->send, 0);

            if ( status == 0 ) {    
                piiu->shutdown ();
                break;
            }

            if ( localError == SOCK_SHUTDOWN ) {
                break;
            }

            if ( localError == SOCK_EINTR ) {
                if ( piiu->state == iiu_disconnected ) {
                    break;
                }
                else {
                    continue;
                }
            }
               
            if ( localError != SOCK_EPIPE && localError != SOCK_ECONNRESET &&
                localError != SOCK_ETIMEDOUT) {
                ca_printf ("CAC: unexpected TCP send error: %s\n", SOCKERRSTR (localError) );
            }

            piiu->shutdown ();
            break;
        }
    }

    semBinaryGive ( piiu->sendThreadExitSignal );
}

/*
 * tcpiiu::recvMsg ()
 */
void tcpiiu::recvMsg ()
{
    char            *pProto;
    unsigned        writeSpace;
    unsigned        totalBytes;
    int             status;
    
    if ( this->state != iiu_connected ) {
        return;
    }
              
    pProto = (char *) cacRingBufferWriteReserve ( &this->recv, &writeSpace );
    if ( ! pProto ) {
        return;
    }

    assert ( writeSpace <= INT_MAX );
    status = ::recv ( this->sock, pProto, (int) writeSpace, 0);
    if ( status <= 0 ) {
        int localErrno = SOCKERRNO;

        cacRingBufferWriteCommit ( &this->recv, 0 );

        if ( status == 0 ) {
            this->shutdown ();
            return;
        }

        if ( localErrno == SOCK_SHUTDOWN ) {
            return;
        }

        if ( localErrno == SOCK_EINTR ) {
            return;
        }
        
        if ( localErrno == SOCK_ECONNABORTED ) {
            return;
        }

        {
            char name[64];
            this->hostName ( name, sizeof (name) );
            ca_printf ( "Disconnecting from CA server %s because: %s\n", 
                name, SOCKERRSTR (localErrno) );
        }

        this->shutdown ();

        return;
    }
    
    assert ( ( (unsigned) status ) <= writeSpace );
    totalBytes = (unsigned) status;
    
    cacRingBufferWriteCommit (&this->recv, totalBytes);
    // cacRingBufferWriteFlush (&this->recv);

    this->messageArrivalNotify (); // reschedule connection activity watchdog

    return;
}


/*
 *  cacRecvThreadTCP ()
 */
extern "C" void cacRecvThreadTCP (void *pParam)
{
    tcpiiu *piiu = (tcpiiu *) pParam;

    piiu->connect ();
    if ( piiu->state == iiu_connected ) {
        unsigned priorityOfSelf = threadGetPrioritySelf ();
        unsigned priorityOfSend;
        threadBoolStatus tbs;
        threadId tid;

        tbs  = threadHighestPriorityLevelBelow (priorityOfSelf, &priorityOfSend);
        if ( tbs != tbsSuccess ) {
            priorityOfSend = priorityOfSelf;
        }
        tid = threadCreate ("CAC-TCP-send", priorityOfSend,
                threadGetStackSize (threadStackMedium), cacSendThreadTCP, piiu);
        if ( tid ) {
            while (1) {
                piiu->recvMsg ();
                if ( piiu->state != iiu_connected ) {
                    break;
                }
                piiu->pcas->signalRecvActivityIIU (*piiu);
            }
        }
        else {
            semBinaryGive (piiu->sendThreadExitSignal);
            piiu->shutdown ();
        }
    }
    else {
        semBinaryGive (piiu->sendThreadExitSignal);
    }
    semBinaryGive (piiu->recvThreadExitSignal);
}

//
// tcpiiu::tcpiiu ()
//
tcpiiu::tcpiiu (cac *pcac, const struct sockaddr_in &ina, unsigned minorVersion, class bhe &bheIn) :
    tcpRecvWatchdog (pcac->ca_connectTMO, *pcac->pTimerQueue, CA_V43 (CA_PROTOCOL_VERSION, minorVersion) ),
    tcpSendWatchdog (pcac->ca_connectTMO, *pcac->pTimerQueue),
    netiiu (pcac),
    bhe (bheIn)
{
    SOCKET newSocket;
    int status;
    int flag;

    newSocket = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( newSocket == INVALID_SOCKET ) {
        ca_printf ("CAC: unable to create virtual circuit because \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        this->fc = false;
        return;
    }

    //
    // apparently this is nolonger necessary with modern IP kernels
    //
#if 0
    flag = TRUE;
    status = setsockopt ( newSocket, IPPROTO_TCP, TCP_NODELAY,
                (char *)&flag, sizeof(flag) );
    if (status < 0) {
        ca_printf ("CAC: problems setting socket option TCP_NODELAY = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
    }
#endif
    flag = TRUE;
    status = setsockopt ( newSocket , SOL_SOCKET, SO_KEEPALIVE,
                (char *)&flag, sizeof (flag) );
    if (status < 0) {
        ca_printf ("CAC: problems setting socket option SO_KEEPALIVE = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
    }

#if 0
    {
        int i;

        /*
         * some concern that vxWorks will run out of mBuf's
         * if this change is made joh 11-10-98
         */        
        i = MAX_MSG_SIZE;
        status = setsockopt ( newSocket, SOL_SOCKET, SO_SNDBUF,
                (char *)&i, sizeof (i) );
        if (status < 0) {
            ca_printf ("CAC: problems setting socket option SO_SNDBUF = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
        i = MAX_MSG_SIZE;
        status = setsockopt (newSocket, SOL_SOCKET, SO_RCVBUF,
                (char *)&i, sizeof (i) );
        if (status < 0) {
            ca_printf ("CAC: problems setting socket option SO_RCVBUF = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
    }
#endif

    this->sock = newSocket;
    this->minor_version_number = minorVersion;
    this->dest.ia = ina;
    this->contiguous_msg_count = 0u;
    this->client_busy = false;
    this->claimRequestsPending = false;
    this->echoRequestPending = false;
    this->sendPending = false;
    this->pushPending = false;
    this->curDataMax = 0ul;
    this->curMsgBytes = 0ul;
    this->curDataBytes = 0ul;
    memset ( (void *) &this->curMsg, '\0', sizeof (this->curMsg) );
    this->pCurData = 0;

    status = cacRingBufferConstruct (&this->recv);
    if (status) {
        ca_printf ("CA: unable to create recv ring buffer\n");
        socket_close ( newSocket );
        this->fc = false;
        return;
    }

    status = cacRingBufferConstruct (&this->send);
    if (status) {
        ca_printf ("CA: unable to create send ring buffer\n");
        cacRingBufferDestroy (&this->recv);
        socket_close ( newSocket );
        this->fc = false;
        return;
    }

    /*
     * Save the Host name for efficient access in the
     * future.
     */
    ipAddrToA ( &this->dest.ia, this->host_name_str, sizeof (this->host_name_str) );

    /*
     * TCP starts out in the connecting state and later transitions
     * to the connected state 
     */
    this->state = iiu_connecting;

    this->sendThreadExitSignal = semBinaryCreate (semEmpty);
    if ( ! this->sendThreadExitSignal ) {
        ca_printf ("CA: unable to create CA client send thread exit semaphore\n");
        cacRingBufferDestroy (&this->recv);
        cacRingBufferDestroy (&this->send);
        socket_close ( newSocket );
        this->fc = false;
        return;
    }

    this->recvThreadExitSignal = semBinaryCreate (semEmpty);
    if ( ! this->recvThreadExitSignal ) {
        ca_printf ("CA: unable to create CA client send thread exit semaphore\n");
        semBinaryDestroy (this->sendThreadExitSignal);
        cacRingBufferDestroy (&this->recv);
        cacRingBufferDestroy (&this->send);
        socket_close ( newSocket );
        this->fc = false;
        return;
    }

    {
        unsigned priorityOfSelf = threadGetPrioritySelf ();
        unsigned priorityOfRecv;
        threadId tid;
        threadBoolStatus tbs;

        tbs  = threadHighestPriorityLevelBelow (priorityOfSelf, &priorityOfRecv);
        if ( tbs != tbsSuccess ) {
            priorityOfRecv = priorityOfSelf;
        }

        tid = threadCreate ("CAC-TCP-recv", priorityOfRecv,
                threadGetStackSize (threadStackMedium), cacRecvThreadTCP, this);
        if (tid==0) {
            ca_printf ("CA: unable to create CA client receive thread\n");
            semBinaryDestroy (this->recvThreadExitSignal);
            semBinaryDestroy (this->sendThreadExitSignal);
            cacRingBufferDestroy (&this->recv);
            cacRingBufferDestroy (&this->send);
            socket_close ( newSocket );
            this->fc = false;
            return;
        }
    }

    bhe.bindToIIU (this);
    pcac->installIIU (*this);

    this->fc = true;
}

/*
 *  tcpiiu::shutdown ()
 */
void tcpiiu::shutdown ()
{
    LOCK ( this->pcas );
    if ( this->state != iiu_disconnected ) {
        int status;

        this->state = iiu_disconnected;
        status = ::shutdown ( this->sock, SD_BOTH );
        if ( status ) {
            errlogPrintf ("CAC TCP shutdown error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
        }
        cacRingBufferShutDown ( &this->send );
        cacRingBufferShutDown ( &this->recv );
        this->pcas->signalRecvActivityIIU ( *this );
    }
    UNLOCK ( this->pcas );
}

//
// tcpiiu::~tcpiiu ()
//
tcpiiu::~tcpiiu ()
{
    unsigned chanDisconnectCount;

    if ( ! this->fc ) {
        return;
    }

    this->fc = false;

    this->shutdown ();

    LOCK (this->pcas);

    chanDisconnectCount = ellCount (&this->chidList);
    if ( chanDisconnectCount ) {
        genLocalExcep (this->pcas, ECA_DISCONN, this->host_name_str);
    }

    tsDLIterBD <nciu> iter ( this->chidList.first () );
    while ( iter != iter.eol () ) {
        tsDLIterBD<nciu> next = iter.itemAfter ();
        iter->disconnect ();
        iter = next;
    }

    UNLOCK (this->pcas);

    // wait for send and recv threads to exit
    semBinaryMustTake ( this->sendThreadExitSignal );
    semBinaryMustTake ( this->recvThreadExitSignal );
    semBinaryDestroy (this->sendThreadExitSignal);
    semBinaryDestroy (this->recvThreadExitSignal);

    this->pcas->removeIIU ( *this );

    this->pcas->removeBeaconInetAddr ( this->dest.ia );

    if ( this->pcas->ca_fd_register_func ) {
        (*this->pcas->ca_fd_register_func) 
            ((void *)this->pcas->ca_fd_register_arg, this->sock, FALSE);
    }
    socket_close (this->sock);

    cacRingBufferDestroy (&this->recv);
    cacRingBufferDestroy (&this->send);

    /*
     * free message body cache
     */
    if (this->pCurData) {
        free (this->pCurData);
    }
}

void tcpiiu::suicide ()
{
    delete this;
}

bool tcpiiu::compareIfTCP ( nciu &chan, const sockaddr_in &addr ) const
{
    if ( this->dest.ia.sin_addr.s_addr != addr.sin_addr.s_addr ||
        this->dest.ia.sin_port != addr.sin_port ) {

        char rej[64];

        ipAddrToA ( &addr, rej, sizeof (rej) );
        LOCK ( this->pcas );
        sprintf ( this->pcas->ca_sprintf_buf, 
                "Channel: %s Accepted: %s Rejected: %s ",
                chan.pName (), this->host_name_str, rej );
        genLocalExcep (this->pcas, ECA_DBLCHNL, this->pcas->ca_sprintf_buf);
        UNLOCK ( this->pcas );
    }
    return true;
}

void tcpiiu::flush ()
{
    if ( cacRingBufferWriteFlush ( &this->send ) ) {
        this->armSendWatchdog ();
    }
}

void tcpiiu::show ( unsigned /* level */ ) const
{
}

/*
 * tcpiiu::noopRequestMsg ()
 */
void tcpiiu::noopRequestMsg ()
{
    caHdr hdr;
    int status;

    hdr.m_cmmd = htons (CA_PROTO_NOOP);
    hdr.m_dataType = htons (0);
    hdr.m_count = htons (0);
    hdr.m_cid = htons (0);
    hdr.m_available = htons (0);
    hdr.m_postsize = 0;
    
    status = this->pushStreamMsg (&hdr, NULL, true);
    if ( status == ECA_NORMAL ) {
        this->flush ();
    }
}

/*
 * tcpiiu::echoRequestMsg ()
 */
void tcpiiu::echoRequestMsg ()
{
    caHdr       hdr;

    hdr.m_cmmd = htons (CA_PROTO_ECHO);
    hdr.m_dataType = htons (0);
    hdr.m_count = htons (0);
    hdr.m_cid = htons (0);
    hdr.m_available = htons (0);
    hdr.m_postsize = 0u;

    /*
     * If we are out of buffer space then postpone this
     * operation until later. This avoids any possibility
     * of a push pull deadlock (since this can be sent when 
     * parsing the UDP input buffer).
     */
    if ( this->pushStreamMsg (&hdr, NULL, false) == ECA_NORMAL ) {
        this->flush ();
        this->echoRequestPending = false;
    }
    else {
        this->echoRequestPending = true;
    }
}

/*
 *  tcpiiu::busyRequestMsg ()
 */
int tcpiiu::busyRequestMsg ()
{
    caHdr hdr;
    int status;

    hdr = cacnullmsg;
    hdr.m_cmmd = htons ( CA_PROTO_EVENTS_OFF );

    status = this->pushStreamMsg ( &hdr, NULL, true );
    if ( status == ECA_NORMAL ) {
        this->flush ();
    }
    return status;
}

/*
 * tcpiiu::readyRequestMsg ()
 */
int tcpiiu::readyRequestMsg ()
{
    caHdr hdr;
    int status;

    hdr = cacnullmsg;
    hdr.m_cmmd = htons (CA_PROTO_EVENTS_ON);
    
    status = this->pushStreamMsg (&hdr, NULL, true);
    if ( status == ECA_NORMAL ) {
        this->flush ();
    }
    return status;
}

/*
 * tcpiiu::hostNameSetMsg ()
 */
void tcpiiu::hostNameSetMsg ()
{
    unsigned    size;
    caHdr       hdr;
    char        *pName;

    if ( ! CA_V41(CA_PROTOCOL_VERSION, this->minor_version_number) ) {
        return;
    }

    /*
     * allocate space in the outgoing buffer
     */
    pName = this->pcas->ca_pHostName, 
    size = strlen (pName) + 1;
    hdr = cacnullmsg;
    hdr.m_cmmd = htons (CA_PROTO_HOST_NAME);
    hdr.m_postsize = size;
    
    this->pushStreamMsg (&hdr, pName, true);

    return;
}

/*
 * tcpiiu::userNameSetMsg ()
 */
void tcpiiu::userNameSetMsg ()
{
    unsigned    size;
    caHdr       hdr;
    char        *pName;

    if ( ! CA_V41(CA_PROTOCOL_VERSION, this->minor_version_number) ) {
        return;
    }

    /*
     * allocate space in the outgoing buffer
     */
    pName = this->pcas->ca_pUserName, 
    size = strlen (pName) + 1;
    hdr = cacnullmsg;
    hdr.m_cmmd = htons ( CA_PROTO_CLIENT_NAME );
    hdr.m_postsize = size;
    
    this->pushStreamMsg ( &hdr, pName, true );

    return;
}


/*
 * tcp_noop_action ()
 */
LOCAL void tcp_noop_action (tcpiiu * /* piiu */)
{
    return;
}

/*
 * echo_resp_action ()
 */
LOCAL void echo_resp_action (tcpiiu *piiu)
{
    return;
}

/*
 * write_notify_resp_action ()
 */
LOCAL void write_notify_resp_action (tcpiiu *piiu)
{
    baseNMIU *monix;

    LOCK (piiu->pcas);
    monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
    if (monix) {
        int status = ntohl (piiu->curMsg.m_cid);
        if ( status == ECA_NORMAL ) {
            monix->completionNotify ();
        }
        else {
            monix->exceptionNotify ( status, "write notify request rejected" );
        }
        monix->destroy ();
    }
    UNLOCK (piiu->pcas);
}

/*
 * read_notify_resp_action ()
 */
LOCAL void read_notify_resp_action ( tcpiiu *piiu )
{
    baseNMIU *monix;

    LOCK (piiu->pcas);
    monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );

    if (monix) {
        int v41;
        int status;

        /*
         * convert the data buffer from net
         * format to host format
         */
#       ifdef CONVERSION_REQUIRED 
            if (piiu->curMsg.m_dataType<NELEMENTS(cac_dbr_cvrt)) {
                (*cac_dbr_cvrt[piiu->curMsg.m_dataType])(
                     piiu->pCurData, 
                     piiu->pCurData, 
                     FALSE,
                     piiu->curMsg.m_count);
            }
            else {
                piiu->curMsg.m_cid = htonl(ECA_BADTYPE);
            }
#       endif

        /*
         * the channel id field is abused for
         * read notify status starting
         * with CA V4.1
         */
        v41 = CA_V41 (CA_PROTOCOL_VERSION, piiu->minor_version_number);
        if (v41) {
            status = ntohl (piiu->curMsg.m_cid);
        }
        else{
            status = ECA_NORMAL;
        }

        if ( status == ECA_NORMAL ) {
            monix->completionNotify (piiu->curMsg.m_dataType, piiu->curMsg.m_count, piiu->pCurData);
        }
        else {
            monix->exceptionNotify (status, "read failed", piiu->curMsg.m_dataType, piiu->curMsg.m_count);
        }
        monix->destroy ();
    }

    UNLOCK (piiu->pcas);

    return;
}

/*
 * event_resp_action ()
 */
LOCAL void event_resp_action (tcpiiu *piiu)
{
    baseNMIU *monix;

    /*
     * run the user's event handler 
     */
    LOCK (piiu->pcas);
    monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
    if ( monix ) {
        int v41;
        int status;

        /*
         * m_postsize = 0 used to be a confirmation, but is
         * now a noop because the above hash lookup will 
         * not find a matching IO block
         */
        if ( ! piiu->curMsg.m_postsize ) {
            monix->destroy ();
            UNLOCK (piiu->pcas);
            return;
        }

        /*
         * convert the data buffer from net
         * format to host format
         */
#       ifdef CONVERSION_REQUIRED 
            if (piiu->curMsg.m_dataType<NELEMENTS(cac_dbr_cvrt)) {
                (*cac_dbr_cvrt[piiu->curMsg.m_dataType])(
                     piiu->pCurData, 
                     piiu->pCurData, 
                     FALSE,
                     piiu->curMsg.m_count);
            }
            else {
                piiu->curMsg.m_cid = htonl(ECA_BADTYPE);
            }
#       endif

        /*
         * the channel id field is abused for
         * read notify status starting
         * with CA V4.1
         */
        v41 = CA_V41 (CA_PROTOCOL_VERSION, piiu->minor_version_number);
        if (v41) {
            status = ntohl (piiu->curMsg.m_cid);
        }
        else {
            status = ECA_NORMAL;
        }
        if ( status == ECA_NORMAL ) {
            monix->completionNotify ( piiu->curMsg.m_dataType, 
                piiu->curMsg.m_count, piiu->pCurData );
        }
        else {
            monix->exceptionNotify ( status, "subscription update failed", 
                            piiu->curMsg.m_dataType, piiu->curMsg.m_count );
        }
    }

    UNLOCK (piiu->pcas);

    return;
}

/*
 * read_resp_action ()
 */
LOCAL void read_resp_action (tcpiiu *piiu)
{
    baseNMIU *pIOBlock;

    LOCK (piiu->pcas);

    /*
     * verify the event id
     */
    pIOBlock = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
    if ( pIOBlock ) {

        /*
         * convert the data buffer from net
         * format to host format
         */
        pIOBlock->completionNotify (piiu->curMsg.m_dataType, piiu->curMsg.m_count, piiu->pCurData);
        pIOBlock->destroy ();
    }

    UNLOCK (piiu->pcas);

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
    baseNMIU *monix;
    char context[255];
    caHdr *req = (caHdr *) piiu->pCurData;

    if ( piiu->curMsg.m_postsize > sizeof (caHdr) ){
        sprintf (context, "detected by: %s for: %s", 
            piiu->host_name_str, (char *)(req+1));
    }
    else{
        sprintf (context, "detected by: %s", piiu->host_name_str);
    }

    LOCK (piiu->pcas);
    switch ( ntohs (req->m_cmmd) ) {
    case CA_PROTO_READ_NOTIFY:
        monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
        if (monix) {
            monix->exceptionNotify ( ntohl (piiu->curMsg.m_available), context, 
                ntohs (req->m_dataType), ntohs (req->m_count) );
            monix->destroy ();
        }
        else {
            piiu->pcas->exceptionNotify (ntohl (piiu->curMsg.m_available), 
                context, __FILE__, __LINE__);
        }
        break;
    case CA_PROTO_READ:
        monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
        if (monix) {
            monix->exceptionNotify ( ntohl (piiu->curMsg.m_available), context, 
                ntohs (req->m_dataType), ntohs (req->m_count) );
            monix->destroy ();
        }
        else {
            piiu->pcas->exceptionNotify (ntohl (piiu->curMsg.m_available), 
                context, __FILE__, __LINE__);
        }
        break;
    case CA_PROTO_WRITE_NOTIFY:
        monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
        if (monix) {
            monix->exceptionNotify (ntohl (piiu->curMsg.m_available), context);
            monix->destroy ();
        }
        else {
            piiu->pcas->exceptionNotify (ntohl ( piiu->curMsg.m_available), 
                context, __FILE__, __LINE__);
        }
        break;
    case CA_PROTO_WRITE:
        piiu->pcas->exceptionNotify (ntohl ( piiu->curMsg.m_available), 
                context, ntohs (req->m_dataType), ntohs (req->m_count), __FILE__, __LINE__);
        break;
    case CA_PROTO_EVENT_ADD:
        monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
        if (monix) {
            monix->exceptionNotify (ntohl (piiu->curMsg.m_available), context);
            monix->destroy ();
        }
        else {
            piiu->pcas->exceptionNotify (ntohl (piiu->curMsg.m_available), 
                context, __FILE__, __LINE__);
        }
        break;
    case CA_PROTO_EVENT_CANCEL:
        monix = piiu->pcas->lookupIO ( piiu->curMsg.m_available );
        if (monix) {
            monix->exceptionNotify (ntohl (piiu->curMsg.m_available), context);
            monix->destroy ();
        }
        else {
            piiu->pcas->exceptionNotify (ntohl (piiu->curMsg.m_available), 
                context, __FILE__, __LINE__);
        }
        break;
    default:
        piiu->pcas->exceptionNotify (ntohl (piiu->curMsg.m_available), 
            context, __FILE__, __LINE__);
        break;
    }

    UNLOCK (piiu->pcas);

    return;
}

/*
 * access_rights_resp_action ()
 */
LOCAL void access_rights_resp_action (tcpiiu *piiu)
{
    int ar;
    nciu *chan;

    LOCK (piiu->pcas);
    chan = piiu->pcas->lookupChan (piiu->curMsg.m_cid);
    if ( ! chan ) {
        /*
         * end up here if they delete the channel
         * prior to connecting
         */
        UNLOCK (piiu->pcas);
        return;
    }

    ar = ntohl (piiu->curMsg.m_available);
    chan->ar.read_access = (ar&CA_PROTO_ACCESS_RIGHT_READ)?1:0;
    chan->ar.write_access = (ar&CA_PROTO_ACCESS_RIGHT_WRITE)?1:0;

    chan->accessRightsNotify (chan->ar);

    UNLOCK (piiu->pcas);
    return;
}

/*
 * claim_ciu_resp_action ()
 */
LOCAL void claim_ciu_resp_action (tcpiiu *piiu)
{
    unsigned sid;
    nciu *chan;

    LOCK (piiu->pcas);

    chan = piiu->pcas->lookupChan (piiu->curMsg.m_cid);
    if ( ! chan ) {
        UNLOCK (piiu->pcas);
        return;
    }

    if ( CA_V44 (CA_PROTOCOL_VERSION, piiu->minor_version_number) ) {
        sid = piiu->curMsg.m_available;
    }
    else {
        sid = chan->sid;
    }

    chan->connect (*piiu, piiu->curMsg.m_dataType, piiu->curMsg.m_count, sid);

    UNLOCK (piiu->pcas);

    return;
}

/*
 * verifyAndDisconnectChan ()
 */
LOCAL void verifyAndDisconnectChan (tcpiiu *piiu)
{
    nciu *chan;

    LOCK (piiu->pcas);
    chan = piiu->pcas->lookupChan (piiu->curMsg.m_cid);
    if (!chan) {
        /*
         * end up here if they delete the channel
         * prior to this response 
         */
        UNLOCK (piiu->pcas);
        return;
    }

    /*
     * need to move the channel back to the cast niiu
     * (so we will be able to reconnect)
     *
     * this marks the niiu for disconnect if the channel 
     * count goes to zero
     */
    chan->disconnect ();
    UNLOCK (piiu->pcas);
    return;
}

/*
 * bad_tcp_resp_action ()
 */
LOCAL void bad_tcp_resp_action (tcpiiu *piiu)
{
    ca_printf ("CAC: Bad response code in TCP message from %s was %u\n", 
        piiu->host_name_str, piiu->curMsg.m_cmmd);
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
    bad_tcp_resp_action,
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
 * post_tcp_msg ()
 *
 * LOCK should be applied when calling this routine
 *
 */
int tcpiiu::post_msg (char *pInBuf, unsigned long blockSize)
{
    unsigned long size;

    while ( blockSize ) {

        /*
         * fetch a complete message header
         */
        if ( this->curMsgBytes < sizeof (this->curMsg) ) {
            char  *pHdr;

            size = sizeof (this->curMsg) - this->curMsgBytes;
            size = min (size, blockSize);
            
            pHdr = (char *) &this->curMsg;
            memcpy ( pHdr + this->curMsgBytes, pInBuf, size);
            
            this->curMsgBytes += size;
            if ( this->curMsgBytes < sizeof (this->curMsg) ) {
#if 0 
                printf ("waiting for %d msg hdr bytes\n", 
                    sizeof(this->curMsg) - this->curMsgBytes);
#endif
                return ECA_NORMAL;
            }

            pInBuf += size;
            blockSize -= size;
        
            /* 
             * fix endian of bytes 
             */
            this->curMsg.m_postsize = ntohs (this->curMsg.m_postsize);
            this->curMsg.m_cmmd = ntohs (this->curMsg.m_cmmd);
            this->curMsg.m_dataType = ntohs (this->curMsg.m_dataType);
            this->curMsg.m_count = ntohs (this->curMsg.m_count);
#if 0
            ca_printf (
                "%s Cmd=%3d Type=%3d Count=%4d Size=%4d",
                this->host_name_str,
                this->curMsg.m_cmmd,
                this->curMsg.m_dataType,
                this->curMsg.m_count,
                this->curMsg.m_postsize);
            ca_printf (
                " Avail=%8x Cid=%6d\n",
                this->curMsg.m_available,
                this->curMsg.m_cid);
#endif
        }

        /*
         * dont allow huge msg body until
         * the server supports it
         */
        if ( this->curMsg.m_postsize > (unsigned) MAX_TCP ) {
            this->curMsgBytes = 0;
            this->curDataBytes = 0;
            return ECA_TOLARGE;
        }

        /*
         * make sure we have a large enough message body cache
         */
        if ( this->curMsg.m_postsize > this->curDataMax ) {
            void *pData;
            size_t cacheSize;

            /* 
             * scalar DBR_STRING is sometimes clipped to the
             * actual string size so make sure this cache is
             * as large as one DBR_STRING so they will
             * not page fault if they read MAX_STRING_SIZE
             * bytes (instead of the actual string size).
             */
            cacheSize = max ( this->curMsg.m_postsize, MAX_STRING_SIZE );
            pData = (void *) calloc (1u, cacheSize);
            if (!pData) {
                return ECA_ALLOCMEM;
            }
            if (this->pCurData) {
                free (this->pCurData);
            }
            this->pCurData = pData;
            this->curDataMax = this->curMsg.m_postsize;
        }

        /*
         * Fetch a complete message body
         * (allows for arrays larger than than the
         * ring buffer size)
         */
        if (this->curMsg.m_postsize > this->curDataBytes ) {
            char *pBdy;

            size = this->curMsg.m_postsize - this->curDataBytes; 
            size = min (size, blockSize);
            pBdy = (char *) this->pCurData;
            memcpy ( pBdy + this->curDataBytes, pInBuf, size);
            this->curDataBytes += size;
            if (this->curDataBytes < this->curMsg.m_postsize) {
#if 0
                printf ("waiting for %d msg bdy bytes\n", 
                    this->curMsg.m_postsize - this->curDataBytes);
#endif
                return ECA_NORMAL;
            }
            pInBuf += size;
            blockSize -= size;
        }   

        /*
         * execute the response message
         */
        pProtoStubTCP      pStub;
        if ( this->curMsg.m_cmmd >= NELEMENTS (tcpJumpTableCAC) ) {
            pStub = bad_tcp_resp_action;
        }
        else {
            pStub = tcpJumpTableCAC [this->curMsg.m_cmmd];
        }
        (*pStub) (this);
         
        this->curMsgBytes = 0;
        this->curDataBytes = 0;
    }
    return ECA_NORMAL;
}

void tcpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, this->host_name_str, bufLength );
        pBuf[bufLength - 1u] = '\0';
    }
}

bool tcpiiu::ca_v42_ok () const
{
    return CA_V42 (CA_PROTOCOL_VERSION,
                this->minor_version_number);
}

bool tcpiiu::ca_v41_ok () const
{
    return CA_V41 (CA_PROTOCOL_VERSION,
                this->minor_version_number);
}

/*
 *  tcpiiu::pushStreamMsg ()
 */ 
int tcpiiu::pushStreamMsg (const caHdr *pmsg, 
                                   const void *pext, bool BlockingOk)
{
    caHdr           msg;
    ca_uint16_t     actualextsize;
    ca_uint16_t     extsize;
    unsigned        msgsize;
    unsigned        bytesSent;

    if ( pext == NULL ) {
        extsize = actualextsize = 0;
    }
    else {
        if ( pmsg->m_postsize > 0xffff-7 ) {
            return ECA_TOLARGE;
        }
        actualextsize = pmsg->m_postsize;
        extsize = CA_MESSAGE_ALIGN (actualextsize);
    }

    msg = *pmsg;
    msg.m_postsize = htons (extsize);
    msgsize = extsize + sizeof (msg);

    if ( ! cacRingBufferWriteLockNoBlock ( &this->send, msgsize ) ) {
        if ( BlockingOk ) {
            this->armSendWatchdog ();
            cacRingBufferWriteLock ( &this->send );
        }
        else {
            return ECA_OPWILLBLOCK;
        }
    }

    /*
     * push the header onto the ring 
     */
    bytesSent = cacRingBufferWrite ( &this->send, &msg, sizeof (msg) );
    if ( bytesSent != sizeof (msg) ) {
        cacRingBufferWriteUnlock ( &this->send );
        return ECA_DISCONNCHID;
    }

    /*
     * push message body onto the ring
     *
     * (optionally encode in network format as we send)
     */
    if ( extsize > 0u ) {
        bytesSent = cacRingBufferWrite ( &this->send, pext, actualextsize );
        if ( bytesSent != actualextsize ) {
            cacRingBufferWriteUnlock ( &this->send );
            return ECA_DISCONNCHID;
        }
        /*
         * force pad bytes at the end of the message to nill
         * if present (this avoids messages from purify)
         */
        {
            unsigned long n;

            n = extsize-actualextsize;
            if (n) {
                assert ( n <= sizeof (nullBuff) );
                bytesSent = cacRingBufferWrite ( &this->send, nullBuff, n );
                if ( bytesSent != n ) {
                    cacRingBufferWriteUnlock ( &this->send );
                    return ECA_DISCONNCHID;
                }
            }
        }
    }

    cacRingBufferWriteUnlock ( &this->send );

    return ECA_NORMAL;
}

int tcpiiu::pushDatagramMsg (const caHdr * /* pMsg */, 
                   const void * /* pExt */, ca_uint16_t /* extsize */)
{
    return ECA_INTERNAL;
}

/*
 * add to the beginning of the list until we
 * have sent the claim message (after which we
 * move it to the end of the list)
 */
void tcpiiu::addToChanList (nciu *chan)
{
    LOCK (this->pcas);
    chan->claimPending = TRUE;
    this->chidList.push (*chan);
    chan->piiu = this;
    UNLOCK (this->pcas);
}

void tcpiiu::removeFromChanList (nciu *chan)
{
    LOCK (this->pcas);
    this->chidList.remove (*chan);
    chan->piiu = NULL;
    UNLOCK (this->pcas);

    if ( this->chidList.count () == 0 ) {
        this->shutdown ();
    }
}

void tcpiiu::disconnect (nciu *chan)
{
    LOCK (this->pcas);
    this->removeFromChanList (chan);
    /*
     * try to reconnect
     */
    assert (this->pcas->pudpiiu);
    this->pcas->pudpiiu->addToChanList ( chan );
    this->pcas->pudpiiu->searchTmr.reset ( CA_RECAST_DELAY );
    UNLOCK (this->pcas);
}
