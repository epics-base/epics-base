
/*
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeffrey O. Hill
 *
 */

#include    "osiSigPipeIgnore.h"
#include    "freeList.h"
#include    "osiProcess.h"

/*
 * allocate error message string array 
 * here so I can use sizeof
 */
#define CA_ERROR_GLBLSOURCE

/*
 * allocate db_access message strings here
 */
#define DB_TEXT_GLBLSOURCE

/*
 * allocate header version strings here
 */
#define CAC_VERSION_GLOBAL

#include    "iocinf.h"

const static caHdr nullmsg = {
    0,0,0,0,0,0
};
const static char nullBuff[32] = {
    0,0,0,0,0,0,0,0,0,0, 
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0
};

static threadPrivateId caClientContextId;

threadPrivateId cacRecursionLock;

#define TYPENOTINUSE (-2)

/*
 * fetchClientContext ();
 */
int fetchClientContext (cac **ppcac)
{
    int status;

    if ( caClientContextId != NULL ) {
        *ppcac = (cac *) threadPrivateGet (caClientContextId);
        if (*ppcac) {
            return ECA_NORMAL;
        }
    }

    status = ca_task_initialize ();
    if (status == ECA_NORMAL) {
        *ppcac = (cac *) threadPrivateGet (caClientContextId);
        if (!*ppcac) {
            status = ECA_INTERNAL;
        }
    }

    return status;
}

#if 0
/*
 * cacSetSendPending ()
 */
LOCAL void cacSetSendPending (tcpiiu *piiu)
{
    if (!piiu->sendPending) {
        piiu->timeAtSendBlock = piiu->niiu.iiu.pcas->currentTime;
        piiu->sendPending = TRUE;
    }
}
#endif

/*
 *  cac_push_tcp_msg()
 */ 
LOCAL int cac_push_tcp_msg (tcpiiu *piiu, const caHdr *pmsg, const void *pext)
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

    /*
     * push the header onto the ring 
     */
    bytesSent = cacRingBufferWrite ( &piiu->send, &msg, sizeof (msg) );
    if ( bytesSent != sizeof (msg) ) {
        return ECA_DISCONNCHID;
    }

    /*
     * push message body onto the ring
     *
     * (optionally encode in network format as we send)
     */
    if (extsize>0u) {
        bytesSent = cacRingBufferWrite ( &piiu->send, pext, actualextsize );
        if ( bytesSent != actualextsize ) {
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
                bytesSent = cacRingBufferWrite ( &piiu->send, nullBuff, n );
                if ( bytesSent != n ) {
                    return ECA_DISCONNCHID;
                }
            }
        }
    }

    return ECA_NORMAL;
}

/*
 *  cac_push_tcp_msg_no_block ()
 */ 
LOCAL bool cac_push_tcp_msg_no_block (tcpiiu *piiu, const caHdr *pmsg, const void *pext)
{
    unsigned size;
    int status;

    size = sizeof (*pmsg);
    if ( pext != NULL ) {
        size += CA_MESSAGE_ALIGN (pmsg->m_postsize);
    }

    if ( cacRingBufferWriteLockNoBlock (&piiu->send, size) ) {
        status = cac_push_tcp_msg (piiu, pmsg, pext);
        cacRingBufferWriteUnlock (&piiu->send);
        if (status == ECA_NORMAL) {
            return true;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}

/*
 *  cac_push_udp_msg()
 */ 
LOCAL int cac_push_udp_msg (udpiiu *piiu, const caHdr *pMsg, const void *pExt, ca_uint16_t extsize)
{
    unsigned long   msgsize;
    ca_uint16_t     allignedExtSize;
    caHdr           *pbufmsg;

    allignedExtSize = CA_MESSAGE_ALIGN (extsize);
    msgsize = sizeof (caHdr) + allignedExtSize;


    /* fail out if max message size exceeded */
    if ( msgsize >= sizeof (piiu->xmitBuf)-7 ) {
        return ECA_TOLARGE;
    }

    semMutexMustTake (piiu->xmitBufLock);
    if ( msgsize + piiu->nBytesInXmitBuf > sizeof (piiu->xmitBuf) ) {
        semMutexGive (piiu->xmitBufLock);
        return ECA_TOLARGE;
    }

    pbufmsg = (caHdr *) &piiu->xmitBuf[piiu->nBytesInXmitBuf];
    *pbufmsg = *pMsg;
    memcpy (pbufmsg+1, pExt, extsize);
    if ( extsize != allignedExtSize ) {
        char *pDest = (char *) (pbufmsg+1);
        memset (pDest + extsize, '\0', allignedExtSize - extsize);
    }
    pbufmsg->m_postsize = htons (allignedExtSize);
    piiu->nBytesInXmitBuf += msgsize;
    semMutexGive (piiu->xmitBufLock);

    return ECA_NORMAL;
}

/*
 *  Default Exception Handler
 */
LOCAL void ca_default_exception_handler (struct exception_handler_args args)
{
    if (args.chid && args.op != CA_OP_OTHER) {
        ca_signal_formated (
            args.stat, 
            args.pFile, 
            args.lineNo, 
            "%s - with request chan=%s op=%ld data type=%s count=%ld",
            args.ctx,
            ca_name (args.chid),
            args.op,
            dbr_type_to_text(args.type),
            args.count);
    }
    else {
        ca_signal_formated (
            args.stat, 
            args.pFile, 
            args.lineNo, 
            args.ctx);
    }
}

extern "C" {

    /*
     * default local pv interface entry points that always fail
     */
    static pvId pvNameToIdNoop (const char *) 
    {
        return 0;
    }
    static int pvPutFieldNoop (pvId, int, 
                             const void *, int)
    {
        return -1;
    }
    static int pvGetFieldNoop (pvId, int,
                        void *, int, void *)
    {
        return -1;
    }
    static long pvPutNotifyInitiateNoop (pvId, 
        unsigned, unsigned long, const void *, 
        void (*)(void *), void *, putNotifyId *)
    {
        return -1;
    }
    static void pvPutNotifyDestroyNoop (putNotifyId)
    {
    }

    static const char * pvNameNoop (pvId)
    {
        return "";
    }
    static unsigned long pvNoElementsNoop (pvId)
    {
        return 0u;
    }
    static short pvTypeNoop (pvId)
    {
        return -1;
    }
    static dbEventCtx pvEventQueueInitNoop ()
    {
        return NULL;
    }
    static int pvEventQueueStartNoop (dbEventCtx, const char *, 
            void (*)(void *), void *, int)
    {
        return -1;
    }
    static void pvEventQueueCloseNoop (dbEventCtx)
    {
    }
    static dbEventSubscription pvEventQueueAddEventNoop (dbEventCtx, pvId,
            void (*)(void *, pvId, int, struct db_field_log *), 
            void *, unsigned)
    {
        return NULL;
    }
    static int pvEventQueuePostSingleEventNoop (dbEventSubscription)
    {
        return -1;
    }
    static void pvEventQueueCancelEventNoop (dbEventSubscription)
    {
    }
    static int pvEventQueueAddExtraLaborEventNoop (dbEventCtx, 
            void (*)(void *), void *)
    {
        return -1;
    }
    static int pvEventQueuePostExtraLaborNoop (dbEventCtx)
    {
        return -1;
    }

}

LOCAL const pvAdapter pvAdapterNOOP =
{
    pvNameToIdNoop,
    pvPutFieldNoop,
    pvGetFieldNoop,
    pvPutNotifyInitiateNoop,
    pvPutNotifyDestroyNoop,
    pvNameNoop,
    pvNoElementsNoop,
    pvTypeNoop,
    pvEventQueueInitNoop,
    pvEventQueueStartNoop,
    pvEventQueueCloseNoop,
    pvEventQueuePostSingleEventNoop,
    pvEventQueueAddEventNoop,
    pvEventQueueCancelEventNoop,
    pvEventQueueAddExtraLaborEventNoop,
    pvEventQueuePostExtraLaborNoop
};

/*
 * event_import()
 */
LOCAL void event_import (void *pParam)
{
    threadPrivateSet (caClientContextId, pParam);
}

/*
 * constructLocalIIU
 */
LOCAL int constructLocalIIU (cac *pcac, const pvAdapter *pva, lclIIU *piiu)
{
    long status;

    freeListInitPvt (&piiu->localSubscrFreeListPVT, sizeof (lmiu), 256);

    piiu->putNotifyLock = semMutexCreate ();
    if (!piiu->putNotifyLock) {
        return ECA_ALLOCMEM;
    }

	ellInit (&piiu->buffList);
    ellInit (&piiu->chidList);
    piiu->iiu.pcas = pcac;
    piiu->pva = pva;

	piiu->evctx = (*piiu->pva->p_pvEventQueueInit) ();
    if (piiu->evctx) {

        /* higher priority */
	    status = (*piiu->pva->p_pvEventQueueStart)
             (piiu->evctx, "CAC Event", event_import, pcac, +1); 
        if (status) {
            (*piiu->pva->p_pvEventQueueClose) (piiu->evctx);
            semMutexDestroy (piiu->putNotifyLock);
            return ECA_ALLOCMEM;
        }
    }
    return ECA_NORMAL;
}

/*
 * convert a generic ciu pointer to a network ciu
 */
LOCAL nciu *ciuToNCIU (baseCIU *pIn)
{
    char *pc = (char *) pIn;

    assert (pIn->piiu != &pIn->piiu->pcas->localIIU.iiu);
    pc -= offsetof (nciu, ciu);
    return (nciu *) pc;
}

/*
 * convert a generic baseCIU pointer to a local ciu
 */
LOCAL lciu *ciuToLCIU (baseCIU *pIn)
{
    char *pc = (char *) pIn;

    assert (pIn->piiu == &pIn->piiu->pcas->localIIU.iiu);
    pc -= offsetof (lciu, ciu);
    return (lciu *) pc;
}

/*
 * convert a generic miu pointer to a network miu
 */
LOCAL nmiu *miuToNMIU (baseMIU *pIn)
{
    char *pc = (char *) pIn;

    assert (pIn->pChan->piiu != &pIn->pChan->piiu->pcas->localIIU.iiu);
    pc -= offsetof (nmiu, miu);
    return (nmiu *) pc;
}

/*
 * convert a generic miu pointer to a local miu
 */
LOCAL lmiu *miuToLMIU (baseMIU *pIn)
{
    char *pc = (char *) pIn;

    assert (pIn->pChan->piiu == &pIn->pChan->piiu->pcas->localIIU.iiu);
    pc -= offsetof (lmiu, miu);
    return (lmiu *) pc;
}

/*
 * localMonitorResourceDestroy ()
 */
LOCAL void localMonitorResourceDestroy (lmiu *pSubscription)
{
    lciu *pLocalChan = ciuToLCIU (pSubscription->miu.pChan);

    /*
     * lock must be off when canceling the event so that
     * the event queue can be flushed
     */   
    (*pSubscription->miu.pChan->piiu->pcas->localIIU.pva->p_pvEventQueueCancelEvent) (pSubscription->es);

    LOCK (pLocalChan->ciu.piiu->pcas);
    ellDelete (&pLocalChan->eventq, &pSubscription->node);
    freeListFree (pSubscription->miu.pChan->piiu->pcas->localIIU.localSubscrFreeListPVT, pSubscription);
    UNLOCK (pLocalChan->ciu.piiu->pcas);
}

/*
 * caPutNotifydestroy ()
 */
LOCAL void caPutNotifydestroy (lclIIU *piiu, caPutNotify *ppn)
{
	if (ppn) {
		(*piiu->pva->p_pvPutNotifyDestroy) (ppn->dbPutNotify);
        free (ppn);
    }
}

/*
 * localChannelDestroy ()
 */
LOCAL void localChannelDestroy (lclIIU *piiu, lciu *pChan)
{
    lmiu *pSubscription;

    LOCK (piiu->iiu.pcas);

    pChan->ciu.pAccessRightsFunc = NULL;
    pChan->ciu.pConnFunc = NULL;

	while ( (pSubscription = (lmiu *) ellFirst (&pChan->eventq)) ) {

        /*
		 * temp release lock so that the event task
		 * can flush the event 
		 */
        UNLOCK (piiu->iiu.pcas);
        localMonitorResourceDestroy (pSubscription);
        LOCK (piiu->iiu.pcas);
    }

    caPutNotifydestroy (piiu, pChan->ppn);

    ellDelete (&piiu->chidList, &pChan->node);

    UNLOCK (piiu->iiu.pcas);

    pChan->ciu.puser = NULL;
    pChan->ciu.piiu = NULL;

    free (pChan);
}

/*
 * liiu_destroy ()
 */
LOCAL void liiu_destroy (lclIIU *piiu)
{
    lciu *pChan;

    freeListCleanup (piiu->localSubscrFreeListPVT);

    /*
     * destroy all channels
     */
    pChan = (lciu *) ellFirst (&piiu->chidList);
    while (pChan) {
        lciu *pChanNext = (lciu *) ellNext (&pChan->node);
        localChannelDestroy (piiu, pChan);
        pChan = pChanNext;
    }

	/*
	 * All local events must be canceled prior to closing the
	 * local event facility
	 */
    (*piiu->pva->p_pvEventQueueClose) (piiu->evctx);

	ellFree (&piiu->buffList);

    semMutexDestroy (piiu->putNotifyLock);
}

/*
 *  ca_task_initialize ()
 */
int epicsShareAPI ca_task_initialize (void)
{
    cac *pcac;

    if (caClientContextId==NULL) {
        caClientContextId = threadPrivateCreate ();
        if (!caClientContextId) {
            return ECA_ALLOCMEM;
        }
    }

    pcac = (cac *) threadPrivateGet (caClientContextId);
	if (pcac) {
		return ECA_NORMAL;
	}

    pcac = new cac;
	if (!pcac) {
		return ECA_ALLOCMEM;
	}
    return ECA_NORMAL;
}

//
// cac::cac ()
//
cac::cac ()
{
	long status;
    int caStatus;

    if (cacRecursionLock==NULL) {
        cacRecursionLock = threadPrivateCreate ();
        if (!cacRecursionLock) {
            throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
        }
    }

	if (!bsdSockAttach()) {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
	}

	ellInit (&this->ca_taskVarList);
	ellInit (&this->putCvrtBuf);
    ellInit (&this->ca_iiuList);
    ellInit (&this->fdInfoFreeList);
    ellInit (&this->fdInfoList);
    this->ca_printf_func = errlogVprintf;
    this->pudpiiu = NULL;
    this->ca_exception_func = NULL;
    this->ca_exception_arg = NULL;
    this->ca_fd_register_func = NULL;
    this->ca_fd_register_arg = NULL;
    this->ca_pEndOfBCastList = NULL;
    this->ca_pndrecvcnt = 0;
    this->ca_nextSlowBucketId = 0;
    this->ca_nextFastBucketId = 0;
    this->ca_flush_pending = FALSE;
    this->ca_number_iiu_in_fc = 0u;
    this->ca_manage_conn_active = FALSE;
    memset (this->ca_beaconHash, '\0', sizeof (this->ca_beaconHash) );
    ca_sg_init (this);

    this->ca_client_lock = semMutexCreate();
    if (!this->ca_client_lock) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }
    this->ca_io_done_sem = semBinaryCreate(semEmpty);
    if (!this->ca_io_done_sem) {
        semMutexDestroy (this->ca_client_lock);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }
    this->ca_blockSem = semBinaryCreate(semEmpty);
    if (!this->ca_blockSem) {
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    installSigPipeIgnore();

    {
        char tmp[256];
        size_t len;
        osiGetUserNameReturn gunRet;

        gunRet = osiGetUserName ( tmp, sizeof (tmp) );
        if (gunRet!=osiGetUserNameSuccess) {
            tmp[0] = '\0';
        }
        len = strlen (tmp) + 1;
        this->ca_pUserName = (char *) malloc ( len );
        if ( ! this->ca_pUserName ) {
            semMutexDestroy (this->ca_client_lock);
            semBinaryDestroy (this->ca_io_done_sem);
            semBinaryDestroy (this->ca_blockSem);
            throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
        }
        strncpy (this->ca_pUserName, tmp, len);
    }

    /* record the host name */
    this->ca_pHostName = localHostName();
    if (!this->ca_pHostName) {
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        semBinaryDestroy (this->ca_blockSem);
        free (this->ca_pUserName);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    status = tsStampGetCurrent (&this->currentTime);
    if (status!=0) {
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        semBinaryDestroy (this->ca_blockSem);
        free (this->ca_pUserName);
        free (this->ca_pHostName);
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
    }
    this->programBeginTime = this->currentTime;
    this->programBeginTime = this->currentTime;

    /* 
     * construct the local IIU with a default 
     * NOOP database adapter)
     */
    caStatus = constructLocalIIU (this, &pvAdapterNOOP, &this->localIIU);
    if (caStatus!=ECA_NORMAL) {
        ca_sg_shutdown (this);
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        semBinaryDestroy (this->ca_blockSem);
        free (this->ca_pUserName);
        free (this->ca_pHostName);
        throwWithLocation ( caErrorCode (caStatus) );
    }

    freeListInitPvt (&this->ca_ioBlockFreeListPVT, sizeof (nmiu), 256);

    this->ca_pSlowBucket = bucketCreate (CLIENT_HASH_TBL_SIZE);
    if (this->ca_pSlowBucket==NULL) {
        ca_sg_shutdown (this);
        liiu_destroy (&this->localIIU);
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        semBinaryDestroy (this->ca_blockSem);
        free (this->ca_pUserName);
        free (this->ca_pHostName);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    this->ca_pFastBucket = bucketCreate (CLIENT_HASH_TBL_SIZE);
    if (this->ca_pFastBucket==NULL) {
        ca_sg_shutdown (this);
        liiu_destroy (&this->localIIU);\
        bucketFree (this->ca_pSlowBucket);
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        semBinaryDestroy (this->ca_blockSem);
        free (this->ca_pUserName);
        free (this->ca_pHostName);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    status = envGetDoubleConfigParam (&EPICS_CA_CONN_TMO, &this->ca_connectTMO);
    if (status) {
        this->ca_connectTMO = CA_CONN_VERIFY_PERIOD;
        ca_printf (this,
            "EPICS \"%s\" float fetch failed\n",
            EPICS_CA_CONN_TMO.name);
        ca_printf (this,
            "Setting \"%s\" = %f\n",
            EPICS_CA_CONN_TMO.name,
            this->ca_connectTMO);
    }

    this->ca_server_port = 
        caFetchPortConfig (this, &EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

    threadPrivateSet (caClientContextId, (void *) this);
}

/*
 * CA_MODIFY_HOST_NAME()
 *
 * Modify or override the default 
 * client host name.
 *
 * This entry point was changed to a NOOP 
 */
int epicsShareAPI ca_modify_host_name(const char *pHostName)
{
    return ECA_NORMAL;
}

/*
 * ca_modify_user_name()
 *
 * Modify or override the default 
 * client user name.
 *
 * This entry point was changed to a NOOP 
 */
int epicsShareAPI ca_modify_user_name (const char *pClientName)
{
    return ECA_NORMAL;
}


/*
 *  ca_task_exit()
 *
 *  releases all resources alloc to a channel access client
 */
epicsShareFunc int epicsShareAPI ca_task_exit (void)
{
    cac   *pcac;

    if ( caClientContextId != NULL ) {
        pcac = (cac *) threadPrivateGet (caClientContextId);
        if (pcac) {
            delete pcac;
        }
    }

    return ECA_NORMAL;
}

/*
 * cac_destroy ()
 *
 * releases all resources alloc to a channel access client
 */
cac::~cac ()
{
    tcpiiu      *piiu;
    int         status;

    threadPrivateSet (caClientContextId, NULL);

    //
    // shutdown all tcp connections and wait for threads to exit
    //
    while ( 1 ) {
        semBinaryId id;

        LOCK (this);
        piiu = (tcpiiu *) ellFirst (&this->ca_iiuList);
        if (piiu) {
            id = piiu->recvThreadExitSignal;
            initiateShutdownTCPIIU (piiu);
        }
        UNLOCK (this);

        if (piiu) {
            semBinaryTake (id);
        }
        else {
            break;
        }
    }

    //
    // shutdown udp and wait for threads to exit
    //
    if (this->pudpiiu) {
        cacShutdownUDP (*this->pudpiiu);
        delete this->pudpiiu;
    }

    LOCK (this);

    liiu_destroy (&this->localIIU);

    /* remove put convert block free list */
    ellFree (&this->putCvrtBuf);

    /* reclaim sync group resources */
    ca_sg_shutdown (this);

    /* remove remote waiting ev blocks */
    freeListCleanup (this->ca_ioBlockFreeListPVT);

    /* free select context lists */
    ellFree (&this->fdInfoFreeList);
    ellFree (&this->fdInfoList);

    /*
     * remove IOCs in use
     */
    ellFree (&this->ca_iiuList);

    /*
     * free user name string
     */
    if (this->ca_pUserName) {
        free (this->ca_pUserName);
    }

    /*
     * free host name string
     */
    if (this->ca_pHostName) {
        free (this->ca_pHostName);
    }

    /*
     * free hash tables 
     */
    status = bucketFree (this->ca_pSlowBucket);
    assert (status == S_bucket_success);
    status = bucketFree (this->ca_pFastBucket);
    assert (status == S_bucket_success);

    /*
     * free beacon hash table
     */
    freeBeaconHash (this);

    semBinaryDestroy (this->ca_io_done_sem);
    semBinaryDestroy (this->ca_blockSem);

    semMutexDestroy (this->ca_client_lock);

	bsdSockRelease ();
}

/*
 *
 *      CA_BUILD_AND_CONNECT
 *
 *      backwards compatible entry point to ca_search_and_connect()
 */
int epicsShareAPI ca_build_and_connect (const char *name_str, chtype get_type,
            unsigned long get_count, chid * chan, void *pvalue, 
            void (*conn_func) (struct connection_handler_args), void *puser)
{
    if (get_type != TYPENOTCONN && pvalue!=0 && get_count!=0) {
        return ECA_ANACHRONISM;
    }

    return ca_search_and_connect (name_str, chan, conn_func, puser);
}

/*
 * constructCoreChannel ()
 */
LOCAL void constructCoreChannel (baseCIU *pciu,
               void (*conn_func) (struct connection_handler_args), void *puser) 
{
    pciu->piiu = NULL; 
    pciu->puser = puser;
    pciu->pConnFunc = conn_func;
    pciu->pAccessRightsFunc = NULL;
}

/*
 * constructLocalChannel ()
 */
LOCAL int constructLocalChannel (cac *pcac, pvId idIn,            
            void (*conn_func) (struct connection_handler_args), void *puser,
            chid *pid) 
{
    lciu *pchan;
    struct connection_handler_args  args;

    /*
     * allocate channel data structue and also allocate enough 
     * space for the channel name
     */
    pchan = (lciu *) calloc (1, sizeof(*pchan));
    if (!pchan){
        return ECA_ALLOCMEM;
    }

    LOCK (pcac);

    constructCoreChannel (&pchan->ciu, conn_func, puser);
    ellInit (&pchan->eventq);
    pchan->id = idIn;
    pchan->ppn = NULL;
    pchan->ciu.piiu = &pcac->localIIU.iiu;
    ellAdd (&pcac->localIIU.chidList, &pchan->node);

    args.chid = (chid) &pchan->ciu;
    args.op = CA_OP_CONN_UP;
    (*conn_func) (args);

    UNLOCK (pcac);

    *pid = (chid *) &pchan->ciu;

    return ECA_NORMAL;
}

/*
 * constructNetChannel ()
 */
LOCAL int constructNetChannel (cac *pcac, 
            void (*conn_func) (struct connection_handler_args), 
            void *puser, const char *pName, unsigned nameLength, chid *pid) 
{
    nciu *pchan;
    char *pNameDest;
    long status;

    if (nameLength>USHRT_MAX) {
        return ECA_STRTOBIG;
    }

    if (!pcac->pudpiiu) {
        pcac->pudpiiu = new udpiiu (pcac);
        if (!pcac->pudpiiu) {
            return ECA_NOCAST;
        }
    }

    /*
     * allocate channel data structue and also allocate enough 
     * space for the channel name
     */
    pchan = (nciu *) calloc (1, sizeof(*pchan) + nameLength);
    if (!pchan){
        return ECA_ALLOCMEM;
    }

    LOCK (pcac);

    do {
        pchan->cid = CLIENT_SLOW_ID_ALLOC (pcac);
        status = bucketAddItemUnsignedId (pcac->ca_pSlowBucket, &pchan->cid, pchan);
    } while (status == S_bucket_idInUse);
    if (status != S_bucket_success) {
        UNLOCK (pcac);
        free (pchan);
        if (status == S_bucket_noMemory) {
            return ECA_ALLOCMEM;
        }
        return ECA_INTERNAL;
    }

    constructCoreChannel (&pchan->ciu, conn_func, puser);

    pNameDest = (char *)(pchan + 1);
    strncpy (pNameDest, pName, nameLength);
    pNameDest[nameLength-1] = '\0';

    pchan->type = USHRT_MAX; /* invalid initial type    */
    pchan->count = 0;    /* invalid initial count     */
    pchan->sid = UINT_MAX; /* invalid initial server id     */
    pchan->ar.read_access = FALSE;
    pchan->ar.write_access = FALSE;
    pchan->nameLength = nameLength;
    pchan->previousConn = 0;
    pchan->connected = 0;
    addToChanList (pchan, &pcac->pudpiiu->niiu);
    ellInit (&pchan->eventq);

    /*
     * reset broadcasted search counters
     */
    pcac->pudpiiu->searchTmr.reset (CA_RECAST_DELAY);

    /*
     * Connection Management takes care 
     * of sending the the search requests
     */
    if (!pchan->ciu.pConnFunc) {
        pcac->ca_pndrecvcnt++;
    }

    UNLOCK (pcac);

    *pid = (chid *) &pchan->ciu;

    return ECA_NORMAL;
}
 
/*
 *  ca_search_and_connect() 
 */
int epicsShareAPI ca_search_and_connect (const char *name_str, chid *chanptr,
    void (*conn_func) (struct connection_handler_args), void *puser)
{
    int             caStatus;
    cac             *pcac;
    pvId            tmpId;
    unsigned        strcnt;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    /* 
     * rational limit on user supplied string size 
     */
    if (name_str==NULL) {
        return ECA_EMPTYSTR;
    }
    strcnt = strlen (name_str) + 1;
    if (strcnt > MAX_UDP-sizeof(caHdr)) {
        return ECA_STRTOBIG;
    }
    if (strcnt <= 1) {
        return ECA_EMPTYSTR;
    }

    /* 
     * check to see if the channel is hosted within this address space
     */
    tmpId = (*pcac->localIIU.pva->p_pvNameToId) (name_str);
    if (tmpId) {
        return constructLocalChannel (pcac, tmpId, conn_func, puser, chanptr);
    }
    else {
        return constructNetChannel (pcac, conn_func, puser, name_str, strcnt, chanptr);
    }
}

/*
 * cac_search_msg ()
 */
int cac_search_msg (nciu *chan)
{
    udpiiu      *piiu = chan->ciu.piiu->pcas->pudpiiu;
    int         status;
    caHdr       msg;

    if ( chan->ciu.piiu != &piiu->niiu.iiu ) {
        return ECA_INTERNAL;
    }

    if (chan->nameLength > 0xffff) {
        return ECA_STRTOBIG;
    }

    msg.m_cmmd = htons (CA_PROTO_SEARCH);
    msg.m_available = chan->cid;
    msg.m_dataType = htons (DONTREPLY);
    msg.m_count = htons (CA_MINOR_VERSION);
    msg.m_cid = chan->cid;

    status = cac_push_udp_msg (piiu, &msg, chan+1, chan->nameLength);
    if (status != ECA_NORMAL) {
        return status;
    }

    /*
     * increment the number of times we have tried to find this channel
     */
    if (chan->retry<MAXCONNTRIES) {
        chan->retry++;
    }

    /*
     * move the channel to the end of the list so
     * that all channels get a equal chance 
     */
    LOCK (chan->ciu.piiu->pcas);
    ellDelete (&chan->ciu.piiu->pcas->pudpiiu->niiu.chidList, &chan->node);
    ellAdd (&chan->ciu.piiu->pcas->pudpiiu->niiu.chidList, &chan->node);
    UNLOCK (chan->ciu.piiu->pcas);

    return ECA_NORMAL;
}

/*
 * caIOBlockCreate ()
 */
LOCAL nmiu *caIOBlockCreate (nciu *pChan, unsigned cmdIn, chtype type, 
    unsigned long count, void (*pFunc) (struct event_handler_args), void *pParam)
{
    int status;
    nmiu *pIOBlock;

    LOCK (pChan->ciu.piiu->pcas);

    pIOBlock = (nmiu *) freeListCalloc (pChan->ciu.piiu->pcas->ca_ioBlockFreeListPVT);
    if (pIOBlock) {
        do {
            pIOBlock->id = CLIENT_FAST_ID_ALLOC (pChan->ciu.piiu->pcas);
            status = bucketAddItemUnsignedId (pChan->ciu.piiu->pcas->ca_pFastBucket, &pIOBlock->id, pIOBlock);
        } while (status == S_bucket_idInUse);

        if(status != S_bucket_success){
            freeListFree (pChan->ciu.piiu->pcas->ca_ioBlockFreeListPVT, pIOBlock);
            pIOBlock = NULL;
        }
    }

    pIOBlock->cmd = cmdIn;
    pIOBlock->miu.pChan = &pChan->ciu;
    pIOBlock->miu.type = type;
    pIOBlock->miu.count = count;
    pIOBlock->miu.usr_func = pFunc;
    pIOBlock->miu.usr_arg = pParam;

    ellAdd (&pChan->eventq, &pIOBlock->node);

    UNLOCK (pChan->ciu.piiu->pcas);

    return pIOBlock;
}

/*
 *
 *  CA_EVENT_HANDLER()
 *  (only for clients attached to local PVs)
 *
 */
LOCAL void ca_event_handler (void *usrArg, 
    pvId idIn, int hold, struct db_field_log *pfl)
{
    lmiu *monix = (lmiu *) usrArg;
    lciu *pChan = ciuToLCIU (monix->miu.pChan);
    lclIIU *piiu = iiuToLIIU (pChan->ciu.piiu);
    union db_access_val valbuf;
    unsigned long count;
    unsigned long nativeElementCount;
    void *pval;
    size_t size;
    int status;
    struct tmp_buff {
        ELLNODE node;
        size_t size;
    };
    struct tmp_buff *pbuf = NULL;

    nativeElementCount = (*piiu->pva->p_pvNoElements) (pChan->id);

    /*
     * clip to the native count
     * and set to the native count if they specify zero
     */
    if (monix->miu.count > nativeElementCount || monix->miu.count == 0){
        count = nativeElementCount;
    }
    else {
        count = monix->miu.count;
    }

    size = dbr_size_n (monix->miu.type, count);

    if ( size <= sizeof(valbuf) ) {
        pval = (void *) &valbuf;
    }
    else {
         /*
          * find a preallocated block which fits
          * (stored with largest block first)
          */
         LOCK (piiu->iiu.pcas);
         pbuf = (struct tmp_buff *) ellFirst (&piiu->buffList);
         if (pbuf && pbuf->size >= size) {
             ellDelete (&piiu->buffList, &pbuf->node);
         }
         else {
             pbuf = NULL;
         }
         UNLOCK (piiu->iiu.pcas);
 
         /* 
          * test again so malloc is not inside LOCKED
          * section
          */
         if (!pbuf) {
             pbuf = (struct tmp_buff *) malloc(sizeof(*pbuf)+size);
             if (!pbuf) {
                 ca_printf (piiu->iiu.pcas,
                     "%s: No Mem, Event Discarded\n",
                     __FILE__);
                 return;
             }
             pbuf->size = size;
         }
         pval = (void *) (pbuf+1);
    }
    status = (*piiu->pva->p_pvGetField) (idIn, monix->miu.type, 
        pval, count, pfl);

    /*
     * Call user's callback
     */
    LOCK (piiu->iiu.pcas);
    if (monix->miu.usr_func) {
         struct event_handler_args args;
 
         args.usr = (void *) monix->miu.usr_arg;
         args.chid = monix->miu.pChan;
         args.type = monix->miu.type;
         args.count = count;
         args.dbr = pval;
 
         if (status) {
             args.status = ECA_GETFAIL;
         }
         else{
             args.status = ECA_NORMAL;
         }
 
         (*monix->miu.usr_func)(args);
    }
     
    /*
     * insert the buffer back into the que in size order if
     * one was used.
     */
    if(pbuf){
        struct tmp_buff     *ptbuf;

        for (ptbuf = (struct tmp_buff *) ellFirst (&piiu->buffList);
            ptbuf; ptbuf = (struct tmp_buff *) ellNext (&pbuf->node) ){

            if(ptbuf->size <= pbuf->size){
                break;
            }
        }
        if (ptbuf) {
            ptbuf = (struct tmp_buff *) ptbuf->node.previous;
        }

        ellInsert (&piiu->buffList, &ptbuf->node, &pbuf->node);
    }
    UNLOCK (piiu->iiu.pcas);
     
    return;
}

/*
 *  issue_get ()
 */
LOCAL int issue_get (ca_uint16_t cmd, nciu *chan, chtype type, 
    unsigned long count, void (*pfunc) (struct event_handler_args), void *pParam)
{
    int         status;
    caHdr       hdr;
    ca_uint16_t type_u16;
    ca_uint16_t count_u16;
    tcpiiu      *piiu;

    /* 
     * fail out if channel isnt connected or arguments are 
     * otherwise invalid
     */
    if (!chan->connected) {
        return ECA_DISCONNCHID;
    }
    if (INVALID_DB_REQ(type)) {
        return ECA_BADTYPE;
    }
    if (!chan->ar.read_access) {
        return ECA_NORDACCESS;
    }
    if (count > chan->count || count>0xffff) {
        return ECA_BADCOUNT;
    }
    if (count == 0) {
        if (cmd==CA_PROTO_READ_NOTIFY) {
            count = chan->count;
        }
        else {
            return ECA_BADCOUNT;
        }
    }

    /*
     * only after range checking type and count cast 
     * them down to a smaller size
     */
    type_u16 = (ca_uint16_t) type;
    count_u16 = (ca_uint16_t) count;

    LOCK (chan->ciu.piiu->pcas);
    {
        nmiu *monix = caIOBlockCreate (chan, cmd, type, count, pfunc, pParam);
        if (!monix) {
            UNLOCK (chan->ciu.piiu->pcas);
            return ECA_ALLOCMEM;
        }

        hdr.m_cmmd = htons (cmd);
        hdr.m_dataType = htons (type_u16);
        hdr.m_count = htons (count_u16);
        hdr.m_available = monix->id;
        hdr.m_postsize = 0;
        hdr.m_cid = chan->sid;
    }
    UNLOCK (chan->ciu.piiu->pcas);

    piiu = iiuToTCPIIU (chan->ciu.piiu);
    status = cac_push_tcp_msg (piiu, &hdr, NULL);
    if (status!=ECA_NORMAL && status!=ECA_DISCONNCHID) {
        /*
         * we need to be careful about touching the monix
         * pointer after the lock has been released
         */
        caIOBlockFree (chan->ciu.piiu->pcas, hdr.m_available);
    }

    return status;
}

/*
 * ca_array_get ()
 */
int epicsShareAPI ca_array_get (chtype type, unsigned long count, chid chanIn, void *pvalue)
{
    baseCIU *pChan = (baseCIU *) chanIn;
    int status;

    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);
        
        status = (*pChan->piiu->pcas->localIIU.pva->p_pvGetField) (
            pLocalChan->id, type, pvalue, count, NULL);
        if (status) {
            return ECA_GETFAIL;
        }
        else {
            return ECA_NORMAL;
        }
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        status = issue_get (CA_PROTO_READ, pNetChan, type, count, NULL, pvalue);
        if (status==ECA_NORMAL) {
            pChan->piiu->pcas->ca_pndrecvcnt++;
        }
        return status;
    }
}

/*
 * ca_array_get_callback ()
 */
int epicsShareAPI ca_array_get_callback (chtype type, unsigned long count, chid chanIn,
            void (*pfunc) (struct event_handler_args), void *arg)
{
    baseCIU *pChan = (baseCIU *) chanIn;

    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);
        lmiu ev;

        ev.miu.usr_func = pfunc;
        ev.miu.usr_arg = arg;
        ev.miu.pChan = pChan;
        ev.miu.type = type;
        ev.miu.count = count;
        ca_event_handler (&ev, pLocalChan->id, 0, NULL);
        return ECA_NORMAL;
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        return issue_get (CA_PROTO_READ_NOTIFY, pNetChan, type, count, pfunc, arg);
    }
}

/*
 * caIOBlockFree ()
 */
void caIOBlockFree (cac *pcac, unsigned id)
{
    nmiu    *pIOBlock;
    nciu    *pNetChan;

    LOCK (pcac);
    pIOBlock = (nmiu *) bucketLookupAndRemoveItemUnsignedId (
                    pcac->ca_pFastBucket, &id);
    if (!pIOBlock) {
        ca_printf (pcac, "CAC: Delete of invalid IO block identifier = %u ignored?\n", id);
        UNLOCK (pcac);
        return;
    }
    pIOBlock->id = ~0U; /* this id always invalid */
    pNetChan = ciuToNCIU (pIOBlock->miu.pChan);
    ellDelete (&pNetChan->eventq, &pIOBlock->node);
    freeListFree (pcac->ca_ioBlockFreeListPVT, pIOBlock);
    UNLOCK (pcac);
}

/*
 * check_a_dbr_string()
 */
LOCAL int check_a_dbr_string (const char *pStr, const unsigned count)
{
    unsigned i;

    for (i=0; i< count; i++) {
        unsigned int strsize;

        strsize = strlen(pStr) + 1;

        if (strsize>MAX_STRING_SIZE) {
            return ECA_STRTOBIG;
        }

        pStr += MAX_STRING_SIZE;
    }

    return ECA_NORMAL;
}

/*
 * malloc_put_convert()
 */
#ifdef CONVERSION_REQUIRED 
LOCAL void *malloc_put_convert (cac *pcac, unsigned long size)
{
    struct putCvrtBuf *pBuf;

    LOCK (pcac);
    while ( (pBuf = (struct putCvrtBuf *) ellGet(&pcac->putCvrtBuf)) ) {
        if(pBuf->size >= size){
            break;
        }
        else {
            free (pBuf);
        }
    }
    UNLOCK (pcac);

    if (!pBuf) {
        pBuf = (struct putCvrtBuf *) malloc (sizeof(*pBuf)+size);
        if (!pBuf) {
            return NULL;
        }
        pBuf->size = size;
        pBuf->pBuf = (void *) (pBuf+1);
    }

    return pBuf->pBuf;
}
#endif /* CONVERSION_REQUIRED */

/*
 * free_put_convert()
 */
#ifdef CONVERSION_REQUIRED 
LOCAL void free_put_convert(cac *pcac, void *pBuf)
{
    struct putCvrtBuf   *pBufHdr;

    pBufHdr = (struct putCvrtBuf *)pBuf;
    pBufHdr -= 1;
    assert (pBufHdr->pBuf == (void *)(pBufHdr+1));

    LOCK (pcac);
    ellAdd (&pcac->putCvrtBuf, &pBufHdr->node);
    UNLOCK (pcac);

    return;
}
#endif /* CONVERSION_REQUIRED */

/*
 * issue_put()
 */
LOCAL int issue_put (ca_uint16_t cmd, unsigned id, nciu *chan, chtype type, 
                     unsigned long count, const void *pvalue)
{ 
    int status;
    caHdr hdr;
    unsigned postcnt;
    ca_uint16_t type_u16;
    ca_uint16_t count_u16;
    void *pCvrtBuf;
    tcpiiu *piiu;

    /* 
     * fail out if the conn is down or the arguments are otherwise invalid
     */
    if (!chan->connected) {
        return ECA_DISCONNCHID;
    }
    if (INVALID_DB_REQ(type)) {
        return ECA_BADTYPE;
    }
    /*
     * compound types not allowed
     */
    if (dbr_value_offset[type]) {
        return ECA_BADTYPE;
    }
    if (!chan->ar.write_access) {
        return ECA_NOWTACCESS;
    }
    if ( count > chan->count || count > 0xffff || count == 0 ) {
            return ECA_BADCOUNT;
    }
    if (type==DBR_STRING) {
        status = check_a_dbr_string ( (char *) pvalue, count );
        if (status != ECA_NORMAL) {
            return status;
        }
    }
    postcnt = dbr_size_n (type,count);
    if (postcnt>0xffff) {
        return ECA_TOLARGE;
    }

    /*
     * only after range checking type and count cast 
     * them down to a smaller size
     */
    type_u16 = (ca_uint16_t) type;
    count_u16 = (ca_uint16_t) count;

    if (type == DBR_STRING && count == 1) {
        char *pstr = (char *)pvalue;

        postcnt = strlen(pstr)+1;
    }

#   ifdef CONVERSION_REQUIRED 
    {
        unsigned i;
        void *pdest;
        unsigned size_of_one;

        size_of_one = dbr_size[type];

        pCvrtBuf = pdest = malloc_put_convert (chan->ciu.piiu->pcas, postcnt);
        if (!pdest) {
            return ECA_ALLOCMEM;
        }

        /*
         * No compound types here because these types are read only
         * and therefore only appropriate for gets or monitors
         *
         * I changed from a for to a while loop here to avoid bounds
         * checker pointer out of range error, and unused pointer
         * update when it is a single element.
         */
        i=0;
        while (TRUE) {
            switch (type) {
            case    DBR_LONG:
                *(dbr_long_t *)pdest = htonl (*(dbr_long_t *)pvalue);
                break;

            case    DBR_CHAR:
                *(dbr_char_t *)pdest = *(dbr_char_t *)pvalue;
                break;

            case    DBR_ENUM:
            case    DBR_SHORT:
            case    DBR_PUT_ACKT:
            case    DBR_PUT_ACKS:
#           if DBR_INT != DBR_SHORT
#               error DBR_INT != DBR_SHORT ?
#           endif /*DBR_INT != DBR_SHORT*/
                *(dbr_short_t *)pdest = htons (*(dbr_short_t *)pvalue);
                break;

            case    DBR_FLOAT:
                dbr_htonf ((dbr_float_t *)pvalue, (dbr_float_t *)pdest);
                break;

            case    DBR_DOUBLE: 
                dbr_htond ((dbr_double_t *)pvalue, (dbr_double_t *)pdest);
            break;

            case    DBR_STRING:
                /*
                 * string size checked above
                 */
                strcpy ( (char *) pdest, (char *) pvalue );
                break;

            default:
                return ECA_BADTYPE;
            }

            if (++i>=count) {
                break;
            }

            pdest = ((char *)pdest) + size_of_one;
            pvalue = ((char *)pvalue) + size_of_one;
        }

        pvalue = pCvrtBuf;
    }
#   endif /*CONVERSION_REQUIRED*/

    hdr.m_cmmd = htons (cmd);
    hdr.m_dataType = htons (type_u16);
    hdr.m_count = htons (count_u16);
    hdr.m_cid = chan->sid;
    hdr.m_available = id;
    hdr.m_postsize = (ca_uint16_t) postcnt;

    piiu = iiuToTCPIIU (chan->ciu.piiu);
    status = cac_push_tcp_msg (piiu, &hdr, pvalue);

#   ifdef CONVERSION_REQUIRED
        free_put_convert (chan->ciu.piiu->pcas, pCvrtBuf);
#   endif /*CONVERSION_REQUIRED*/

    return status;
}

/*
 *  ca_put_notify_action
 */
LOCAL void ca_put_notify_action (void *pPrivate)
{
    lciu *pChan = (lciu *) pPrivate;
    lclIIU *pliiu = iiuToLIIU (pChan->ciu.piiu);

    /*
     * independent lock used here in order to
     * avoid any possibility of blocking
     * the database (or indirectly blocking
     * one client on another client).
     */
    semMutexMustTake (pliiu->putNotifyLock);
    ellAdd (&pliiu->putNotifyQue, &pChan->ppn->node);
    semMutexGive (pliiu->putNotifyLock);

    /*
     * offload the labor for this to the
     * event task so that we never block
     * the db or another client.
     */
    (*pliiu->pva->p_pvEventQueuePostExtraLabor) (pliiu->evctx);
}

/*
 * localPutNotifyInitiate ()
 */
int localPutNotifyInitiate (lciu *pChan, chtype type, unsigned long count, 
    const void *pValue, void (*pCallback)(struct event_handler_args), void *usrArg)
{
    lclIIU *pliiu = iiuToLIIU (pChan->ciu.piiu);
    unsigned size;
    long dbStatus;

    size = dbr_size_n (type, count);

    LOCK (pChan->ciu.piiu->pcas);

    if (pChan->ppn) {
        /*
         * wait while it is busy
         */
        while (pChan->ppn->dbPutNotify) {
            semTakeStatus semStatus;
            UNLOCK (pChan->ciu.piiu->pcas);
            semStatus = semBinaryTakeTimeout (
                pChan->ciu.piiu->pcas->ca_blockSem, 60.0);
            if (semStatus != semTakeOK) {
                return ECA_PUTCBINPROG;
            }
            LOCK (pChan->ciu.piiu->pcas);
        }

        /*
         * once its not busy then free the current
         * block if it is too small
         */
        if ( pChan->ppn->valueSize < size ) {
            free ( pChan->ppn );
            pChan->ppn = NULL;
        }
    }

    if ( !pChan->ppn ) {

        pChan->ppn = (caPutNotify *) calloc (1, sizeof(*pChan->ppn)+size);
        if ( !pChan->ppn ) {
            UNLOCK (pChan->ciu.piiu->pcas);
            return ECA_ALLOCMEM;
        }
    }
    pChan->ppn->pValue = pChan->ppn + 1;
    memcpy (pChan->ppn->pValue, pValue, size);
    pChan->ppn->caUserCallback = pCallback;
    pChan->ppn->caUserArg = usrArg;

    dbStatus = (*pliiu->pva->p_pvPutNotifyInitiate) 
        (pChan->id, type, count, pChan->ppn->pValue, ca_put_notify_action, pChan, 
        &pChan->ppn->dbPutNotify);
    UNLOCK (pChan->ciu.piiu->pcas);
    if (dbStatus!=0 && dbStatus!=S_db_Pending) {
        errMessage (dbStatus, "CAC: unable to initiate put callback\n");
        if (dbStatus==S_db_Blocked) {
            return ECA_PUTCBINPROG;
        }
        return ECA_PUTFAIL;
    }
    return ECA_NORMAL;
}

/*
 *  ca_array_put_callback ()
 */
int epicsShareAPI ca_array_put_callback (chtype type, unsigned long count, 
    chid pChanIn, const void *pvalue, void (*pfunc)(struct event_handler_args), 
    void *usrarg)
{
    baseCIU *pChan = (baseCIU *) pChanIn;
    int status;
    unsigned id;

    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);

        return localPutNotifyInitiate (pLocalChan, type, count, pvalue, pfunc, usrarg);
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        tcpiiu *piiu;
        nmiu *monix;

        if ( !pNetChan->connected ) {
            return ECA_DISCONNCHID;
        }

        piiu = iiuToTCPIIU (pChan->piiu);

        if (!CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number))  {
            return ECA_NOSUPPORT;
        }

        /*
         * lock around io block create and list add
         * so that we are not deleted without
         * reclaiming the resource
         */
        LOCK (pChan->piiu->pcas);

        monix = caIOBlockCreate (pNetChan, CA_PROTO_WRITE_NOTIFY, 
                    type, count, pfunc, usrarg);
        if (!monix) {
            UNLOCK (pChan->piiu->pcas);
            return ECA_ALLOCMEM;
        }

        id = monix->id;
    
        UNLOCK (pChan->piiu->pcas);

        status = issue_put (CA_PROTO_WRITE_NOTIFY, id, pNetChan, type, count, pvalue);
        if (status!=ECA_NORMAL && status!=ECA_DISCONNCHID) {
            /*
             * we need to be careful about touching the monix
             * pointer after the lock has been released
             */
            caIOBlockFree (pChan->piiu->pcas, id);
        }
        return status;
    }
}

/*
 *  ca_array_put ()
 */
int epicsShareAPI ca_array_put (chtype type, unsigned long count, chid pChanIn, const void *pvalue)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    /*
     * If channel is on this client's host then
     * call the database directly
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);
        int status;

        status = (*pChan->piiu->pcas->localIIU.pva->p_pvPutField) (pLocalChan->id,  
                                type, pvalue, count);            
        if (status) {
            return ECA_PUTFAIL;
        }
        else {
            return ECA_NORMAL;
        }
    }
    else {
        return issue_put (CA_PROTO_WRITE, ~0U, ciuToNCIU (pChan), type, count, pvalue);
    }
}

/*
 *  Specify an event subroutine to be run for connection events
 */
int epicsShareAPI ca_change_connection_event (chid pChanIn, void (*pfunc)(struct connection_handler_args))
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    if (!pChan->piiu) {
        return ECA_BADCHID;
    }

    if (pChan->pConnFunc == pfunc) {
        return ECA_NORMAL;
    }

    LOCK (pChan->piiu->pcas);
    if (pChan->piiu != &pChan->piiu->pcas->localIIU.iiu) {
        nciu *pNetChan = ciuToNCIU (pChan);

        if (!pNetChan->previousConn) {
            if (!pChan->pConnFunc) {
                if (--pChan->piiu->pcas->ca_pndrecvcnt==0u) {
                    semBinaryGive (pChan->piiu->pcas->ca_io_done_sem);
                }
            }
            if (!pfunc) {
                pChan->piiu->pcas->ca_pndrecvcnt++;
            }
        }
    }
    pChan->pConnFunc = pfunc;
    UNLOCK (pChan->piiu->pcas);

    return ECA_NORMAL;
}

/*
 * ca_replace_access_rights_event
 */
int epicsShareAPI ca_replace_access_rights_event (chid pChanIn, void (*pfunc)(struct access_rights_handler_args))
{
    baseCIU *pChan = (baseCIU *) pChanIn;
    struct access_rights_handler_args args;
    caar ar;
    int connected;

    if (!pChan->piiu) {
        return ECA_BADCHID;
    }

    LOCK (pChan->piiu->pcas);
    pChan->pAccessRightsFunc = pfunc;

    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        connected = TRUE;
        ar.read_access = TRUE;
        ar.write_access = TRUE;
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        if (pNetChan->connected) {
            connected = TRUE;
        }
        else {
            connected = FALSE;
        }
        ar = pNetChan->ar;
    }

    /*
     * make certain that it runs at least once
     */
    if ( connected && pChan->pAccessRightsFunc ) {
        args.chid = (chid) pChan;
        args.ar = ar;
        (*pChan->pAccessRightsFunc)(args);
    }

    UNLOCK (pChan->piiu->pcas);

    return ECA_NORMAL;
}

/*
 *  Specify an event subroutine to be run for asynch exceptions
 */
int epicsShareAPI ca_add_exception_event
    (void (*pfunc)(struct exception_handler_args), void *arg)
{
    cac *pcac;
    int caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }
    
    LOCK (pcac);
    if (pfunc) {
        pcac->ca_exception_func = pfunc;
        pcac->ca_exception_arg = arg;
    }
    else {
        pcac->ca_exception_func = ca_default_exception_handler;
        pcac->ca_exception_arg = NULL;
    }
    UNLOCK (pcac);

    return ECA_NORMAL;
}

/*
 *  ca_add_masked_array_event
 */
int epicsShareAPI ca_add_masked_array_event (chtype type, unsigned long count, chid pChanIn, 
        void (*pCallBack)(struct event_handler_args), void *pCallBackArg, ca_real p_delta, 
        ca_real n_delta, ca_real timeout, evid *monixptr, long mask)
{
    baseCIU *pChan = (baseCIU *) pChanIn;
    baseMIU *pMon;
    int status;

    if (!pChan->piiu) {
        return ECA_BADCHID;
    }

    if ( INVALID_DB_REQ(type) ) {
        return ECA_BADTYPE;
    }

    /*
     * Check for huge waveform
     *
     * (the count is not checked here against the native count
     * when connected because this introduces a race condition 
     * for the client tool - the requested count is clipped to 
     * the actual count when the monitor request is sent so
     * verifying that the requested count is valid here isnt
     * required)
     */
    if (dbr_size_n(type,count)>MAX_MSG_SIZE-sizeof(caHdr)) {
        return ECA_TOLARGE;
    }

    if (pCallBack==NULL) {
        return ECA_BADFUNCPTR;
    }

    if (mask&USHRT_MAX==0) {
        return ECA_BADMASK;
    }

    /*
     * lock around io block create and list add
     * so that we are not deleted while
     * creating the resource
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);
        lmiu *pLclMon;

        LOCK (pChan->piiu->pcas);

        pLclMon = (lmiu *) freeListMalloc (pChan->piiu->pcas->localIIU.localSubscrFreeListPVT);
        if (!pLclMon) {
            UNLOCK (pChan->piiu->pcas);
            return ECA_ALLOCMEM;
        }

        pLclMon->miu.type =         type;
        pLclMon->miu.usr_func =     pCallBack;
        pLclMon->miu.usr_arg =      pCallBackArg;
        pLclMon->miu.count =        count;
        pLclMon->miu.pChan =        pChan;

        pLclMon->es = (*pChan->piiu->pcas->localIIU.pva->p_pvEventQueueAddEvent) 
                (pChan->piiu->pcas->localIIU.evctx,
                pLocalChan->id, ca_event_handler, pLclMon, mask);

        if (!pLclMon->es) {
            freeListFree (pChan->piiu->pcas->localIIU.localSubscrFreeListPVT, pLclMon); 
            UNLOCK (pChan->piiu->pcas);
            return ECA_ALLOCMEM; 
        }

        ellAdd (&pLocalChan->eventq, &pLclMon->node);

        UNLOCK (pChan->piiu->pcas);

        (*pChan->piiu->pcas->localIIU.pva->p_pvEventQueuePostSingleEvent)  (pLclMon->es);

        pMon = &pLclMon->miu;
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        nmiu *pNetMon;
        unsigned id;

        LOCK (pChan->piiu->pcas);

        pNetMon = caIOBlockCreate (pNetChan, CA_PROTO_EVENT_ADD, 
                type, count, pCallBack, pCallBackArg);
        if (!pNetMon) {
            UNLOCK (pChan->piiu->pcas);
            return ECA_ALLOCMEM;
        }

        pNetMon->p_delta =          p_delta;
        pNetMon->n_delta =          n_delta;
        pNetMon->timeout =          timeout;
        pNetMon->mask =             (unsigned short) mask;

        id = pNetMon->id;
        UNLOCK (pChan->piiu->pcas);
        if (pNetChan->connected) {
            status = ca_request_event (pNetChan, pNetMon);
            if (status != ECA_NORMAL) {
                if ( !pNetChan->connected ) {
                    caIOBlockFree (pNetChan->ciu.piiu->pcas, id);
                }
                return status;
            }
        }
        pMon = &pNetMon->miu;
    }

    if (monixptr) {
        *monixptr = pMon;
    }

    return ECA_NORMAL;
}

/*
 *  CA_REQUEST_EVENT()
 */
int ca_request_event (nciu *pNetChan, nmiu *pNetMon)
{
    int                 status;
    unsigned long       count;
    struct monops       msg;
    ca_float32_t        p_delta;
    ca_float32_t        n_delta;
    ca_float32_t        tmo;
    
    /* 
     * dont send the message if the conn is down 
     * (it will be sent once connected)
     */
    if (!pNetChan->connected) {
        return ECA_DISCONNCHID;
    }

    /*
     * clip to the native count and set to the native count if they
     * specify zero
     */
    if (pNetMon->miu.count > pNetChan->count || pNetMon->miu.count == 0){
        count = pNetChan->count;
    }
    else {
        count = pNetMon->miu.count;
    }

    /*
     * dont allow overflow when converting to ca_uint16_t
     */
    if (count>0xffff) {
        count = 0xffff;
    }

    /* msg header    */
    msg.m_header.m_cmmd = htons (CA_PROTO_EVENT_ADD);
    msg.m_header.m_available = pNetMon->id;
    msg.m_header.m_dataType = htons ( ((ca_uint16_t)pNetMon->miu.type) );
    msg.m_header.m_count = htons ( ((ca_uint16_t)count) );
    msg.m_header.m_cid = pNetChan->sid;
    msg.m_header.m_postsize = sizeof (msg.m_info);

    /* msg body  */
    p_delta = (ca_float32_t) pNetMon->p_delta;
    n_delta = (ca_float32_t) pNetMon->n_delta;
    tmo = (ca_float32_t) pNetMon->timeout;
    dbr_htonf (&p_delta, &msg.m_info.m_hval);
    dbr_htonf (&n_delta, &msg.m_info.m_lval);
    dbr_htonf (&tmo, &msg.m_info.m_toval);
    msg.m_info.m_mask = htons (pNetMon->mask);
    msg.m_info.m_pad = 0; /* allow future use */    

    status = cac_push_tcp_msg (iiuToTCPIIU(pNetChan->ciu.piiu), &msg.m_header, &msg.m_info);

    return status;
}


/*
 *
 *  ca_clear_event ()
 *
 *  Cancel an outstanding event for a channel.
 *
 *  NOTE: returns before operation completes in the server 
 *  (and the client). 
 *  This is a good reason not to allow them to make the monix 
 *  block as part of a larger structure.
 *  Nevertheless the caller is gauranteed that his specified
 *  event is disabled and therefore will not run (from this source)
 *  after leaving this routine.
 *
 */
int epicsShareAPI ca_clear_event (evid evidIn)
{
    baseMIU     *pMon = (baseMIU *) evidIn;
    int         status;

    /* disable any further events from this event block */
    pMon->usr_func = NULL;

    /*
     * is it a local event subscription ?
     */
    if (pMon->pChan->piiu == &pMon->pChan->piiu->pcas->localIIU.iiu) {
        localMonitorResourceDestroy (miuToLMIU (pMon));
        status = ECA_NORMAL;
    }
    else {
        nmiu        *pNetMIU = miuToNMIU (pMon);
        nciu        *pNetCIU = ciuToNCIU (pMon->pChan);

        if (pNetCIU->connected) {
            caHdr hdr;
            ca_uint16_t type, count;
            
            type = (ca_uint16_t) pNetCIU->type;
            if (pNetCIU->count>0xffff) {
                count = 0xffff;
            }
            else {
                count = (ca_uint16_t) pNetCIU->count;
            }

            hdr.m_cmmd = htons (CA_PROTO_EVENT_CANCEL);
            hdr.m_available = pNetMIU->id;
            hdr.m_dataType = htons (type);
            hdr.m_count = htons (count);
            hdr.m_cid = pNetCIU->sid;
            hdr.m_postsize = 0;
    
            status = cac_push_tcp_msg (iiuToTCPIIU(pMon->pChan->piiu), &hdr, NULL);
        }
        else {
            status = ECA_NORMAL;
        }

        caIOBlockFree (pMon->pChan->piiu->pcas, pNetMIU->id);
    }

    return status;
}

/*
 *
 *  ca_clear_channel ()
 *
 *  clear the resources allocated for a channel by search
 *
 *  NOTE: returns before operation completes in the server 
 *  (and the client). 
 *  This is a good reason not to allow them to make the monix 
 *  block part of a larger structure.
 *  Nevertheless the caller is gauranteed that his specified
 *  event is disabled and therefore will not run 
 *  (from this source) after leaving this routine.
 *
 */
int epicsShareAPI ca_clear_channel (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;
    int status;

    /*
     * is it a local channel ?
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        localChannelDestroy (iiuToLIIU(pChan->piiu), ciuToLCIU(pChan));
        status = ECA_NORMAL;
    }
    else {
        nciu *pNetCIU = ciuToNCIU (pChan);

        if (pNetCIU->connected) {
            caHdr hdr;

            hdr.m_cmmd = htons (CA_PROTO_CLEAR_CHANNEL);
            hdr.m_available = pNetCIU->cid;
            hdr.m_cid = pNetCIU->sid;
            hdr.m_dataType = htons (0);
            hdr.m_count = htons (0);
            hdr.m_postsize = 0;

            status = cac_push_tcp_msg (iiuToTCPIIU(pChan->piiu), &hdr, NULL);
        }
        else {
            status = ECA_NORMAL;
        }

        netChannelDestroy (pChan->piiu->pcas, pNetCIU->cid);
    }
    return status;
}

/*
 * netChannelDestroy ()
 */
void netChannelDestroy (cac *pcac, unsigned id)
{
    nciu *chan;
    nmiu *monix;
    nmiu *next;

    LOCK (pcac);

    chan = (nciu *) bucketLookupAndRemoveItemUnsignedId (pcac->ca_pSlowBucket, &id);
    if (chan==NULL) {
        UNLOCK (pcac);
        genLocalExcep (pcac, ECA_BADCHID, "netChannelDestroy()");
        return;
    }

    /*
     * if this channel does not have a connection handler
     * and it has not connected for the first time then clear the
     * outstanding IO count
     */
    if (!chan->previousConn && !chan->ciu.pConnFunc) {
        if (--pcac->ca_pndrecvcnt==0u) {
            semBinaryGive (pcac->ca_io_done_sem);
        }
    }

    /*
     * remove any IO blocks still attached to this channel
     */
    for (monix = (nmiu *) ellFirst (&chan->eventq);
         monix; monix = next) {

        next = (nmiu *) ellNext (&monix->node);

        caIOBlockFree (pcac, monix->id);
    }

    removeFromChanList (chan);

    /* 
     * attempt to catch use of this channel after it
     * is returned to pool
     */
    chan->ciu.piiu = NULL; 

    free (chan);
    
    UNLOCK (pcac);
}

#if 0
/*
 * cacSetPushPending ()
 */
LOCAL void cacSetPushPending (tcpiiu *piiu)
{
    unsigned long sendCnt;

    cacRingBufferWriteFlush (&piiu->send);

    sendCnt = cacRingBufferReadSize (&piiu->send, TRUE);
    if (sendCnt) {
        piiu->pushPending = TRUE;
        cacSetSendPending (piiu);
    }
}
#endif

/*
 * cacFlushAllIIU ()
 */
void cacFlushAllIIU (cac *pcac)
{
    tcpiiu *piiu;

    /*
     * set the push pending flag on all virtual circuits
     */
    LOCK (pcac);
    for ( piiu = (tcpiiu *) ellFirst (&pcac->ca_iiuList);
        piiu; piiu = (tcpiiu *) ellNext (&piiu->node) ) {
        cacRingBufferWriteFlush (&piiu->send);
    }
    UNLOCK (pcac);
}

/*
 * noopConnHandler()
 * This is installed into channels which dont have
 * a connection handler when ca_pend_io() times
 * out so that we will not decrement the pending
 * recv count in the future.
 */
LOCAL void noopConnHandler(struct  connection_handler_args args)
{
}

/*
 *
 * set pending IO count back to zero and
 * send a sync to each IOC and back. dont
 * count reads until we recv the sync
 *
 */
LOCAL void ca_pend_io_cleanup (cac *pcac)
{
    tcpiiu *piiu;
    nciu *pchan;

    LOCK (pcac);

    pchan = (nciu *) ellFirst (&pcac->pudpiiu->niiu.chidList);
    while (pchan) {
        if (!pchan->ciu.pConnFunc) {
            pchan->ciu.pConnFunc = noopConnHandler;
        }
        pchan = (nciu *) ellNext (&pchan->node);
    }

    for (piiu = (tcpiiu *) ellFirst (&pcac->ca_iiuList);
        piiu; piiu = (tcpiiu *) ellNext (&piiu->node.next) ){

        caHdr hdr;

        if ( piiu->state != iiu_connected ) {
            continue;
        }

        piiu->cur_read_seq++;

        hdr = nullmsg;
        hdr.m_cmmd = htons (CA_PROTO_READ_SYNC);
        cac_push_tcp_msg (piiu, &hdr, NULL);
    }
    UNLOCK (pcac);
    pcac->ca_pndrecvcnt = 0u;
}

/*
 * ca_pend_private ()
 */
static int ca_pend_private (cac *pcac, ca_real timeout, int early)
{
    TS_STAMP    beg_time;
    TS_STAMP    cur_time;
    double      delay;
    int         caStatus;

    caStatus = tsStampGetCurrent (&cur_time);
    if (caStatus != 0) {
        return ECA_INTERNAL;
    }

    LOCK (pcac);
    pcac->currentTime = cur_time;
    UNLOCK (pcac);

    cacFlushAllIIU (pcac);

    if (pcac->ca_pndrecvcnt==0u && early) {
        return ECA_NORMAL;
    }
   
    if (timeout<0.0) {
        if (early) ca_pend_io_cleanup (pcac);
        return ECA_TIMEOUT;
    }

    beg_time = cur_time;
    delay = 0.0;
    while (TRUE) {
        ca_real  remaining;
        
        if(timeout == 0.0){
            remaining = 60.0;
        }
        else{
            remaining = timeout-delay;
            remaining = min (60.0, remaining);

            /*
             * If we are not waiting for any significant delay
             * then force the delay to zero so that we avoid
             * scheduling delays (which can be substantial
             * on some os)
             */
            if (remaining <= CAC_SIGNIFICANT_SELECT_DELAY) {
                if (early) ca_pend_io_cleanup (pcac);
                return ECA_TIMEOUT;
            }
        }    
        
        /* recv occurs in another thread */
        semBinaryTakeTimeout (pcac->ca_io_done_sem, remaining);

        if (pcac->ca_pndrecvcnt==0 && early) {
            return ECA_NORMAL;
        }
 
        /* force time update */
        caStatus = tsStampGetCurrent (&cur_time);
        if (caStatus != 0) {
            if (early) ca_pend_io_cleanup (pcac);
            return ECA_INTERNAL;
        }

        LOCK (pcac);
        pcac->currentTime = cur_time;
        UNLOCK (pcac);

        if (timeout != 0.0) {
            delay = tsStampDiffInSeconds (&cur_time, &beg_time);
        }
    }
}

/*
 * ca_pend ()
 */
int epicsShareAPI ca_pend (ca_real timeout, int early)
{
    cac *pcac;
    int status;
    void *p;

    status = fetchClientContext (&pcac);
    if ( status != ECA_NORMAL ) {
        return status;
    }

    /*
     * dont allow recursion
     */
    p = threadPrivateGet (cacRecursionLock);
    if (p) {
        return ECA_EVDISALLOW;
    }

    threadPrivateSet (cacRecursionLock, &cacRecursionLock);

    status = ca_pend_private (pcac, timeout, early);

    threadPrivateSet (cacRecursionLock, NULL);

    return status;
}

#if 0
/*
 * cac_fetch_poll_period()
 */
double cac_fetch_poll_period (cac *pcac)
{
    if (!pcac->pudpiiu) {
        return SELECT_POLL_NO_SEARCH;
    }
    else if ( ellCount (&pcac->pudpiiu->niiu.chidList) == 0 ) {
        return SELECT_POLL_NO_SEARCH;
    }
    else {
        return SELECT_POLL_SEARCH;
    }
}
#endif /* #if 0 */

/*
 *  CA_FLUSH_IO()
 *  reprocess connection state and
 *  flush the send buffer 
 *
 *  Wait for all send buffers to be flushed
 *  while performing socket io and processing recv backlog
 */ 
int epicsShareAPI ca_flush_io ()
{
    int             caStatus;
    cac       *pcac;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    cacFlushAllIIU (pcac);

    return ECA_NORMAL;
}

/*
 *  CA_TEST_IO ()
 */
int epicsShareAPI ca_test_io ()
{
    int         caStatus;
    cac   *pcac;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if (pcac->ca_pndrecvcnt==0u) {
        return ECA_IODONE;
    }
    else{
        return ECA_IOINPROGRESS;
    }
}

/*
 * genLocalExcepWFL ()
 * (generate local exception with file and line number)
 */
void genLocalExcepWFL (cac *pcac, long stat, const char *ctx, const char *pFile, unsigned lineNo)
{
    struct exception_handler_args   args;

    args.chid = NULL;
    args.type = -1;
    args.count = 0u;
    args.addr = NULL;
    args.stat = stat;
    args.op = CA_OP_OTHER;
    args.ctx = ctx;
    args.pFile = pFile;
    args.lineNo = lineNo;

    /*
     * dont lock if there is no CA context
     */
    if (pcac==NULL) {
        args.usr = NULL;
        ca_default_exception_handler (args);
    }
    /*
     * NOOP if they disable exceptions
     */
    else if (pcac->ca_exception_func!=NULL) {
        args.usr = pcac->ca_exception_arg;

        LOCK (pcac);
        (*pcac->ca_exception_func) (args);
        UNLOCK (pcac);
    }
}

/*
 *  CA_SIGNAL()
 */
void epicsShareAPI ca_signal (long ca_status, const char *message)
{
    ca_signal_with_file_and_lineno (ca_status, message, NULL, 0);
}

/*
 * ca_message (long ca_status)
 *
 * - if it is an unknown error code then it possible
 * that the error string generated below 
 * will be overwritten before (or while) the caller
 * of this routine is calling this routine
 * (if they call this routine again).
 */
READONLY char * epicsShareAPI ca_message (long ca_status)
{
    unsigned msgNo = CA_EXTRACT_MSG_NO (ca_status);

    if( msgNo < NELEMENTS (ca_message_text) ){
        return ca_message_text[msgNo];
    }
    else {
        return "new CA message number known only by server - see caerr.h";
    }
}

/*
 * ca_signal_with_file_and_lineno()
 */
void epicsShareAPI ca_signal_with_file_and_lineno (long ca_status, 
            const char *message, const char *pfilenm, int lineno)
{
    ca_signal_formated (ca_status, pfilenm, lineno, message);
}

/*
 * ca_signal_formated()
 */
void epicsShareAPI ca_signal_formated (long ca_status, const char *pfilenm, 
                                       int lineno, const char *pFormat, ...)
{
    cac           *pcac;
    va_list             theArgs;
    static const char   *severity[] = 
    {
        "Warning",
        "Success",
        "Error",
        "Info",
        "Fatal",
        "Fatal",
        "Fatal",
        "Fatal"
    };

    if (caClientContextId) {
        pcac = (cac *) threadPrivateGet (caClientContextId);
    }
    else {
        pcac = NULL;
    }
    
    va_start (theArgs, pFormat);  
    
    ca_printf (pcac,
        "CA.Client.Diagnostic..............................................\n");
    
    ca_printf (pcac,
        "    %s: \"%s\"\n", 
        severity[CA_EXTRACT_SEVERITY(ca_status)], 
        ca_message (ca_status));

    if  (pFormat) {
        ca_printf (pcac, "    Context: \"");
        ca_vPrintf (pcac, pFormat, theArgs);
        ca_printf (pcac, "\"\n");
    }
        
    if (pfilenm) {
        ca_printf (pcac,
            "    Source File: %s Line Number: %d\n",
            pfilenm,
            lineno);    
    }
    
    /*
     *  Terminate execution if unsuccessful
     */
    if( !(ca_status & CA_M_SUCCESS) && 
        CA_EXTRACT_SEVERITY(ca_status) != CA_K_WARNING ){
        abort();
    }
    
    ca_printf (pcac,
        "..................................................................\n");
    
    va_end (theArgs);
}


/*
 *  ca_busy_message ()
 */
int ca_busy_message (tcpiiu *piiu)
{
    caHdr hdr;
    int status;

    hdr = nullmsg;
    hdr.m_cmmd = htons (CA_PROTO_EVENTS_OFF);
    hdr.m_postsize = 0;
    
    status = cac_push_tcp_msg (piiu, &hdr, NULL);
    if (status == ECA_NORMAL) {
        cacRingBufferWriteFlush (&piiu->send);
    }
    return status;
}

/*
 * ca_ready_message ()
 */
int ca_ready_message (tcpiiu *piiu)
{
    caHdr hdr;
    int status;

    hdr = nullmsg;
    hdr.m_cmmd = htons (CA_PROTO_EVENTS_ON);
    hdr.m_postsize = 0;
    
    status = cac_push_tcp_msg (piiu, &hdr, NULL);
    if (status == ECA_NORMAL) {
        cacRingBufferWriteFlush (&piiu->send);
    }
    return status;
}

/*
 * echo_request ()
 */
void echo_request (tcpiiu *piiu)
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
    if (cac_push_tcp_msg_no_block (piiu, &hdr, NULL)) {
        piiu->echoPending = TRUE;
        LOCK (piiu->niiu.iiu.pcas);
        piiu->timeAtEchoRequest = piiu->niiu.iiu.pcas->currentTime;
        UNLOCK (piiu->niiu.iiu.pcas)
        cacRingBufferWriteFlush (&piiu->send);
    }
}

/*
 * noop_msg ()
 */
void noop_msg (tcpiiu *piiu)
{
    caHdr   hdr;
    bool     status;

    hdr.m_cmmd = htons (CA_PROTO_NOOP);
    hdr.m_dataType = htons (0);
    hdr.m_count = htons (0);
    hdr.m_cid = htons (0);
    hdr.m_available = htons (0);
    hdr.m_postsize = 0;
    
    status = cac_push_tcp_msg_no_block (piiu, &hdr, NULL);
    if (status) {
        cacRingBufferWriteFlush (&piiu->send);
    }
}

/*
 * issue_client_host_name ()
 */
void issue_client_host_name (tcpiiu *piiu)
{
    unsigned    size;
    caHdr       hdr;
    char        *pName;

    if (!CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number)) {
        return;
    }

    /*
     * allocate space in the outgoing buffer
     */
    pName = piiu->niiu.iiu.pcas->ca_pHostName, 
    size = strlen (pName) + 1;
    hdr = nullmsg;
    hdr.m_cmmd = htons(CA_PROTO_HOST_NAME);
    hdr.m_postsize = size;
    
    cac_push_tcp_msg (piiu, &hdr, pName);

    return;
}

/*
 * issue_identify_client ()
 */
void issue_identify_client (tcpiiu *piiu)
{
    unsigned    size;
    caHdr       hdr;
    char        *pName;

    if (!CA_V41(CA_PROTOCOL_VERSION, piiu->minor_version_number)) {
        return;
    }

    /*
     * allocate space in the outgoing buffer
     */
    pName = piiu->niiu.iiu.pcas->ca_pUserName, 
    size = strlen (pName) + 1;
    hdr = nullmsg;
    hdr.m_cmmd = htons (CA_PROTO_CLIENT_NAME);
    hdr.m_postsize = size;
    
    cac_push_tcp_msg (piiu, &hdr, pName);

    return;
}

/*
 * issue_claim_channel ()
 */
bool issue_claim_channel (nciu *pchan)
{
    tcpiiu      *pNetIIU;
    caHdr       hdr;
    unsigned    size;
    const char  *pStr;
    bool        success;

    LOCK (pchan->ciu.piiu->pcas);

    if (pchan->ciu.piiu == &pchan->ciu.piiu->pcas->pudpiiu->niiu.iiu) {
        ca_printf (pchan->ciu.piiu->pcas, 
            "CAC: UDP claim attempted?\n");
        UNLOCK (pchan->ciu.piiu->pcas);
        return false;
    }

    pNetIIU = iiuToTCPIIU (pchan->ciu.piiu);

    if (!pchan->claimPending) {
        ca_printf (pchan->ciu.piiu->pcas,
            "CAC: duplicate claim attempted (while disconnected)?\n");
        UNLOCK (pchan->ciu.piiu->pcas);
        return false;
    }

    if (pchan->connected) {
        ca_printf (pchan->ciu.piiu->pcas,
            "CAC: duplicate claim attempted (while connected)?\n");
        UNLOCK (pchan->ciu.piiu->pcas);
        return false;
    }

    hdr = nullmsg;
    hdr.m_cmmd = htons (CA_PROTO_CLAIM_CIU);

    if (CA_V44(CA_PROTOCOL_VERSION, pNetIIU->minor_version_number)) {
        hdr.m_cid = pchan->cid;
        pStr = ca_name (&pchan->ciu);
        size = pchan->nameLength;
    }
    else {
        hdr.m_cid = pchan->sid;
        pStr = NULL;
        size = 0u;
    }

    hdr.m_postsize = size;

    /*
     * The available field is used (abused)
     * here to communicate the minor version number
     * starting with CA 4.1.
     */
    hdr.m_available = htonl (CA_MINOR_VERSION);

    /*
     * If we are out of buffer space then postpone this
     * operation until later. This avoids any possibility
     * of a push pull deadlock (since this is sent when 
     * parsing the UDP input buffer).
     */
    success = cac_push_tcp_msg_no_block (pNetIIU, &hdr, pStr);
    if ( success ) {

        /*
         * move to the end of the list once the claim has been sent
         */
        pchan->claimPending = FALSE;
        ellDelete (&pNetIIU->niiu.chidList, &pchan->node);
        ellAdd (&pNetIIU->niiu.chidList, &pchan->node);

        if (!CA_V42(CA_PROTOCOL_VERSION, pNetIIU->minor_version_number)) {
            cac_reconnect_channel (pNetIIU, pchan->cid, USHRT_MAX, 0);
        }
    }
    else {
        pNetIIU->claimsPending = TRUE;
    }
    UNLOCK (pchan->ciu.piiu->pcas);

    return success;
}

/*
 *  CA_ADD_FD_REGISTRATION
 *
 *  call their function with their argument whenever 
 *  a new fd is added or removed
 *  (for a manager of the select system call under UNIX)
 *
 */
int epicsShareAPI ca_add_fd_registration(CAFDHANDLER *func, void *arg)
{
    cac       *pcac;
    int             caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    LOCK (pcac);
    pcac->ca_fd_register_func = func;
    pcac->ca_fd_register_arg = arg;
    UNLOCK (pcac);

    return ECA_NORMAL;
}

/*
 *  CA_DEFUNCT
 *
 *  what is called by vacated entries in the VMS share image jump table
 */
int ca_defunct()
{
    SEVCHK (ECA_DEFUNCT, NULL);
    return ECA_DEFUNCT;
}

/*
 *  CA_HOST_NAME() 
 *
 *  returns a pointer to the channel's host name
 *
 *  currently implemented as a function 
 *  (may be implemented as a MACRO in the future)
 */
const char * epicsShareAPI ca_host_name (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    /*
     * is it a local channel ?
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        return pChan->piiu->pcas->ca_pHostName;
    }
    else {
        nciu *pNetChan = (nciu *) ciuToNCIU (pChan);
        
        if (pNetChan->connected) {
            tcpiiu *piiu = iiuToTCPIIU (pChan->piiu);
            return piiu->host_name_str;
        }
        else {
            return "<disconnected>";
        }    
    }

}

/*
 * ca_v42_ok(chid chan)
 */
int epicsShareAPI ca_v42_ok (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    /*
     * is it a local channel ?
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        return TRUE;
    }
    else {
        nciu *pNetChan = (nciu *) ciuToNCIU (pChan);
        
        if (pNetChan->connected) {
            tcpiiu *piiu = iiuToTCPIIU (pChan->piiu);
            return CA_V42 (CA_PROTOCOL_VERSION,
                    piiu->minor_version_number);
        }
        else {
            return FALSE;
        }    
    }
}

/*
 * ca_version()
 * function that returns the CA version string
 */
const char * epicsShareAPI ca_version()
{
    return CA_VERSION_STRING; 
}

/*
 * ca_replace_printf_handler ()
 */
int epicsShareAPI ca_replace_printf_handler (
int (*ca_printf_func)(const char *pformat, va_list args)
)
{
    cac       *pcac;
    int             caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    LOCK (pcac);
    if (ca_printf_func) {
        pcac->ca_printf_func = ca_printf_func;
    }
    else {
        pcac->ca_printf_func = epicsVprintf;
    }
    UNLOCK (pcac);

    return ECA_NORMAL;
}

/*
 * ca_printf()
 */
int ca_printf (cac *pcac, const char *pformat, ...)
{
    va_list theArgs;
    int     status;
    
    va_start (theArgs, pformat);
    
    status = ca_vPrintf (pcac, pformat, theArgs);
    
    va_end (theArgs);
    
    return status;
}

/*
 *      ca_vPrintf()
 */
int ca_vPrintf (cac *pcac, const char *pformat, va_list args)
{
    int (*ca_printf_func)(const char *pformat, va_list args);
    
    if (pcac) {
        if (pcac->ca_printf_func) {
            ca_printf_func = pcac->ca_printf_func;
        }
        else {
            ca_printf_func = errlogVprintf;
        }
    }
    else {
        ca_printf_func = errlogVprintf;
    }
    
    return (*ca_printf_func) (pformat, args);
}


/*
 * ca_field_type()
 */
short epicsShareAPI ca_field_type (chid pChanIn) 
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    /*
     * is it a local channel ?
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);
        return (*pChan->piiu->pcas->localIIU.pva->p_pvType) (pLocalChan->id);
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        if (pNetChan->connected) {
            return (short) pNetChan->type;
        }
        else {
            return TYPENOTCONN;
        }
    }
}

/*
 * ca_element_count ()
 */
unsigned long epicsShareAPI ca_element_count (chid pChanIn) 
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    /*
     * is it a local channel ?
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);
        return (*pChan->piiu->pcas->localIIU.pva->p_pvNoElements) (pLocalChan->id);
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        if (pNetChan->connected) {
            return pNetChan->count;
        }
        else {
            return 0;
        }
    }
}

/*
 * ca_state ()
 */
epicsShareFunc enum channel_state epicsShareAPI ca_state (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    if ( pChan->piiu == &pChan->piiu->pcas->localIIU.iiu ) {
        return cs_conn;
    }
    else if ( pChan->piiu == NULL ) {
        return cs_closed;
    }
    else {
        nciu *pNChan = ciuToNCIU (pChan);
        if (pNChan->connected) {
            return cs_conn;
        }
        else if (pNChan->previousConn) {
            return cs_prev_conn;
        }
        else {
            return cs_never_conn;
        }
    }
}

/*
 * ca_set_puser ()
 */
epicsShareFunc void epicsShareAPI ca_set_puser (chid pChanIn, void *puser)
{
    baseCIU *pChan = (baseCIU *) pChanIn;
    pChan->puser = puser;
}

/*
 * ca_get_puser ()
 */
epicsShareFunc void * epicsShareAPI ca_puser (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;
    return pChan->puser;
}

/*
 * ca_read_access ()
 */
epicsShareFunc unsigned epicsShareAPI ca_read_access (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        return TRUE;
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        return pNetChan->ar.read_access;
    }
}

/*
 * ca_write_access ()
 */
epicsShareFunc unsigned epicsShareAPI ca_write_access (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        return TRUE;
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        return pNetChan->ar.write_access;
    }
}

/*
 * ca_name ()
 */
epicsShareFunc const char * epicsShareAPI ca_name (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    /*
     * is it a local channel ?
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        lciu *pLocalChan = ciuToLCIU (pChan);
        lclIIU *pLocalIIU = iiuToLIIU (pChan->piiu);
        return (*pLocalIIU->pva->p_pvName) (pLocalChan->id);
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        return (const char *) (pNetChan+1);
    }
}

epicsShareFunc unsigned epicsShareAPI ca_search_attempts (chid pChanIn)
{
    baseCIU *pChan = (baseCIU *) pChanIn;

    /*
     * is it a local channel ?
     */
    if (pChan->piiu == &pChan->piiu->pcas->localIIU.iiu) {
        return 0;
    }
    else {
        nciu *pNetChan = ciuToNCIU (pChan);
        return pNetChan->retry;
    }
}

/*
 * ca_get_ioc_connection_count()
 *
 * returns the number of IOC's that CA is connected to
 * (for testing purposes only)
 */
unsigned epicsShareAPI ca_get_ioc_connection_count () 
{
    unsigned    count;
    cac   *pcac;
    int         caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    LOCK (pcac);

    count = ellCount (&pcac->ca_iiuList);

    UNLOCK (pcac);

    return count;
}

LOCAL void niiuShow (netIIU *piiu, unsigned level)
{
	nciu                *pChan;

	pChan = (nciu *) ellFirst (&piiu->chidList);
	while ( pChan ) {
		printf(	"%s native type=%d ", 
			ca_name (&pChan->ciu), ca_field_type (&pChan->ciu));
		printf(	"N elements=%lu server=%s state=", 
			ca_element_count (&pChan->ciu), ca_host_name(&pChan->ciu));
		switch ( ca_state (&pChan->ciu) ) {
		case cs_never_conn:
			printf ("never connected to an IOC");
			break;
		case cs_prev_conn:
			printf ("disconnected from IOC");
			break;
		case cs_conn:
			printf ("connected to an IOC");
			break;
		case cs_closed:
			printf ("invalid channel");
			break;
		default:
			break;
		}
		printf("\n");
        pChan = (nciu *) ellNext (&pChan->node);
	}
}

epicsShareFunc int epicsShareAPI ca_channel_status (threadId tid)
{
	tcpiiu			    *piiu;
    cac           *pcac;
    int                 caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

	LOCK (pcac);
    if (pcac->pudpiiu) {
        niiuShow (&pcac->pudpiiu->niiu, 10);
    }
	piiu = (tcpiiu *) ellFirst (&pcac->ca_iiuList);
	while (piiu) {
        niiuShow (&piiu->niiu, 10);
		piiu = (tcpiiu *) ellNext (&piiu->node);
	}
    UNLOCK (pcac);
	return ECA_NORMAL;
}

/*
 * ca_current_context ()
 *
 * used when an auxillary thread needs to join a CA client context started
 * by another thread
 */
epicsShareFunc int epicsShareAPI ca_current_context (caClientCtx *pCurrentContext)
{
    if (caClientContextId) {
        void *pCtx = threadPrivateGet (caClientContextId);
        if (pCtx) {
            *pCurrentContext = pCtx;
            return ECA_NORMAL;
        }
        else {
            return ECA_NOCACTX;
        }
    }
    else {
        return ECA_NOCACTX;
    }
}

/*
 * ca_attach_context ()
 *
 * used when an auxillary thread needs to join a CA client context started
 * by another thread
 */
epicsShareFunc int epicsShareAPI ca_attach_context (caClientCtx context)
{
    threadPrivateSet (caClientContextId, context);
    return ECA_NORMAL;
}


