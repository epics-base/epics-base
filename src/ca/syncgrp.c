/*
 *	$Id$
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
 *      Modification Log:
 *      -----------------
 * $Log$
 * Revision 1.23  1997/04/29 06:12:42  jhill
 * use free lists
 *
 * Revision 1.22  1996/11/22 19:08:02  jhill
 * added const to API
 *
 * Revision 1.21  1996/11/02 00:51:08  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.20  1996/07/10 23:30:12  jhill
 * fixed GNU warnings
 *
 * Revision 1.19  1996/06/19 17:59:29  jhill
 * many 3.13 beta changes
 *
 * Revision 1.18  1995/10/12  01:36:39  jhill
 * New ca_flush_io() mechanism
 *
 * Revision 1.17  1995/09/29  22:13:59  jhill
 * check for nill dbr pointer
 *
 * Revision 1.16  1995/08/22  00:27:55  jhill
 * added cvs style mod log
 *
 *
 *	NOTES:
 *	1) Need to fix if the OP is on a FD that
 *	becomes disconneted it will stay on the 
 *	queue forever.
 */

#include "iocinf.h"
#include "freeList.h"

LOCAL void io_complete(struct event_handler_args args);


/*
 * ca_sg_init()
 */
void ca_sg_init(void)
{
	/*
	 * init all sync group lists
	 */
	freeListInitPvt(&ca_static->ca_sgFreeListPVT,sizeof(CASG),32);
	freeListInitPvt(&ca_static->ca_sgopFreeListPVT,sizeof(CASGOP),256);

	return;
}


/*
 * ca_sg_shutdown()
 */
void ca_sg_shutdown(struct CA_STATIC *ca_temp)
{
	CASG 	*pcasg;
	CASG 	*pnextcasg;
	int	status;

	/*
	 * free all sync group lists
	 */
	LOCK;
	pcasg = (CASG *) ellFirst (&ca_temp->activeCASG);
	while (pcasg) {
		pnextcasg = (CASG *) ellNext (&pcasg->node);
		status = ca_sg_delete (pcasg->id);
		assert (status==ECA_NORMAL);
		pcasg = pnextcasg;
	}
	assert (ellCount(&ca_temp->activeCASG)==0);

	/*
	 * per sync group
	 */
	freeListCleanup(ca_temp->ca_sgFreeListPVT);

	/*
	 * per sync group op
	 */
	ellFree (&ca_temp->activeCASGOP);
	freeListCleanup(ca_temp->ca_sgopFreeListPVT);

	UNLOCK;

	return;
}


/*
 * ca_sg_create()
 */
int epicsShareAPI ca_sg_create(CA_SYNC_GID *pgid)
{
	int	status;
	CASG 	*pcasg;

	/*
	 * Force the CA client id bucket to
	 * init if needed.
	 * Return error if unable to init.
	 */
	INITCHK;

	/*
 	 * first look on a free list. If not there
	 * allocate dynamic memory for it.
	 */
	pcasg = (CASG *) freeListMalloc(ca_static->ca_sgFreeListPVT);
	if(!pcasg){
		return ECA_ALLOCMEM;
	}

	LOCK;

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

	os_specific_sg_create(pcasg);

	do {
		pcasg->id = CLIENT_SLOW_ID_ALLOC; 
		status = bucketAddItemUnsignedId (pSlowBucket, 
						&pcasg->id, pcasg);
	} while (status == S_bucket_idInUse);

	if (status == S_bucket_success) {
		/*
		 * place it on the active sync group list
		 */
		ellAdd (&ca_static->activeCASG, &pcasg->node);
	}
	else {
		/*
	  	 * place it back on the free sync group list
	 	 */
		freeListFree(ca_static->ca_sgFreeListPVT, pcasg);
		UNLOCK;
		if (status == S_bucket_noMemory) {
			return ECA_ALLOCMEM;
		}
		else {
			return ECA_INTERNAL;
		}
	}

	UNLOCK;

	*pgid = pcasg->id;
	return ECA_NORMAL;
}


/*
 * ca_sg_delete()
 */
int epicsShareAPI ca_sg_delete(const CA_SYNC_GID gid)
{
	int	status;
	CASG 	*pcasg;

	/*
	 * Force the CA client id bucket to
	 * init if needed.
	 * Return error if unable to init.
	 */
	INITCHK;

	LOCK;

	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}

	status = bucketRemoveItemUnsignedId(pSlowBucket, &gid);
	assert (status == S_bucket_success);

	os_specific_sg_delete(pcasg);

	pcasg->magic = 0;
	ellDelete(&ca_static->activeCASG, &pcasg->node);
	UNLOCK;

	freeListFree(ca_static->ca_sgFreeListPVT, pcasg);

	return ECA_NORMAL;
}


/*
 * ca_sg_block()
 */
int epicsShareAPI ca_sg_block(const CA_SYNC_GID gid, ca_real timeout)
{
	struct timeval	beg_time;
	ca_real		delay;
	int		status;
	CASG 		*pcasg;

	/*
	 * Force the CA client id bucket to
	 * init if needed.
	 * Return error if unable to init.
	 */
	INITCHK;

	if(timeout<0.0){
		return ECA_TIMEOUT;
	}

	/*
	 * until CAs input mechanism is 
	 * revised we have to dissallow
	 * this routine from an event routine
	 * (to avoid excess recursion)
	 */
	if(EVENTLOCKTEST){
		return ECA_EVDISALLOW;
	}

	LOCK;
	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}
	UNLOCK;

	/*
	 * always flush at least once.
	 */
	ca_static->ca_flush_pending = TRUE;

	cac_gettimeval (&ca_static->currentTime);
        beg_time = ca_static->currentTime;
        delay = 0.0;

	status = ECA_NORMAL;
	while(pcasg->opPendCount){
		ca_real         remaining;
		struct timeval	tmo;

		/*
		 * Exit if the timeout has expired
		 * (dont wait forever for an itsy bitsy
		 * delay which will no be updated if
		 * select is called with no delay)
		 *
		 * current time is only updated by
		 * cac_select_io() if we specify
		 * at least 1 usec to wait
		 */
		remaining = timeout-delay;
		if (remaining<=(1.0/USEC_PER_SEC)) {
			/*
			 * Make sure that we take care of
			 * recv backlog at least once
			 */
			tmo.tv_sec = 0L;
			tmo.tv_usec = 0L;
			cac_mux_io (&tmo);
			status = ECA_TIMEOUT;
			break;
		}

		/*
		 * Allow for CA background labor
		 */
		remaining = min(SELECT_POLL, remaining);

		/*
		 * wait for asynch notification
		 */
		tmo.tv_sec = (long) remaining;
		tmo.tv_usec = (long) ((remaining-tmo.tv_sec)*USEC_PER_SEC);
		cac_block_for_sg_completion (pcasg, &tmo);

		delay = cac_time_diff (&ca_static->currentTime, &beg_time);
	}
	pcasg->opPendCount = 0;
	pcasg->seqNo++;
	return status;
}


/*
 * ca_sg_reset
 */
int epicsShareAPI ca_sg_reset(const CA_SYNC_GID gid)
{
	CASG 	*pcasg;

	LOCK;
	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}

	pcasg->opPendCount = 0;
	pcasg->seqNo++;
	UNLOCK;

	return ECA_NORMAL;
}


/*
 * ca_sg_stat
 */
int epicsShareAPI ca_sg_stat(const CA_SYNC_GID gid)
{
	CASG 	*pcasg;
	CASGOP	*pcasgop;

	LOCK;
	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		printf("Bad Sync Group Id\n");
		return ECA_BADSYNCGRP;
	}
	UNLOCK;

	printf("Sync Group: id=%u, magic=%lu, opPend=%lu, seqNo=%lu\n",
		pcasg->id, pcasg->magic, pcasg->opPendCount,
		pcasg->seqNo);

	LOCK;
	pcasgop = (CASGOP *) ellFirst(&ca_static->activeCASGOP);
	while (pcasgop) {
		if (pcasg->id == pcasgop->id) {
			printf(
			"pending op: id=%u pVal=%x, magic=%lu seqNo=%lu\n",
			pcasgop->id, (unsigned)pcasgop->pValue, pcasgop->magic,
			pcasgop->seqNo);
		}
		pcasgop = (CASGOP *) ellNext(&pcasgop->node);
	}
	return ECA_NORMAL;
}


/*
 * ca_sg_test
 */
int epicsShareAPI ca_sg_test(const CA_SYNC_GID gid)
{
	CASG 	*pcasg;

	LOCK;
	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}
	UNLOCK;

	if(pcasg->opPendCount){
		return ECA_IOINPROGRESS;
	}
	else{
		return ECA_IODONE;
	}
}


/*
 * ca_sg_array_put()
 */
int epicsShareAPI ca_sg_array_put(
const CA_SYNC_GID 	gid, 
chtype			type,
unsigned long 		count, 
chid 			chix, 
const void 		*pvalue)
{
	int	status;
	CASGOP	*pcasgop;
	CASG 	*pcasg;

	/*
 	 * first look on a free list. If not there
	 * allocate dynamic memory for it.
	 */
	pcasgop = (CASGOP *) freeListMalloc(ca_static->ca_sgopFreeListPVT);
	if(!pcasgop){
		return ECA_ALLOCMEM;
	}

	LOCK;
	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		freeListFree(ca_static->ca_sgopFreeListPVT, pcasgop);
		return ECA_BADSYNCGRP;
	}

	memset((char *)pcasgop, 0,sizeof(*pcasgop));
	pcasgop->id = gid;
	pcasgop->seqNo = pcasg->seqNo;
	pcasgop->magic = CASG_MAGIC;
	pcasgop->pValue = NULL; /* handler will know its a put */
	ellAdd(&ca_static->activeCASGOP, &pcasgop->node);
	pcasg->opPendCount++;
	UNLOCK;

	status = ca_array_put_callback(
			type, 
			count, 
			chix, 
			pvalue, 
			io_complete, 
			pcasgop);

	if(status != ECA_NORMAL){
                LOCK;
		assert(pcasg->opPendCount>=1u);
                pcasg->opPendCount--;
                ellDelete(&ca_static->activeCASGOP, &pcasgop->node);
                UNLOCK;
		freeListFree(ca_static->ca_sgopFreeListPVT, pcasgop);
         }


	return  status;
}



/*
 * ca_sg_array_get()
 */
int epicsShareAPI ca_sg_array_get(
const CA_SYNC_GID 	gid, 
chtype			type,
unsigned long 		count, 
chid 			chix, 
void 			*pvalue)
{
	int	status;
	CASGOP	*pcasgop;
	CASG 	*pcasg;

	/*
 	 * first look on a free list. If not there
	 * allocate dynamic memory for it.
	 */
	pcasgop = (CASGOP *) freeListMalloc(ca_static->ca_sgopFreeListPVT);
	if(!pcasgop){
		return ECA_ALLOCMEM;
	}

	LOCK;
	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		freeListFree(ca_static->ca_sgopFreeListPVT, pcasgop);
		return ECA_BADSYNCGRP;
	}

	memset((char *)pcasgop, 0,sizeof(*pcasgop));
	pcasgop->id = gid;
	pcasgop->seqNo = pcasg->seqNo;
	pcasgop->magic = CASG_MAGIC;
	pcasgop->pValue = pvalue;
	ellAdd(&ca_static->activeCASGOP, &pcasgop->node);
	pcasg->opPendCount++;

	UNLOCK;

	status = ca_array_get_callback(
			type, 
			count, 
			chix, 
			io_complete, 
			pcasgop);

	if(status != ECA_NORMAL){
		LOCK;
		assert(pcasg->opPendCount>=1u);
		pcasg->opPendCount--;
		ellDelete(&ca_static->activeCASGOP, &pcasgop->node);
		UNLOCK;
		freeListFree(ca_static->ca_sgopFreeListPVT, pcasgop);
	}
	return status;
}


/*
 * io_complete()
 */
LOCAL void io_complete(struct event_handler_args args)
{
	unsigned long	size;
	CASGOP		*pcasgop;
	CASG 		*pcasg;

	pcasgop = args.usr;
	assert(pcasgop->magic == CASG_MAGIC);

	LOCK;
	ellDelete(&ca_static->activeCASGOP, &pcasgop->node);
	pcasgop->magic = 0;

	/*
 	 * ignore stale replies
	 */
	pcasg = bucketLookupItemUnsignedId(pSlowBucket, &pcasgop->id);
	if(!pcasg || pcasg->seqNo != pcasgop->seqNo){
		UNLOCK;
		return;
	}

	assert(pcasg->magic == CASG_MAGIC);
	assert(pcasg->id == pcasgop->id);

	if(!(args.status&CA_M_SUCCESS)){
		ca_printf(
			"CA Sync Group (id=%d) request failed because \"%s\"\n",
			pcasgop->id,
			ca_message(args.status));
		UNLOCK;
		freeListFree(ca_static->ca_sgopFreeListPVT, pcasgop);
		return;
	}

	/*
 	 * Update the user's variable
	 * (if its a get)
	 */
	if(pcasgop->pValue && args.dbr){
		size = dbr_size_n(args.type, args.count);
		memcpy(pcasgop->pValue, args.dbr, size);
	}

	/*
 	 * decrement the outstanding IO ops count
	 */
	assert(pcasg->opPendCount>=1u);
	pcasg->opPendCount--;

	UNLOCK;

	freeListFree(ca_static->ca_sgopFreeListPVT, pcasgop);

	/*
 	 * Wake up any tasks pending
	 *
	 * occurs through select on UNIX
	 */
	if(pcasg->opPendCount == 0){
		os_specific_sg_io_complete(pcasg);
	}

	return;
}
