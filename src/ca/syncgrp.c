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
 *
 *	NOTES:
 *	1) Need to fix if the OP is on a FD that
 *	becomes disconneted it will stay on the 
 *	queue forever.
 */

#include "iocinf.h"

LOCAL void io_complete(struct event_handler_args args);


/*
 * ca_sg_init()
 */
void ca_sg_init(void)
{
	/*
	 * init all sync group lists
	 */
	ellInit(&ca_static->activeCASG);
	ellInit(&ca_static->freeCASG);

	return;
}


/*
 * ca_sg_shutdown()
 */
void ca_sg_shutdown(struct ca_static *ca_temp)
{
	/*
	 * free all sync group lists
	 */
	ellFree(&ca_temp->activeCASG);
	ellFree(&ca_temp->freeCASG);
	ellInit(&ca_temp->activeCASGOP);
	ellInit(&ca_temp->freeCASGOP);

	return;
}


/*
 * ca_sg_create()
 */
int ca_sg_create(CA_SYNC_GID *pgid)
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
	LOCK;
	pcasg = (CASG *) ellGet(&ca_static->freeCASG);
	if(!pcasg){
		pcasg = (CASG *) malloc(sizeof(*pcasg));	
		if(!pcasg){
			return ECA_ALLOCMEM;
		}
	}

	/*
 	 * setup initial values for all of the fields
	 *
	 * lock must be applied when allocating an id
	 * and using the id bucket
	 */
	memset((char *)pcasg,0,sizeof(*pcasg));
	pcasg->magic = CASG_MAGIC;
	pcasg->id = CLIENT_ID_ALLOC; 
	pcasg->opPendCount = 0;
	pcasg->seqNo = 0;

	os_specific_sg_create(pcasg);

	status = bucketAddItem(pBucket, pcasg->id, pcasg);
	if(status == BUCKET_SUCCESS){
		/*
	  	 * place it on the active sync group list
	 	 */
		ellAdd(&ca_static->activeCASG, &pcasg->node);
	}
	else{
		/*
	  	 * place it back on the free sync group list
	 	 */
		ellAdd(&ca_static->freeCASG, &pcasg->node);
	}
	UNLOCK;
	if(status != BUCKET_SUCCESS){
		return ECA_ALLOCMEM;
	}

	*(WRITEABLE_CA_SYNC_GID *)pgid = pcasg->id;
	return ECA_NORMAL;
}


/*
 * ca_sg_delete()
 */
int ca_sg_delete(CA_SYNC_GID gid)
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

	pcasg = bucketLookupItem(pBucket, gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}

	status = bucketRemoveItem(pBucket, gid, pcasg);
	assert(status == BUCKET_SUCCESS);

	os_specific_sg_delete(pcasg);

	pcasg->magic = 0;
	ellDelete(&ca_static->activeCASG, &pcasg->node);
	ellAdd(&ca_static->freeCASG, &pcasg->node);

	UNLOCK;

	return ECA_NORMAL;
}


/*
 * ca_sg_block()
 */
int ca_sg_block(CA_SYNC_GID gid, ca_real timeout)
{
	time_t	beg_time;
	int	status;
	CASG 	*pcasg;

	/*
	 * Force the CA client id bucket to
	 * init if needed.
	 * Return error if unable to init.
	 */
	INITCHK;

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
	pcasg = bucketLookupItem(pBucket, gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}
	UNLOCK;

	/*
	 * always flush and take care
	 * of connection management
	 * at least once.
	 */
	ca_flush_io();

	status = ECA_NORMAL;
	beg_time = time(NULL);
	while(pcasg->opPendCount){
		/*
		 * wait for asynch notification
		 */
		cac_block_for_sg_completion(pcasg);

		/*
		 * Exit if the timeout has expired
		 */
		if(timeout < time(NULL)-beg_time){
			status = ECA_TIMEOUT;
			break;
		}
	}
	pcasg->opPendCount = 0;
	pcasg->seqNo++;
	return status;
}


/*
 * ca_sg_reset
 */
int ca_sg_reset(CA_SYNC_GID gid)
{
	CASG 	*pcasg;

	LOCK;
	pcasg = bucketLookupItem(pBucket, gid);
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
 * ca_sg_test
 */
int ca_sg_test(CA_SYNC_GID gid)
{
	CASG 	*pcasg;

	LOCK;
	pcasg = bucketLookupItem(pBucket, gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}
	UNLOCK;

	if(pcasg->opPendCount){
		return ECA_IODONE;
	}
	else{
		return ECA_IOINPROGRESS;
	}
}




/*
 * ca_sg_array_put()
 */
int     ca_sg_array_put(
CA_SYNC_GID 	gid, 
chtype		type,
unsigned long 	count, 
chid 		chix, 
void 		*pvalue)
{
	int	status;
	CASGOP	*pcasgop;
	CASG 	*pcasg;

	LOCK;
	pcasg = bucketLookupItem(pBucket, gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}

	/*
 	 * first look on a free list. If not there
	 * allocate dynamic memory for it.
	 */
	pcasgop = (CASGOP *)ellGet(&ca_static->freeCASGOP);
	if(!pcasgop){
		pcasgop = (CASGOP *)malloc(sizeof(*pcasgop));	
		if(!pcasgop){
			UNLOCK;
			return ECA_ALLOCMEM;
		}
	}

	memset((char *)pcasgop, 0,sizeof(*pcasgop));
	pcasgop->id = gid;
	pcasgop->seqNo = pcasg->seqNo;
	pcasgop->magic = CASG_MAGIC;
	pcasgop->pValue = NULL; /* handler will know its a put */
	ellAdd(&ca_static->activeCASGOP, &pcasgop->node);
	UNLOCK;

	status = ca_array_put_callback(
			type, 
			count, 
			chix, 
			pvalue, 
			io_complete, 
			pcasgop);
	return status;
}



/*
 * ca_sg_array_get()
 */
int     ca_sg_array_get(
CA_SYNC_GID 	gid, 
chtype		type,
unsigned long 	count, 
chid 		chix, 
void 		*pvalue)
{
	int	status;
	CASGOP	*pcasgop;
	CASG 	*pcasg;

	LOCK;
	pcasg = bucketLookupItem(pBucket, gid);
	if(!pcasg || pcasg->magic != CASG_MAGIC){
		UNLOCK;
		return ECA_BADSYNCGRP;
	}

	/*
 	 * first look on a free list. If not there
	 * allocate dynamic memory for it.
	 */
	pcasgop = (CASGOP *)ellGet(&ca_static->freeCASGOP);
	if(!pcasgop){
		pcasgop = (CASGOP *) malloc(sizeof(*pcasgop));	
		if(!pcasgop){
			UNLOCK;
			return ECA_ALLOCMEM;
		}
	}

	memset((char *)pcasgop, 0,sizeof(*pcasgop));
	pcasgop->id = gid;
	pcasgop->seqNo = pcasg->seqNo;
	pcasgop->magic = CASG_MAGIC;
	pcasgop->pValue = pvalue;
	ellAdd(&ca_static->activeCASGOP, &pcasgop->node);

	UNLOCK;

	status = ca_array_get_callback(
			type, 
			count, 
			chix, 
			io_complete, 
			pcasgop);
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

	LOCK;
	pcasg = bucketLookupItem(pBucket, pcasgop->id);

	/*
 	 * ignore stale replies
	 */
	if(!pcasg || pcasg->seqNo != pcasgop->seqNo){
		UNLOCK;
		return;
	}

	assert(pcasg->magic == CASG_MAGIC);
	assert(pcasgop->magic == CASG_MAGIC);

	ellDelete(&ca_static->activeCASGOP, &pcasgop->node);
	pcasgop->magic = 0;
	ellAdd(&ca_static->freeCASGOP, &pcasgop->node);

	if(!(args.status&CA_M_SUCCESS)){
		ca_printf(
			"CA Sync Group (id=%d) operation failed because %d \"%s\"\n",
			pcasgop->id,
			args.status,
			ca_message(args.status));
		UNLOCK;
		return;
	}

	/*
 	 * Update the user's variable
	 * (if its a get)
	 */
	if(pcasgop->pValue){
		size = dbr_size_n(args.type, args.count);
		memcpy(pcasgop->pValue, args.dbr, size);
	}

	/*
 	 * decrement the outstanding IO ops count
	 */
	if(pcasg->opPendCount!=0)
		pcasg->opPendCount--;

	UNLOCK;

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
