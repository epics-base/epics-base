/*
 *  $Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *      Date:  9-93
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
 */

#include "freeList.h"

#include "iocinf.h"

/*
 * ca_sg_init()
 */
void ca_sg_init (cac *pcac)
{
    /*
     * init all sync group lists
     */
    ellInit (&pcac->activeCASG);
    ellInit (&pcac->activeCASGOP);
    freeListInitPvt (&pcac->ca_sgFreeListPVT, sizeof(CASG), 32);
    freeListInitPvt (&pcac->ca_sgopFreeListPVT, sizeof(CASGOP), 256);

    return;
}

/*
 * ca_sg_shutdown()
 */
void ca_sg_shutdown (cac *pcac)
{
    CASG    *pcasg;
    CASG    *pnextcasg;
    int     status;

    /*
     * free all sync group lists
     */
    LOCK (pcac);
    pcasg = (CASG *) ellFirst (&pcac->activeCASG);
    while (pcasg) {
        pnextcasg = (CASG *) ellNext (&pcasg->node);
        status = ca_sg_delete (pcasg->id);
        assert (status==ECA_NORMAL);
        pcasg = pnextcasg;
    }
    assert (ellCount(&pcac->activeCASG)==0);

    /*
     * per sync group
     */
    freeListCleanup(pcac->ca_sgFreeListPVT);

    /*
     * per sync group op
     */
    ellFree (&pcac->activeCASGOP);
    freeListCleanup(pcac->ca_sgopFreeListPVT);

    UNLOCK (pcac);

    return;
}

/*
 * ca_sg_create()
 */
int epicsShareAPI ca_sg_create (CA_SYNC_GID *pgid)
{
    int caStatus;
    int status;
    CASG *pcasg;
    cac *pcac;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    /*
     * first look on a free list. If not there
     * allocate dynamic memory for it.
     */
    pcasg = (CASG *) freeListMalloc (pcac->ca_sgFreeListPVT);
    if(!pcasg){
        return ECA_ALLOCMEM;
    }

    LOCK (pcac);

    /*
     * setup initial values for all of the fields
     *
     * lock must be applied when allocating an id
     * and using the id bucket
     */
    memset((char *)pcasg,0,sizeof(*pcasg));
    pcasg->magic = CASG_MAGIC;
    pcasg->opPendCount = 0;
    pcasg->seqNo = 0;

    pcasg->sem = semBinaryMustCreate (semEmpty);

    do {
        pcasg->id = CLIENT_SLOW_ID_ALLOC (pcac); 
        status = bucketAddItemUnsignedId (pcac->ca_pSlowBucket, &pcasg->id, pcasg);
    } while (status == S_bucket_idInUse);

    if (status == S_bucket_success) {
        /*
         * place it on the active sync group list
         */
        ellAdd (&pcac->activeCASG, &pcasg->node);
    }
    else {
        /*
         * place it back on the free sync group list
         */
        freeListFree(pcac->ca_sgFreeListPVT, pcasg);
        UNLOCK (pcac);
        if (status == S_bucket_noMemory) {
            return ECA_ALLOCMEM;
        }
        else {
            return ECA_INTERNAL;
        }
    }

    UNLOCK (pcac);

    *pgid = pcasg->id;
    return ECA_NORMAL;
}

/*
 * ca_sg_delete()
 */
int epicsShareAPI ca_sg_delete(const CA_SYNC_GID gid)
{
    int         caStatus;
    int         status;
    CASG        *pcasg;
    cac   *pcac;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    LOCK (pcac);

    pcasg = (CASG *) bucketLookupItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    if (!pcasg || pcasg->magic != CASG_MAGIC) {
        UNLOCK (pcac);
        return ECA_BADSYNCGRP;
    }

    status = bucketRemoveItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    assert (status == S_bucket_success);

    semBinaryDestroy(pcasg->sem);
    pcasg->magic = 0;
    ellDelete(&pcac->activeCASG, &pcasg->node);
    UNLOCK (pcac);

    freeListFree(pcac->ca_sgFreeListPVT, pcasg);

    return ECA_NORMAL;
}

/*
 * ca_sg_block_private ()
 */
static int ca_sg_block_private (cac *pcac, const CA_SYNC_GID gid, ca_real timeout)
{
    TS_STAMP        cur_time;
    TS_STAMP        beg_time;
    ca_real         delay;
    int             status;
    CASG            *pcasg;
    unsigned        flushCompleted = FALSE;

    if (timeout<0.0) {
        return ECA_TIMEOUT;
    }

    status = tsStampGetCurrent (&cur_time);
    if (status!=0) {
        return ECA_INTERNAL;
    }

    LOCK (pcac);

    pcac->currentTime = cur_time;

    pcasg = (CASG *) bucketLookupItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    if ( !pcasg || pcasg->magic != CASG_MAGIC ) {
        UNLOCK (pcac);
        return ECA_BADSYNCGRP;
    }

    UNLOCK (pcac);

    cacFlushAllIIU (pcac);

    beg_time = cur_time;
    delay = 0.0;

    status = ECA_NORMAL;
    while (pcasg->opPendCount) {
        ca_real  remaining;
        int tsStatus;

        /*
         * Exit if the timeout has expired
         * (dont wait forever for an itsy bitsy
         * delay which will not be updated if
         * select is called with no delay)
         *
         * current time is only updated by
         * cac_select_io() if we specify
         * at non-zero delay
         */
        remaining = timeout-delay;
        if (remaining<=CAC_SIGNIFICANT_SELECT_DELAY) {
            /*
             * Make sure that we take care of
             * recv backlog at least once
             */
            status = ECA_TIMEOUT;
            break;
        }

        remaining = min (60.0, remaining);

        /*
         * wait for asynch notification
         */
        semBinaryTakeTimeout (pcasg->sem, remaining);

        /*
         * force a time update 
         */
        tsStatus = tsStampGetCurrent (&cur_time);
        if (tsStatus!=0) {
            status = ECA_INTERNAL;
            break;
        }

        LOCK (pcac);
        pcac->currentTime = cur_time;
        UNLOCK (pcac);

        flushCompleted = TRUE;
        delay = tsStampDiffInSeconds (&cur_time, &beg_time);
    }
    pcasg->opPendCount = 0;
    pcasg->seqNo++;
    return status;
}

/*
 * ca_sg_block ()
 */
int epicsShareAPI ca_sg_block (const CA_SYNC_GID gid, ca_real timeout)
{
    cac *pcac;
    int status;

    status = fetchClientContext (&pcac);
    if ( status != ECA_NORMAL ) {
        return status;
    }

    /*
     * dont allow recursion
     */
    {
        void *p = threadPrivateGet (cacRecursionLock);
        if (p) {
            return ECA_EVDISALLOW;
        }
        threadPrivateSet (cacRecursionLock, &cacRecursionLock);
    }

    status = ca_sg_block_private (pcac, gid, timeout);

    threadPrivateSet (cacRecursionLock, NULL);

    return status;
}

/*
 * ca_sg_reset
 */
int epicsShareAPI ca_sg_reset (const CA_SYNC_GID gid)
{
    CASG        *pcasg;
    cac   *pcac;
    int         caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    LOCK (pcac);
    pcasg = (CASG *) bucketLookupItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    if(!pcasg || pcasg->magic != CASG_MAGIC){
        UNLOCK (pcac);
        return ECA_BADSYNCGRP;
    }

    pcasg->opPendCount = 0;
    pcasg->seqNo++;
    UNLOCK (pcac);

    return ECA_NORMAL;
}

/*
 * ca_sg_stat
 */
int epicsShareAPI ca_sg_stat (const CA_SYNC_GID gid)
{
    CASG        *pcasg;
    CASGOP      *pcasgop;
    cac   *pcac;
    int         caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    LOCK (pcac);
    pcasg = (CASG *) bucketLookupItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    if (!pcasg || pcasg->magic != CASG_MAGIC) {
        UNLOCK (pcac);
        printf("Bad Sync Group Id\n");
        return ECA_BADSYNCGRP;
    }
    UNLOCK (pcac);

    printf("Sync Group: id=%u, magic=%lu, opPend=%lu, seqNo=%lu\n",
        pcasg->id, pcasg->magic, pcasg->opPendCount,
        pcasg->seqNo);

    LOCK (pcac);
    pcasgop = (CASGOP *) ellFirst (&pcac->activeCASGOP);
    while (pcasgop) {
        if (pcasg->id == pcasgop->id) {
            printf(
            "pending op: id=%u pVal=%x, magic=%lu seqNo=%lu\n",
            pcasgop->id, (unsigned)pcasgop->pValue, pcasgop->magic,
            pcasgop->seqNo);
        }
        pcasgop = (CASGOP *) ellNext(&pcasgop->node);
    }
    UNLOCK (pcac);

    return ECA_NORMAL;
}

/*
 * ca_sg_test
 */
int epicsShareAPI ca_sg_test (const CA_SYNC_GID gid)
{
    CASG        *pcasg;
    cac   *pcac;
    int         caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    LOCK (pcac);
    pcasg = (CASG *) bucketLookupItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    if(!pcasg || pcasg->magic != CASG_MAGIC){
        UNLOCK (pcac);
        return ECA_BADSYNCGRP;
    }
    UNLOCK (pcac);

    if(pcasg->opPendCount){
        return ECA_IOINPROGRESS;
    }
    else{
        return ECA_IODONE;
    }
}

/*
 * ca_sync_grp_io_complete ()
 */
extern "C" void ca_sync_grp_io_complete (struct event_handler_args args)
{
    unsigned long   size;
    CASGOP          *pcasgop;
    CASG            *pcasg;

    pcasgop = (CASGOP *) args.usr;

    if (pcasgop->magic != CASG_MAGIC) {
        errlogPrintf ("cac: sync group io_complete(): bad sync grp op magic number?\n");
        return;
    }

    LOCK (pcasgop->pcac);
    ellDelete (&pcasgop->pcac->activeCASGOP, &pcasgop->node);
    pcasgop->magic = 0;

    /*
     * ignore stale replies
     */
    pcasg = (CASG *) bucketLookupItemUnsignedId (pcasgop->pcac->ca_pSlowBucket, &pcasgop->id);
    if (!pcasg || pcasg->seqNo != pcasgop->seqNo) {
        UNLOCK (pcasgop->pcac);
        return;
    }

    assert (pcasg->magic == CASG_MAGIC);
    assert (pcasg->id == pcasgop->id);

    if ( !( args.status & CA_M_SUCCESS ) ) {
        ca_printf (
            pcasgop->pcac,
            "CA Sync Group (id=%d) request failed because \"%s\"\n",
            pcasgop->id,
            ca_message(args.status) );
        UNLOCK (pcasgop->pcac);
        freeListFree(pcasgop->pcac->ca_sgopFreeListPVT, pcasgop);
        return;
    }

    /*
     * Update the user's variable
     * (if its a get)
     */
    if (pcasgop->pValue && args.dbr) {
        size = dbr_size_n (args.type, args.count);
        memcpy (pcasgop->pValue, args.dbr, size);
    }

    /*
     * decrement the outstanding IO ops count
     */
    assert (pcasg->opPendCount>=1u);
    pcasg->opPendCount--;

    UNLOCK (pcasgop->pcac);

    freeListFree (pcasgop->pcac->ca_sgopFreeListPVT, pcasgop);

    /*
     * Wake up any tasks pending
     *
     * occurs through select on UNIX
     */
    if (pcasg->opPendCount == 0) {
        semBinaryGive(pcasg->sem);
    }
    return;
}

/*
 * ca_sg_array_put()
 */
int epicsShareAPI ca_sg_array_put (
const CA_SYNC_GID   gid, 
chtype              type,
unsigned long       count, 
chid                chix, 
const void          *pvalue)
{
    int         status;
    CASGOP      *pcasgop;
    CASG        *pcasg;
    cac   *pcac;
    int         caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    /*
     * first look on a free list. If not there
     * allocate dynamic memory for it.
     */
    pcasgop = (CASGOP *) freeListMalloc (pcac->ca_sgopFreeListPVT);
    if(!pcasgop){
        return ECA_ALLOCMEM;
    }

    LOCK (pcac);
    pcasg = (CASG *) bucketLookupItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    if(!pcasg || pcasg->magic != CASG_MAGIC){
        UNLOCK (pcac);
        freeListFree(pcac->ca_sgopFreeListPVT, pcasgop);
        return ECA_BADSYNCGRP;
    }

    memset((char *)pcasgop, 0,sizeof(*pcasgop));
    pcasgop->id = gid;
    pcasgop->seqNo = pcasg->seqNo;
    pcasgop->magic = CASG_MAGIC;
    pcasgop->pValue = NULL; /* handler will know its a put */
    pcasgop->pcac = pcac;
    ellAdd (&pcac->activeCASGOP, &pcasgop->node);
    pcasg->opPendCount++;
    UNLOCK (pcac);

    status = ca_array_put_callback (type, count, chix, 
                    pvalue, ca_sync_grp_io_complete, pcasgop);
    if (status != ECA_NORMAL) {
        LOCK (pcac);
        assert (pcasg->opPendCount>=1u);
        pcasg->opPendCount--;
        ellDelete (&pcac->activeCASGOP, &pcasgop->node);
        UNLOCK (pcac);
        freeListFree (pcac->ca_sgopFreeListPVT, pcasgop);
    }

    return  status;
}

/*
 * ca_sg_array_get()
 */
int epicsShareAPI ca_sg_array_get (
const CA_SYNC_GID   gid, 
chtype              type,
unsigned long       count, 
chid                chix, 
void                *pvalue)
{
    int         status;
    CASGOP      *pcasgop;
    CASG        *pcasg;
    cac   *pcac;
    int         caStatus;

    caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    /*
     * first look on a free list. If not there
     * allocate dynamic memory for it.
     */
    pcasgop = (CASGOP *) freeListMalloc (pcac->ca_sgopFreeListPVT);
    if (!pcasgop) {
        return ECA_ALLOCMEM;
    }

    LOCK (pcac);
    pcasg = (CASG *) bucketLookupItemUnsignedId (pcac->ca_pSlowBucket, &gid);
    if(!pcasg || pcasg->magic != CASG_MAGIC){
        UNLOCK (pcac);
        freeListFree(pcac->ca_sgopFreeListPVT, pcasgop);
        return ECA_BADSYNCGRP;
    }

    memset((char *)pcasgop, 0,sizeof(*pcasgop));
    pcasgop->id = gid;
    pcasgop->seqNo = pcasg->seqNo;
    pcasgop->magic = CASG_MAGIC;
    pcasgop->pValue = pvalue;
    pcasgop->pcac = pcac;
    ellAdd(&pcac->activeCASGOP, &pcasgop->node);
    pcasg->opPendCount++;

    UNLOCK (pcac);

    status = ca_array_get_callback (
            type, count, chix, ca_sync_grp_io_complete, pcasgop);

    if(status != ECA_NORMAL){
        LOCK (pcac);
        assert (pcasg->opPendCount>=1u);
        pcasg->opPendCount--;
        ellDelete (&pcac->activeCASGOP, &pcasgop->node);
        UNLOCK (pcac);
        freeListFree (pcac->ca_sgopFreeListPVT, pcasgop);
    }
    return status;
}
