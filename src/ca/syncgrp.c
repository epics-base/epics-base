/*
 *	@(#)syncgrp.c	1.1 2/7/94
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

#include "assert.h"
#include "db_access.h"
#include "iocinf.h"

#define CASG_MAGIC	0xFAB4CAFE

/*
 * one per outstanding op
 */
typedef struct{
	ELLNODE			node;
	WRITEABLE_CA_SYNC_GID	id;
	void			*pValue;
	unsigned long		magic;
	unsigned long		seqNo;
}CASGOP;

/*
 * one per synch group
 */
typedef struct{
	ELLNODE			node;
	WRITEABLE_CA_SYNC_GID	id;
	unsigned long		magic;
	unsigned long		opPendCount;
	unsigned long		seqNo;
	/*
	 * Asynch Notification
	 */
#ifdef vxWorks
	SEM_ID		sem;
#endif /*vxWorks*/
#ifdef VMS
	int		ef;
#endif /*VMS*/
}CASG;

#ifdef __STDC__
static void io_complete(struct event_handler_args args);
#else /*__STDC__*/
static void io_complete();
#endif /*__STDC__*/


/*
 * ca_sg_init()
 */
#ifdef __STDC__
void ca_sg_init(void)
#else /*__STDC__*/
void ca_sg_init()
#endif /*__STDC__*/
{
	/*
	 * init all sync group lists
	 */
	ellInit(&ca_static->activeCASG);
	ellInit(&ca_static->freeCASG);
	ellInit(&ca_static->activeCASGOP);
	ellInit(&ca_static->freeCASGOP);

	return;
}


/*
 * ca_sg_shutdown()
 */
#ifdef __STDC__
void ca_sg_shutdown(struct ca_static *ca_temp)
#else /*__STDC__*/
void ca_sg_shutdown(ca_temp)
struct ca_static *ca_temp;
#endif /*__STDC__*/
{
	/*
	 * free all sync group lists
	 */
	ellFree(&ca_static->activeCASG);
	ellFree(&ca_static->freeCASG);
	ellInit(&ca_static->activeCASGOP);
	ellInit(&ca_static->freeCASGOP);

	return;
}


/*
 * ca_sg_create()
 */
#ifdef __STDC__
int ca_sg_create(CA_SYNC_GID *pgid)
#else /*__STDC__*/
int ca_sg_create(pgid)
CA_SYNC_GID *pgid;
#endif /*__STDC__*/
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
	memset(pcasg,0,sizeof(*pcasg));
	pcasg->magic = CASG_MAGIC;
	pcasg->id = CLIENT_ID_ALLOC; 
	pcasg->opPendCount = 0;
	pcasg->seqNo = 0;

#ifdef VMS
	status = lib$get_ef(&pcasg->ef);
	assert(status == SS$_NORMAL)
#endif /*VMS*/
#ifdef vxWorks
	pcasg->sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	assert(pcasg->sem);
#endif /*vxWorks*/
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
#ifdef __STDC__
int ca_sg_delete(CA_SYNC_GID gid)
#else /*__STDC__*/
int ca_sg_delete(gid)
CA_SYNC_GID gid;
#endif /*__STDC__*/
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

#ifdef VMS
	status = lib$free_ef(&pcasg->ef);
	assert(status == SS$_NORMAL)
#endif /*VMS*/

#ifdef vxWorks
	status = semDelete(pcasg->sem);
	assert(status == OK);
#endif /*vxWorks*/

	pcasg->magic = 0;
	ellDelete(&ca_static->activeCASG, &pcasg->node);
	ellAdd(&ca_static->freeCASG, &pcasg->node);
	UNLOCK;

	return ECA_NORMAL;
}


/*
 * ca_sg_block()
 */
#ifdef __STDC__
int ca_sg_block(CA_SYNC_GID gid, float timeout)
#else /*__STDC__*/
int ca_sg_block(gid, timeout)
CA_SYNC_GID gid;
float timeout;
#endif /*__STDC__*/
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

	/*
	 * always flush and take care
	 * of connection management
	 * at least once.
	 */
	manage_conn(TRUE);
	cac_send_msg();
	UNLOCK;

	status = ECA_NORMAL;
	beg_time = time(NULL);
	while(pcasg->opPendCount){
		/*
		 * wait for asynch notification
		 */
#ifdef UNIX
		{
                        struct timeval  itimeout;

                        itimeout.tv_usec        = LOCALTICKS;
                        itimeout.tv_sec         = 0;
                        LOCK;
                        recv_msg_select(&itimeout);
                        UNLOCK;
                }
#endif /*UNIX*/
#ifdef vxWorks
		{
                        semTake(pcasg->sem, LOCALTICKS);
		}
#endif /*vxWorks*/
#ifdef VMS
                {
                        int             status;
                        unsigned int    systim[2]={-LOCALTICKS,~0};
       
                        status = sys$setimr(
                                pcasg->ef,
                                systim,
                                NULL,
                                MYTIMERID,
                                NULL);
                        if(status != SS$_NORMAL)
                                lib$signal(status);
                        status = sys$waitfr(pcasg->ef);
                        if(status != SS$_NORMAL)
                                lib$signal(status);
                }
#endif /*VMS*/
		/*
		 * Exit if the timeout has expired
		 */
		if(timeout < time(NULL)-beg_time){
			status = ECA_TIMEOUT;
			break;
		}

		/*
		 * flush and take care of conn
		 * management prior to each time 
		 * that we pend
		 */
		LOCK;
		manage_conn(TRUE);
		cac_send_msg();
		UNLOCK;
	}
	UNLOCK;
	pcasg->opPendCount = 0;
	pcasg->seqNo++;
	return status;
}


/*
 * ca_sg_reset
 */
#ifdef __STDC__
int ca_sg_reset(CA_SYNC_GID gid)
#else /*__STDC__*/
int ca_sg_reset(gid)
CA_SYNC_GID gid;
#endif /*__STDC__*/
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
#ifdef __STDC__
int ca_sg_test(CA_SYNC_GID gid)
#else /*__STDC__*/
int ca_sg_test(gid)
CA_SYNC_GID gid;
#endif /*__STDC__*/
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
#ifdef __STDC__
int     ca_sg_array_put(
CA_SYNC_GID 	gid, 
chtype		type,
unsigned long 	count, 
chid 		chix, 
void 		*pvalue)
#else /*__STDC__*/
int     ca_sg_array_put(gid, type, count, chix, pvalue)
CA_SYNC_GID 	gid; 
chtype		type;
unsigned long 	count; 
chid 		chix; 
void 		*pvalue;
#endif /*__STDC__*/
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
	UNLOCK;

	memset(pcasgop, 0,sizeof(*pcasgop));
	pcasgop->id = gid;
	pcasgop->seqNo = pcasg->seqNo;
	pcasgop->magic = CASG_MAGIC;
	pcasgop->pValue = NULL; /* handler will know its a put */
	ellAdd(&ca_static->activeCASGOP, &pcasgop->node);

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
#ifdef __STDC__
int     ca_sg_array_get(
CA_SYNC_GID 	gid, 
chtype		type,
unsigned long 	count, 
chid 		chix, 
void 		*pvalue)
#else /*__STDC__*/
int     ca_sg_array_get(gid, type, count, chix, pvalue)
CA_SYNC_GID 	gid; 
chtype		type;
unsigned long 	count; 
chid 		chix; 
void 		*pvalue;
#endif /*__STDC__*/
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
	UNLOCK;

	memset(pcasgop, 0,sizeof(*pcasgop));
	pcasgop->id = gid;
	pcasgop->seqNo = pcasg->seqNo;
	pcasgop->magic = CASG_MAGIC;
	pcasgop->pValue = pvalue;
	ellAdd(&ca_static->activeCASGOP, &pcasgop->node);

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
#ifdef __STDC__
static void io_complete(struct event_handler_args args)
#else /*__STDC__*/
static void io_complete(args)
struct event_handler_args args;
#endif /*__STDC__*/
{
	unsigned long	size;
	int		status;
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
			"CA Sync Group (id=%d) operation failed because \"%s\"\n",
			pcasgop->id,
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
#ifdef VMS
	if(pcasg->opPendCount == 0){
		sys$setef(pcasg->ef)
	}
#endif /*VMS*/
#ifdef vxWorks
	if(pcasg->opPendCount == 0){
		status = semGive(pcasg->sem);
		assert(status == OK);
	}
#endif /*vxWorks*/

	return;
}
