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

/*  Allocate storage for global variables in this module        */
#define     CA_GLBLSOURCE
#include    "iocinf.h"
#include    "net_convert.h"
#include    "locationException.h"
#include    "osiProcess.h"

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
    nciu *chan;

    LOCK (piiu->niiu.iiu.pcas);
    while ( (chan = (nciu *) ellFirst (&piiu->niiu.chidList)) ) {
        if (!chan->claimPending) {
            piiu->claimsPending = FALSE;
            break;
        }
        success = issue_claim_channel (chan);
        if (!success) {
            break;
        }
    }
    UNLOCK (piiu->niiu.iiu.pcas);
}

/*
 * cac_connect_tcp ()
 */
LOCAL void cac_connect_tcp (tcpiiu *piiu)
{
    int status;
        
    /* 
     * attempt to connect to a CA server
     */
    while (1) {
        int errnoCpy;

        status = connect ( piiu->sock, &piiu->dest.sa,
                sizeof (piiu->dest.sa) );
        if (status == 0) {
            break;
        }

        errnoCpy = SOCKERRNO;
        if (errnoCpy==SOCK_EISCONN) {
            /*
             * called connect after we are already connected 
             * (this appears to be how they provide 
             * connect completion notification)
             */
            break;
        }
        else if (
            errnoCpy==SOCK_EINPROGRESS ||
            errnoCpy==SOCK_EWOULDBLOCK /* for WINSOCK */
            ) {
            /*
             * The  socket  is   non-blocking   and   a
             * connection attempt has been initiated,
             * but not completed.
             */
            return;
        }
        else if (errnoCpy==SOCK_EALREADY) {
            return; 
        }
#ifdef _WIN32
        /*
         * including this with vxWorks appears to
         * cause trouble
         */
        else if (errnoCpy==SOCK_EINVAL) { /* a SOCK_EALREADY alias used by early WINSOCK */
            return; 
        }
#endif
        else if(errnoCpy==SOCK_EINTR) {
            /*
             * restart the system call if interrupted
             */
            continue;
        }
        else {  
            initiateShutdownTCPIIU (piiu);
            ca_printf (piiu->niiu.iiu.pcas,
    "CAC: Unable to connect to \"%s\" because %d=\"%s\"\n", 
                piiu->host_name_str, errnoCpy, SOCKERRSTR(errnoCpy));
            return;
        }
    }

    /*
     * put the iiu into the connected state
     */
    piiu->state = iiu_connected;

    status = tsStampGetCurrent (&piiu->timeAtLastRecv);
    assert (status==0);

    return;
}

/*
 *  constructNIIU ()
 */
void constructNIIU (cac *pcac, netIIU *piiu)
{
    piiu->iiu.pcas = pcac;
    ellInit (&piiu->chidList);

    piiu->curDataMax = 0u;
    piiu->curMsgBytes = 0u;
    piiu->curDataBytes = 0u;
    memset ( (void *) &piiu->curMsg, '\0', sizeof (piiu->curMsg) );
    piiu->pCurData = 0;
}

/*
 *  destroyNIIU ()
 */
LOCAL void destroyNIIU (netIIU *piiu)
{
    nciu *pChan, *pNext;

    pChan = (nciu *) ellFirst (&piiu->chidList);
    while (pChan) {
        pNext = (nciu *) ellNext (&pChan->node);
        /* channel is destroyed here if it is disconnected */
        cacDisconnectChannel (pChan);
        pChan = pNext;
    }

    /*
     * free message body cache
     */
    if (piiu->pCurData) {
        free (piiu->pCurData);
    }
}

/*
 * cac_tcp_recv_msg ()
 */
LOCAL void cac_tcp_recv_msg (tcpiiu *piiu)
{
    char            *pProto;
    unsigned        writeSpace;
    unsigned        totalBytes;
    int             status;
    
    if ( piiu->state != iiu_connected ) {
        return;
    }
              
    pProto = (char *) cacRingBufferWriteReserve (&piiu->recv, &writeSpace);
    
    assert (writeSpace<=INT_MAX);
    status = recv ( piiu->sock, pProto, (int) writeSpace, 0);
    if ( status <= 0 ) {
        int localErrno = SOCKERRNO;

        cacRingBufferWriteCommit (&piiu->recv, 0);

        if (status == 0) {
            initiateShutdownTCPIIU (piiu);
            return;
        }

        if (localErrno == SOCK_SHUTDOWN) {
            return;
        }

        if ( localErrno == SOCK_EWOULDBLOCK || localErrno == SOCK_EINTR ) {
            return;
        }
        
        if ( localErrno != SOCK_EPIPE && 
            localErrno != SOCK_ECONNRESET &&
            localErrno != SOCK_ETIMEDOUT){
            ca_printf (piiu->niiu.iiu.pcas, 
                "CAC: unexpected TCP recv error: %s\n", SOCKERRSTR(localErrno));
        }
        initiateShutdownTCPIIU (piiu);
        return;
    }
    
    assert ( ( (unsigned) status ) <= writeSpace );
    totalBytes = (unsigned) status;
    
    cacRingBufferWriteCommit (&piiu->recv, totalBytes);
    cacRingBufferWriteFlush (&piiu->recv);

    status = tsStampGetCurrent (&piiu->timeAtLastRecv);
    assert (status==0);    

    return;
}

/*
 *  cacSendThreadTCP ()
 */
LOCAL void cacSendThreadTCP (void *pParam)
{
    tcpiiu *piiu = (tcpiiu *) pParam;

    while (1) {
        unsigned    sendCnt;
        char        *pOutBuf;

        pOutBuf = (char *) cacRingBufferReadReserve (&piiu->send, &sendCnt);
        if (pOutBuf) {
            int status;

            assert ( sendCnt <= INT_MAX );
            status = send ( piiu->sock, pOutBuf, (int) sendCnt, 0);
            if (status>0) {
                cacRingBufferReadCommit (&piiu->send, (unsigned long) status);
                cacRingBufferReadFlush (&piiu->send);
                if (piiu->claimsPending) {
                    retryPendingClaims (piiu);
                }
            }
            else {
                int localError = SOCKERRNO;

                cacRingBufferReadCommit (&piiu->send, 0);

                if (status==0) {
                    initiateShutdownTCPIIU (piiu);
                    break;
                }

                if (localError == SOCK_SHUTDOWN) {
                    break;
                }

                if ( localError != SOCK_EWOULDBLOCK && localError != SOCK_EINTR ) {
                    if ( localError != SOCK_EPIPE && localError != SOCK_ECONNRESET &&
                        localError != SOCK_ETIMEDOUT) {
                        ca_printf ( piiu->niiu.iiu.pcas, "CAC: unexpected TCP send error: %s\n",
                            SOCKERRSTR (localError) );
                    }

                    initiateShutdownTCPIIU (piiu);
                    break;
                }
            }
        }
        else if ( piiu->state != iiu_connected ) {
            break;
        }
    }

    semBinaryGive (piiu->sendThreadExitSignal);
}

/*
 *  cacRecvThreadTCP ()
 */
LOCAL void cacRecvThreadTCP (void *pParam)
{
    tcpiiu *piiu = (tcpiiu *) pParam;
    unsigned chanDisconnectCount;

    cac_connect_tcp (piiu);
    if ( piiu->state == iiu_connected ) {
        threadId tid;

        tid = threadCreate ("CAC TCP Send", threadPriorityChannelAccessClient,
                threadGetStackSize (threadStackMedium), cacSendThreadTCP, piiu);
        if (tid) {
            while (1) {
                cac_tcp_recv_msg (piiu);
                if ( piiu->state != iiu_connected ) {
                    break;
                }
                piiu->niiu.iiu.pcas->procThread.installLabor (*piiu);
            }
        }
    }

    semBinaryMustTake (piiu->sendThreadExitSignal);

    piiu->niiu.iiu.pcas->procThread.removeLabor (*piiu);

    LOCK (piiu->niiu.iiu.pcas);

    chanDisconnectCount = ellCount (&piiu->niiu.chidList);
    if (chanDisconnectCount) {
        genLocalExcep (piiu->niiu.iiu.pcas, ECA_DISCONN, piiu->host_name_str);
    }

    removeBeaconInetAddr (piiu->niiu.iiu.pcas, &piiu->dest.ia);

    ellDelete (&piiu->niiu.iiu.pcas->ca_iiuList, &piiu->node);

    destroyNIIU (&piiu->niiu);

    if (piiu->niiu.iiu.pcas->ca_fd_register_func) {
        (*piiu->niiu.iiu.pcas->ca_fd_register_func) 
            ((void *)piiu->niiu.iiu.pcas->ca_fd_register_arg, piiu->sock, FALSE);
    }
    cacRingBufferDestroy (&piiu->recv);
    cacRingBufferDestroy (&piiu->send);
    socket_close (piiu->sock);
    semBinaryDestroy (piiu->sendThreadExitSignal);

    semBinaryGive (piiu->recvThreadExitSignal);

    UNLOCK (piiu->niiu.iiu.pcas);

    free (piiu);
}

/*
 * constructTCPIIU ()
 * (lock must be applied by caller)
 */
tcpiiu * constructTCPIIU (cac *pcac, const struct sockaddr_in *pina, 
                                     unsigned minorVersion)
{
    SOCKET sock;
    tcpiiu *piiu;
    int status;
    int flag;
    threadId tid;
    bhe *pBHE;

    /*
     * look for an existing virtual circuit
     */
    pBHE = lookupBeaconInetAddr (pcac, pina);
    if (!pBHE) {
        pBHE = createBeaconHashEntry (pcac, pina, FALSE);
        if (!pBHE){
            UNLOCK (pcac);
            return NULL;
        }
    }
    
    if (pBHE->piiu) {
        if ( pBHE->piiu->state == iiu_connecting || 
            pBHE->piiu->state == iiu_connected ) {
            return pBHE->piiu;
        }
        else {
            return NULL;
        }
    }

    sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if (sock == INVALID_SOCKET) {
        ca_printf (pcac, "CAC: unable to create virtual circuit because \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return NULL;
    }

    flag = TRUE;
    status = setsockopt ( sock, IPPROTO_TCP, TCP_NODELAY,
                (char *)&flag, sizeof(flag) );
    if (status < 0) {
        ca_printf (pcac, "CAC: problems setting socket option TCP_NODELAY = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
    }

    flag = TRUE;
    status = setsockopt ( sock, SOL_SOCKET, SO_KEEPALIVE,
                (char *)&flag, sizeof (flag) );
    if (status < 0) {
        ca_printf (pcac, "CAC: problems setting socket option SO_KEEPALIVE = \"%s\"\n",
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
        status = setsockopt ( piiu->sock, SOL_SOCKET, SO_SNDBUF,
                (char *)&i, sizeof (i) );
        if (status < 0) {
            ca_printf (pcac, "CAC: problems setting socket option SO_SNDBUF = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
        i = MAX_MSG_SIZE;
        status = setsockopt (piiu->sock, SOL_SOCKET, SO_RCVBUF,
                (char *)&i, sizeof (i) );
        if (status < 0) {
            ca_printf (pcac, "CAC: problems setting socket option SO_RCVBUF = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
    }
#endif

    piiu = (tcpiiu *) calloc (1, sizeof (*piiu) );
    if (!piiu) {
        socket_close (sock);
        return NULL;
    }

    constructNIIU (pcac, &piiu->niiu);
    piiu->sock = sock;
    piiu->minor_version_number = minorVersion;
    piiu->claimsPending = FALSE;
    piiu->recvPending = FALSE;
    piiu->pushPending = FALSE;
    piiu->beaconAnomaly = FALSE;
    piiu->dest.ia = *pina;

    status = cacRingBufferConstruct (&piiu->recv);
    if (status) {
        ca_printf (pcac, "CA: unable to create recv ring buffer\n");
        socket_close (sock);
        destroyNIIU (&piiu->niiu);
        free (piiu);
    }

    status = cacRingBufferConstruct (&piiu->send);
    if (status) {
        ca_printf (pcac, "CA: unable to create send ring buffer\n");
        cacRingBufferDestroy (&piiu->recv);
        socket_close (sock);
        destroyNIIU (&piiu->niiu);
        free (piiu);
    }

    status = tsStampGetCurrent (&piiu->timeAtLastRecv);
    assert (status==0);

    /*
     * Save the Host name for efficient access in the
     * future.
     */
    ipAddrToA (&piiu->dest.ia, piiu->host_name_str, sizeof (piiu->host_name_str) );
        
    /*
     * TCP starts out in the connecting state and later transitions
     * to the connected state 
     */
    piiu->state = iiu_connecting;

    piiu->sendThreadExitSignal = semBinaryCreate (semEmpty);
    if ( !piiu->sendThreadExitSignal ) {
        ca_printf (pcac, "CA: unable to create CA client send thread exit semaphore\n");
        cacRingBufferDestroy (&piiu->recv);
        cacRingBufferDestroy (&piiu->send);
        destroyNIIU (&piiu->niiu);
        socket_close (sock);
        free (piiu);
        return NULL;
    }

    piiu->recvThreadExitSignal = semBinaryCreate (semEmpty);
    if ( !piiu->sendThreadExitSignal ) {
        ca_printf (pcac, "CA: unable to create CA client send thread exit semaphore\n");
        semBinaryDestroy (piiu->sendThreadExitSignal);
        cacRingBufferDestroy (&piiu->recv);
        cacRingBufferDestroy (&piiu->send);
        destroyNIIU (&piiu->niiu);
        socket_close (sock);
        free (piiu);
        return NULL;
    }

    tid = threadCreate (
            "CAC TCP Recv",
            threadPriorityChannelAccessClient,
            threadGetStackSize (threadStackMedium),
            cacRecvThreadTCP,
            piiu);
    if (tid==0) {
        ca_printf (pcac, "CA: unable to create CA client receive thread\n");
        semBinaryDestroy (piiu->recvThreadExitSignal);
        semBinaryDestroy (piiu->sendThreadExitSignal);
        cacRingBufferDestroy (&piiu->recv);
        cacRingBufferDestroy (&piiu->send);
        destroyNIIU (&piiu->niiu);
        socket_close (sock);
        free (piiu);
        return NULL;
    }
    pBHE->piiu = piiu;

    ellAdd (&pcac->ca_iiuList, &piiu->node);

    if (pcac->ca_fd_register_func) {
        (*pcac->ca_fd_register_func) ((void *)pcac->ca_fd_register_arg, piiu->sock, TRUE);
    }

    return piiu;
}

/*
 *  initiateShutdownTCPIIU ()
 */
void initiateShutdownTCPIIU (tcpiiu *piiu)
{
    LOCK (piiu->niiu.iiu.pcas);
    if (piiu->state != iiu_disconnected) {
        piiu->state = iiu_disconnected;
        shutdown (piiu->sock, SD_BOTH);
        cacRingBufferShutDown (&piiu->send);
        cacRingBufferShutDown (&piiu->recv);
    }
    UNLOCK (piiu->niiu.iiu.pcas);
}

/*
 *  cac_udp_recv_msg ()
 */
LOCAL int cac_udp_recv_msg (udpiiu *piiu)
{
    osiSockAddr         src;
    int                 src_size = sizeof (src);
    int                 status;

    status = recvfrom (piiu->sock, piiu->recvBuf, sizeof (piiu->recvBuf), 0,
                        &src.sa, &src_size);
    if (status < 0) {
        int errnoCpy = SOCKERRNO;

        if (errnoCpy == SOCK_SHUTDOWN) {
            return -1;
        }
        if (errnoCpy == SOCK_EWOULDBLOCK || errnoCpy == SOCK_EINTR) {
            return 0;
        }
#       ifdef linux
            /*
             * Avoid spurious ECONNREFUSED bug
             * in linux
             */
            if (errnoCpy==SOCK_ECONNREFUSED) {
                return 0;
            }
#       endif
        ca_printf (piiu->niiu.iiu.pcas,
            "Unexpected UDP recv error %s\n", SOCKERRSTR(errnoCpy));
    }
    else if (status > 0) {
        
        status = post_msg (&piiu->niiu, &src.ia,
                    piiu->recvBuf, (unsigned long) status);
        if ( status!=ECA_NORMAL || piiu->niiu.curMsgBytes ) {
            char buf[64];

            ipAddrToA (&src.ia, buf, sizeof(buf));

            ca_printf (piiu->niiu.iiu.pcas,
                "%s: bad UDP msg from %s\n", __FILE__, buf);

            /*
             * resync the ring buffer
             * (discard existing messages)
             */
            piiu->niiu.curMsgBytes = 0;
            piiu->niiu.curDataBytes = 0;
            return 0;
        }
    }
    
    return 0;
}

/*
 *  cacRecvThreadUDP ()
 */
LOCAL void cacRecvThreadUDP (void *pParam)
{
    udpiiu *piiu = (udpiiu *) pParam;
    int status;

    do {
        status = cac_udp_recv_msg (piiu);
    } while ( status == 0 );

    semBinaryGive (piiu->recvThreadExitSignal);
}

/*
 *  NOTIFY_CA_REPEATER()
 *
 *  tell the cast repeater that another client needs fan out
 *
 *  NOTES:
 *  1)  local communication only (no LAN traffic)
 *
 */
void notify_ca_repeater (udpiiu *piiu)
{
    caHdr msg;
    osiSockAddr saddr;
    int status;
    static int once = FALSE;
    int len;

    if (piiu->repeaterContacted) {
        return;
    }

    if (piiu->repeaterTries > N_REPEATER_TRIES_PRIOR_TO_MSG ) {
        if (!once) {
            ca_printf (piiu->niiu.iiu.pcas,
        "Unable to contact CA repeater after %d tries\n",
            N_REPEATER_TRIES_PRIOR_TO_MSG);
            ca_printf (piiu->niiu.iiu.pcas,
        "Silence this message by starting a CA repeater daemon\n");
            once = TRUE;
        }
    }

    /*
     * In 3.13 beta 11 and before the CA repeater calls local_addr() 
     * to determine a local address and does not allow registration 
     * messages originating from other addresses. In these 
     * releases local_addr() returned the address of the first enabled
     * interface found, and this address may or may not have been the loop
     * back address. Starting with 3.13 beta 12 local_addr() was
     * changed to always return the address of the first enabled 
     * non-loopback interface because a valid non-loopback local
     * address is required in the beacon messages. Therefore, to 
     * guarantee compatibility with past versions of the repeater
     * we alternate between the address returned by local_addr()
     * and the loopback address here.
     *
     * CA repeaters in R3.13 beta 12 and higher allow
     * either the loopback address or the address returned
     * by local address (the first non-loopback address found)
     */
    if (piiu->repeaterTries&1) {
        saddr = osiLocalAddr (piiu->sock);
        if (saddr.sa.sa_family != AF_INET) {
            /*
             * use the loop back address to communicate with the CA repeater
             * if this os does not have interface query capabilities
             *
             * this will only work with 3.13 beta 12 CA repeaters or later
             */
            saddr.ia.sin_family = AF_INET;
            saddr.ia.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
            saddr.ia.sin_port = htons (piiu->repeaterPort);   
        }
    }
    else {
        saddr.ia.sin_family = AF_INET;
        saddr.ia.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
        saddr.ia.sin_port = htons (piiu->repeaterPort);   
    }

    memset ((char *)&msg, 0, sizeof(msg));
    msg.m_cmmd = htons (REPEATER_REGISTER);
    msg.m_available = saddr.ia.sin_addr.s_addr;

    /*
     * Intentionally sending a zero length message here
     * until most CA repeater daemons have been restarted
     * (and only then will accept the above protocol)
     * (repeaters began accepting this protocol
     * starting with EPICS 3.12)
     *
     * SOLARIS will not accept a zero length message
     * and we are just porting there for 3.12 so
     * we will use the new protocol for 3.12
     *
     * recent versions of UCX will not accept a zero 
     * length message and we will assume that folks
     * using newer versions of UCX have rebooted (and
     * therefore restarted the CA repeater - and therefore
     * moved it to an EPICS release that accepts this protocol)
     */
#   if defined (DOES_NOT_ACCEPT_ZERO_LENGTH_UDP)
        len = sizeof (msg);
#   else 
        len = 0;
#   endif 

    status = sendto (piiu->sock, (char *)&msg, len,  
                        0, (struct sockaddr *)&saddr, sizeof(saddr));
    if (status < 0) {
        int errnoCpy = SOCKERRNO;
        if( errnoCpy != SOCK_EINTR && 
            errnoCpy != SOCK_EWOULDBLOCK &&
            /*
             * This is returned from Linux when
             * the repeater isnt running
             */
            errnoCpy != SOCK_ECONNREFUSED 
            ) {
            ca_printf (piiu->niiu.iiu.pcas,
                "CAC: error sending to repeater was \"%s\"\n", 
                SOCKERRSTR(errnoCpy));
        }
    }
    piiu->repeaterTries++;
    piiu->contactRepeater = 0u;
}

/*
 *  cacSendThreadUDP ()
 */
LOCAL void cacSendThreadUDP (void *pParam)
{
    udpiiu *piiu = (udpiiu *) pParam;

    while (1) {
        int status;

        if (piiu->contactRepeater) {
            notify_ca_repeater (piiu);
        }

        semMutexMustTake (piiu->xmitBufLock);

        if (piiu->nBytesInXmitBuf > 0) {
            osiSockAddrNode  *pNode;

            pNode = (osiSockAddrNode *) ellFirst (&piiu->dest);
            while (pNode) {

                assert ( piiu->nBytesInXmitBuf <= INT_MAX );
                status = sendto (piiu->sock, piiu->xmitBuf,   
                        (int) piiu->nBytesInXmitBuf, 0, 
                        &pNode->addr.sa, sizeof(pNode->addr.sa));
                if (status <= 0) {
                    int localErrno = SOCKERRNO;

                    if (status==0) {
                        break;
                    }

                    if (localErrno == SOCK_SHUTDOWN) {
                        break;
                    }
                    else if ( localErrno == SOCK_EINTR ) {
                        status = 1;
                    }
                    else {
                        char buf[64];

                        ipAddrToA (&pNode->addr.ia, buf, sizeof (buf));

                        ca_printf (piiu->niiu.iiu.pcas,
                            "CAC: error = \"%s\" sending UDP msg to %s\n",
                            SOCKERRSTR(localErrno), buf);

                        break;
                    }
                }
                pNode = (osiSockAddrNode *) ellNext (&pNode->node);
            }

            piiu->nBytesInXmitBuf = 0u;

            if (status<=0) {
                break;
            }
        }

        semMutexGive (piiu->xmitBufLock);

        semBinaryMustTake (piiu->xmitSignal);
    }

    semBinaryGive (piiu->sendThreadExitSignal);
}

/*
 *  cacShutdownUDP ()
 */
void cacShutdownUDP (udpiiu &iiu)
{
    shutdown (iiu.sock, SD_BOTH);
    semBinaryGive (iiu.xmitSignal);
}

/*
 *  udpiiu::~udpiiu ()
 */
udpiiu::~udpiiu ()
{
    semBinaryMustTake (this->recvThreadExitSignal);
    semBinaryMustTake (this->sendThreadExitSignal);

    semBinaryDestroy (this->xmitSignal);
    semMutexDestroy (this->xmitBufLock);
    semBinaryDestroy (this->recvThreadExitSignal);
    semBinaryDestroy (this->sendThreadExitSignal);
    ellFree (&this->dest);

    if (this->niiu.iiu.pcas->ca_fd_register_func) {
        (*this->niiu.iiu.pcas->ca_fd_register_func) 
            (this->niiu.iiu.pcas->ca_fd_register_arg, this->sock, FALSE);
    }
    destroyNIIU (&this->niiu);
    socket_close (this->sock);
}

/*
 * getToken()
 */
LOCAL char *getToken(const char **ppString, char *pBuf, unsigned bufSIze)
{
    const char *pToken;
    unsigned i;

    pToken = *ppString;
    while(isspace(*pToken)&&*pToken){
        pToken++;
    }

    for (i=0u; i<bufSIze; i++) {
        if (isspace(pToken[i]) || pToken[i]=='\0') {
            pBuf[i] = '\0';
            break;
        }
        pBuf[i] = pToken[i];
    }

    *ppString = &pToken[i];

    if(*pToken){
        return pBuf;
    }
    else{
        return NULL;
    }
}

/*
 * caAddConfiguredAddr()
 */
void caAddConfiguredAddr (cac *pcac, ELLLIST *pList, const ENV_PARAM *pEnv, 
    SOCKET socket, unsigned short port)
{
    osiSockAddrNode *pNewNode;
    const char *pStr;
    const char *pToken;
    struct sockaddr_in addr;
    char buf[32u]; /* large enough to hold an IP address */
    int status;

    pStr = envGetConfigParamPtr (pEnv);
    if (!pStr) {
        return;
    }

    while ( ( pToken = getToken (&pStr, buf, sizeof (buf) ) ) ) {
        status = aToIPAddr ( pToken, port, &addr );
        if (status<0) {
            ca_printf (pcac, "%s: Parsing '%s'\n", __FILE__, pEnv->name);
            ca_printf (pcac, "\tBad internet address or host name: '%s'\n", pToken);
            continue;
        }

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            ca_printf (pcac, "caAddConfiguredAddr(): no memory available for configuration\n");
            return;
        }

        pNewNode->addr.ia = addr;

		/*
		 * LOCK applied externally
		 */
        ellAdd (pList, &pNewNode->node);
    }

    return;
}

/*
 * caSetupBCastAddrList()
 */
void caSetupBCastAddrList (cac *pcac, ELLLIST *pList, 
                           SOCKET sock, unsigned short port)
{
    osiSockAddrNode *pNode;
    ELLLIST         tmpList;
    char            *pstr;
    char            yesno[32u];
    int             yes;

    /*
     * dont load the list twice
     */
    assert ( ellCount(pList) == 0 );

    ellInit ( &tmpList );

    /*
     * Check to see if the user has disabled
     * initializing the search b-cast list
     * from the interfaces found.
     */
    yes = TRUE;
    pstr = envGetConfigParam (&EPICS_CA_AUTO_ADDR_LIST,       
            sizeof(yesno), yesno);
    if (pstr) {
        if ( strstr ( pstr, "no" ) || strstr ( pstr, "NO" ) ) {
            yes = FALSE;
        }
    }

    /*
     * LOCK is for piiu->destAddr list
     * (lock outside because this is used by the server also)
     */
    if (yes) {
        osiSockAddr addr;
        addr.ia.sin_family = AF_UNSPEC;
        osiSockDiscoverBroadcastAddresses ( &tmpList, sock, &addr );
    }

    caAddConfiguredAddr (pcac, &tmpList, &EPICS_CA_ADDR_LIST, sock, port );

    /*
     * eliminate duplicates and set the port
     */
    while ( (pNode  = (osiSockAddrNode *) ellGet ( &tmpList ) ) ) {
        osiSockAddrNode *pTmpNode;

        if ( pNode->addr.sa.sa_family == AF_INET ) {
            /*
             * set the correct destination port
             */
            pNode->addr.ia.sin_port = htons (port);

            pTmpNode = (osiSockAddrNode *) ellFirst (pList);
            while ( pTmpNode ) {
                if (pTmpNode->addr.sa.sa_family == AF_INET) {
                    if (pNode->addr.ia.sin_addr.s_addr == pTmpNode->addr.ia.sin_addr.s_addr && 
                        pNode->addr.ia.sin_port == pTmpNode->addr.ia.sin_port) {
                        char buf[64];
                        ipAddrToA (&pNode->addr.ia, buf, sizeof(buf));
                        printf ("Warning: Duplicate EPICS CA Address list entry \"%s\" discarded\n", buf);
                        free (pNode);
                        pNode = NULL;
                        break;
                    }
                }
                pTmpNode = (osiSockAddrNode *) ellNext (&pNode->node);
            }
            if (pNode) {
                ellAdd (pList, &pNode->node);
            }
        }
        else {
            ellAdd (pList, &pNode->node);
        }
    }

    /*
     * print warning message if there is an empty search query
     * address list
     */
    if ( ellCount ( pList ) == 0 ) {
        genLocalExcep ( NULL, ECA_NOSEARCHADDR, NULL );
        return;
    }
}

/*
 *  repeater_installed ()
 *
 *  Test for the repeater already installed
 *
 *  NOTE: potential race condition here can result
 *  in two copies of the repeater being spawned
 *  however the repeater detects this, prints a message,
 *  and lets the other task start the repeater.
 *
 *  QUESTION: is there a better way to test for a port in use? 
 *  ANSWER: none that I can find.
 *
 *  Problems with checking for the repeater installed
 *  by attempting to bind a socket to its address
 *  and port.
 *
 *  1) Closed socket may not release the bound port
 *  before the repeater wakes up and tries to grab it.
 *  Attempting to bind the open socket to another port
 *  also does not work.
 *
 *  072392 - problem solved by using SO_REUSEADDR
 */
int repeater_installed (udpiiu *piiu)
{
    int                 installed = FALSE;
    int                 status;
    SOCKET              sock;
    struct sockaddr_in  bd;
    int                 flag;

    sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return installed;
    }

    memset ( (char *) &bd, 0, sizeof (bd) );
    bd.sin_family = AF_INET;
    bd.sin_addr.s_addr = htonl (INADDR_ANY); 
    bd.sin_port = htons (piiu->repeaterPort);   
    status = bind (sock, (struct sockaddr *) &bd, sizeof(bd) );
    if (status<0) {
        if (SOCKERRNO == SOCK_EADDRINUSE) {
            installed = TRUE;
        }
    }

    /*
     * turn on reuse only after the test so that
     * this works on kernels that support multicast
     */
    flag = TRUE;
    status = setsockopt ( sock, SOL_SOCKET, SO_REUSEADDR, 
                (char *)&flag, sizeof (flag) );
    if (status<0) {
        ca_printf (piiu->niiu.iiu.pcas, "CAC: set socket option reuseaddr set failed\n");
    }

    socket_close (sock);

    return installed;
}

//
// udpiiu::udpiiu ()
//
udpiiu::udpiiu (cac *pcac) :
    searchTmr (*this, pcac->timerQueue), repeaterSubscribeTmr (*this, pcac->timerQueue)
{
    static const unsigned short PORT_ANY = 0u;
    osiSockAddr addr;
    int boolValue = TRUE;
    SOCKET sock;
    int status;
    threadId tid;

    this->repeaterPort = 
        caFetchPortConfig (pcac, &EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT);

    sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        ca_printf (pcac, "CAC: unable to create datagram socket because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        throwWithLocation ( noSocket () );
    }

    status = setsockopt ( sock, SOL_SOCKET, SO_BROADCAST, 
                (char *) &boolValue, sizeof (boolValue) );
    if (status<0) {
        ca_printf (pcac, "CAC: unable to enable IP broadcasting because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
    }

#if 0
    {
        /*
         * some concern that vxWorks will run out of mBuf's
         * if this change is made joh 11-10-98
         *
         * bump up the UDP recv buffer
         */
        int size = 1u<<15u;
        status = setsockopt ( sock, SOL_SOCKET, SO_RCVBUF,
                (char *)&size, sizeof (size) );
        if (status<0) {
            ca_printf ("CAC: unable to set socket option SO_RCVBUF because \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
    }
#endif

    /*
     * force a bind to an unconstrained address because we may end
     * up receiving first
     */
    memset ( (char *)&addr, 0 , sizeof (addr) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl (INADDR_ANY); 
    addr.ia.sin_port = htons (PORT_ANY);   
    status = bind (sock, &addr.sa, sizeof (addr) );
    if (status<0) {
        socket_close (sock);
        ca_printf (pcac, "CAC: unable to bind to an unconstrained address because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        throwWithLocation ( noSocket () );
    }

    constructNIIU (pcac, &this->niiu);

    this->sock = sock;
    this->nBytesInXmitBuf = 0u;
    this->contactRepeater = 0u;
    this->repeaterContacted = 0u;
    this->repeaterTries = 0u;

    this->xmitBufLock = semMutexCreate ();
    if (!this->xmitBufLock) {
        socket_close (this->sock);
        destroyNIIU (&this->niiu);
        throwWithLocation ( noMemory () );
    }

    this->recvThreadExitSignal = semBinaryCreate (semEmpty);
    if (!this->recvThreadExitSignal) {
        semMutexDestroy (this->xmitBufLock);
        socket_close (this->sock);
        destroyNIIU (&this->niiu);
        throwWithLocation ( noMemory () );
    }

    this->sendThreadExitSignal = semBinaryCreate (semEmpty);
    if (!this->sendThreadExitSignal) {
        semBinaryDestroy (this->recvThreadExitSignal);
        semMutexDestroy (this->xmitBufLock);
        destroyNIIU (&this->niiu);
        socket_close (this->sock);
        throwWithLocation ( noMemory () );
    }

    this->xmitSignal = semBinaryCreate (semEmpty);
    if (!this->sendThreadExitSignal) {
        ca_printf (pcac, "CA: unable to create xmit signal\n");
        semBinaryDestroy (this->recvThreadExitSignal);
        semBinaryDestroy (this->sendThreadExitSignal);
        semMutexDestroy (this->xmitBufLock);
        destroyNIIU (&this->niiu);
        socket_close (this->sock);
        throwWithLocation ( noMemory () );
    }

    /*
     * load user and auto configured
     * broadcast address list
     */
    ellInit (&this->dest);
    caSetupBCastAddrList (pcac, &this->dest, this->sock, pcac->ca_server_port);
  
    tid = threadCreate ("CAC UDP Recv", threadPriorityChannelAccessClient,
            threadGetStackSize (threadStackMedium), cacRecvThreadUDP, this);
    if (tid==0) {
        ca_printf (pcac, "CA: unable to create UDP receive thread\n");
        shutdown (this->sock, SD_BOTH);
        semBinaryDestroy (this->xmitSignal);
        semBinaryDestroy (this->recvThreadExitSignal);
        semBinaryDestroy (this->sendThreadExitSignal);
        semMutexDestroy (this->xmitBufLock);
        destroyNIIU (&this->niiu);
        socket_close (this->sock);
        throwWithLocation ( noMemory () );
    }

    tid = threadCreate ( "CAC UDP Send", threadPriorityChannelAccessClient,
            threadGetStackSize (threadStackMedium), cacSendThreadUDP, this);
    if (tid==0) {
        ca_printf (pcac, "CA: unable to create UDP transmitt thread\n");
        shutdown (this->sock, SD_BOTH);
        semMutexMustTake (this->recvThreadExitSignal);
        semBinaryDestroy (this->xmitSignal);
        semBinaryDestroy (this->recvThreadExitSignal);
        semBinaryDestroy (this->sendThreadExitSignal);
        semMutexDestroy (this->xmitBufLock);
        destroyNIIU (&this->niiu);
        socket_close (this->sock);
        throwWithLocation ( noMemory () );
    }

    if (pcac->ca_fd_register_func) {
        (*pcac->ca_fd_register_func) (pcac->ca_fd_register_arg, this->sock, TRUE);
    }

    if ( ! repeater_installed (this) ) {
        osiSpawnDetachedProcessReturn osptr;
        
	    /*
	     * This is not called if the repeater is known to be 
	     * already running. (in the event of a race condition 
	     * the 2nd repeater exits when unable to attach to the 
	     * repeater's port)
	     */
        osptr = osiSpawnDetachedProcess ("CA Repeater", "caRepeater");
        if (osptr==osiSpawnDetachedProcessNoSupport) {
            threadId tid;

            tid = threadCreate (
                    "CA repeater",
                    threadPriorityChannelAccessClient,
                    threadGetStackSize (threadStackMedium),
                    caRepeaterThread,
                    0);
            if (tid==0) {
                ca_printf (pcac, "CA: unable to create CA repeater daemon thread\n");
            }
        }
        else if (osptr==osiSpawnDetachedProcessFail) {
            ca_printf (pcac, "CA: unable to start CA repeater daemon detached process\n");
        }
    }

    this->repeaterSubscribeTmr.reschedule ();
}


/*
 * cacDisconnectChannel()
 */
void cacDisconnectChannel (nciu *chan)
{
    cac *pcac = chan->ciu.piiu->pcas;

    LOCK (pcac);

    if (chan->ciu.piiu == &pcac->pudpiiu->niiu.iiu) {
        netChannelDestroy (pcac, chan->cid);
        UNLOCK (pcac);
        return;
    }

    chan->type = USHRT_MAX;
    chan->count = 0u;
    chan->sid = UINT_MAX;
    chan->ar.read_access = FALSE;
    chan->ar.write_access = FALSE;

    /*
     * call their connection handler as required
     */
    if ( chan->connected ) {
        nmiu *monix, *next;

        /*
         * look for events that have an event cancel in progress
         */
        for (monix = (nmiu *) ellFirst (&chan->eventq);
                monix; monix = next) {
            
            next = (nmiu *) ellNext (&monix->node);
            
            if (monix->cmd==CA_PROTO_EVENT_ADD) {
                /*
                 * if there is an event cancel in progress
                 * delete the event - we will never receive
                 * an event cancel confirm from this server
                 */
                if (monix->miu.usr_func == NULL) {
                    caIOBlockFree (chan->ciu.piiu->pcas, monix->id);
                }
            }
            else {
                struct event_handler_args   args;

                args.usr =      monix->miu.usr_arg;
                args.chid =     monix->miu.pChan;
                args.type =     monix->miu.type;
                args.count =    monix->miu.count;
                args.status =   ECA_DISCONN;
                args.dbr =      NULL;

                if (monix->miu.usr_func) {
                    (*monix->miu.usr_func) (args);
                }
                caIOBlockFree (chan->ciu.piiu->pcas, monix->id);
            }
        }

        if (chan->ciu.pConnFunc) {
            struct connection_handler_args  args;

            args.chid = (chid) &chan->ciu;
            args.op = CA_OP_CONN_DOWN;
            (*chan->ciu.pConnFunc) (args);
        }
        if (chan->ciu.pAccessRightsFunc) {
            struct access_rights_handler_args   args;

            args.chid = (chid) &chan->ciu;
            args.ar = chan->ar;
            (*chan->ciu.pAccessRightsFunc) (args);
        }
    }
    removeFromChanList (chan);
    /*
     * try to reconnect
     */
    assert (pcac->pudpiiu);
    addToChanList (chan, &pcac->pudpiiu->niiu);
    pcac->pudpiiu->searchTmr.reset (0.0);
    UNLOCK (pcac);
}

/*
 * localHostName()
 *
 * o Indicates failure by setting ptr to nill
 *
 * o Calls non posix gethostbyname() so that we get DNS style names
 *      (gethostbyname() should be available with most BSD sock libs)
 *
 * vxWorks user will need to configure a DNS format name for the
 * host name if they wish to be cnsistent with UNIX and VMS hosts.
 *
 * this needs to attempt to determine if the process is a remote 
 * login - hard to do under UNIX
 */
char *localHostName ()
{
    int     size;
    int     status;
    char    pName[MAXHOSTNAMELEN];
    char    *pTmp;

    status = gethostname ( pName, sizeof (pName) );
    if(status){
        return NULL;
    }

    size = strlen (pName)+1;
    pTmp = (char *) malloc (size);
    if (!pTmp) {
        return pTmp;
    }

    strncpy ( pTmp, pName, size-1 );
    pTmp[size-1] = '\0';

    return pTmp;
}

/*
 * caPrintAddrList()
 */
void caPrintAddrList (ELLLIST *pList)
{
    osiSockAddrNode *pNode;

    printf ("Channel Access Address List\n");
    pNode = (osiSockAddrNode *) ellFirst (pList);
    while (pNode) {
        char buf[64];
        ipAddrToA (&pNode->addr.ia, buf, sizeof(buf));
        printf ("%s\n", buf);
        pNode = (osiSockAddrNode *) ellNext (&pNode->node);
    }
}

/*
 * caFetchPortConfig()
 */
unsigned short caFetchPortConfig
    (cac *pcac, const ENV_PARAM *pEnv, unsigned short defaultPort)
{
    long        longStatus;
    long        epicsParam;
    int         port;

    longStatus = envGetLongConfigParam(pEnv, &epicsParam);
    if (longStatus!=0) {
        epicsParam = (long) defaultPort;
        ca_printf (pcac, "EPICS \"%s\" integer fetch failed\n", pEnv->name);
        ca_printf (pcac, "setting \"%s\" = %ld\n", pEnv->name, epicsParam);
    }

    /*
     * This must be a server port that will fit in an unsigned
     * short
     */
    if (epicsParam<=IPPORT_USERRESERVED || epicsParam>USHRT_MAX) {
        ca_printf (pcac, "EPICS \"%s\" out of range\n", pEnv->name);
        /*
         * Quit if the port is wrong due CA coding error
         */
        assert (epicsParam != (long) defaultPort);
        epicsParam = (long) defaultPort;
        ca_printf (pcac, "Setting \"%s\" = %ld\n", pEnv->name, epicsParam);
    }

    /*
     * ok to clip to unsigned short here because we checked the range
     */
    port = (unsigned short) epicsParam;

    return port;
}

#if 0
/*
 * cacPushPending()
 */
LOCAL unsigned cacPushPending (cac *pcac)
{
    unsigned        pending = FALSE;
    unsigned long   bytesPending;
    netIIU            *piiu;
    
    LOCK (pcac);
    for ( piiu = (netIIU *) ellFirst (&pcac->ca_iiuList);
        piiu; piiu = (netIIU *) ellNext (&piiu->node) ){
        
        if (piiu == pcac->pudpiiu) {
            continue;
        }
        
        if ( piiu->state == iiu_connected && piiu->pushPending ) {
            bytesPending = cacRingBufferReadSize (&piiu->send, FALSE);
            if(bytesPending > 0u){
                pending = TRUE;
                break;
            }
        }
    }
    UNLOCK (pcac);
    
    return pending;
}
#endif

#if 0
/*
 *      CAC_MUX_IO()
 */
void cac_mux_io (cac *pcac,  double maxDelay, unsigned iocCloseAllowed)
{
    int         count;
    double      timeOut;
    unsigned    countDown;

    /*
     * first check for pending recv's with a zero time out so that
     * 1) flow control works correctly (and)
     * 2) we queue up sends resulting from recvs properly
     *  (this results in improved max throughput)
     *
     * ... but dont allow this to go on forever if a fast
     * server is flooding a slow client with monitors ...
     */
    countDown = 512u;
    while (--countDown) {
        /*
         * NOTE cac_select_io() will set the
         * send flag for a particular iiu irregradless
         * of what is requested here if piiu->pushPending
         * is set
         */
        count = cac_select_io (pcac, 0.0, CA_DO_RECVS);
        if (count<=0) {
            break;
        }
        ca_process_input_queue (pcac);
    }

    /*
     * manage search timers and detect disconnects
     */
    manage_conn(pcac);

    /*
     * next check for pending writes's with the specified time out 
     *
     * ... but dont allow this to go on forever if a fast
     * server is flooding a slow client with monitors ...
     */
    countDown = 512u;
    timeOut = maxDelay;
    while (TRUE) {
        count = cac_select_io (pcac, timeOut, CA_DO_RECVS|CA_DO_SENDS);
        countDown--;
        if (count<=0 || countDown==0u) {
            /*
             * if its a flush then loop until all
             * of the send buffers are empty
             */
            if (pcac->ca_flush_pending) {
                /*
                 * complete flush is deferred if we are 
                 * inside an event routine 
                 */
                if (EVENTLOCKTEST(pcac)) {
                    break;
                }
                else {
                    if ( cacPushPending (pcac) ) {
                        countDown = 512u;
                        timeOut = cac_fetch_poll_period (pcac);
                        /*
                         * allow connection to be marked disconnected
                         * if we wait too long for the flush to occurr
                         */
                        checkConnWatchdogs (pcac, FALSE); 
                    }
                    else {
                        pcac->ca_flush_pending = FALSE;
                        break;
                    }
                }
            }
            else {
                break;
            }
        }
        else {
            timeOut = 0.0;
        }
        ca_process_input_queue (pcac);
    }

    checkConnWatchdogs (pcac);
}
#endif

tcpiiu *iiuToTCPIIU (baseIIU *pIn)
{
    char *pc = (char *) pIn;

    assert (pIn != &pIn->pcas->localIIU.iiu);
    assert (pIn != &pIn->pcas->pudpiiu->niiu.iiu);
    pc -= offsetof (tcpiiu, niiu.iiu);
    return (tcpiiu *) pc;
}

/*
 * convert a generic IIU pointer to a network IIU
 */
netIIU *iiuToNIIU (baseIIU *pIn)
{
    char *pc = (char *) pIn;

    assert (pIn != &pIn->pcas->localIIU.iiu);
    pc -= offsetof (netIIU, iiu);
    return (netIIU *) pc;
}

/*
 * convert a generic IIU pointer to a local IIU
 */
lclIIU *iiuToLIIU (baseIIU *pIn)
{
    char *pc = (char *) pIn;

    assert (pIn == &pIn->pcas->localIIU.iiu);
    pc -= offsetof (lclIIU, iiu);
    return (lclIIU *) pc;
}
