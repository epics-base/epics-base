/*
 *
 *	share/src/libCom/$Id$	
 *
 *	A file descriptor manager for use with the UNIX system call select
 *
 *	Author	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		505 665 1831
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
 *
 *
 *
 *
 *
 * 	History
 *	.01 joh	071890	written - uses lots of memory and could be 
 *			slow with many fds
 * 	.02 joh	110990	Added alarms
 * 	.03 joh	110990	now uses the number of fd the system is 
 *			configured for- a linked list would be faster
 *			and use less memory
 *	.04 joh 050291	Now uses a linked list entry for each fd
 *	.05 rac 062891  get <types> and <time> from <sys/
 *	.06 joh 050792  ported to V5 vxWorks now that wrs provides
 *			a fully funtional select()
 *	.07 joh 050792	all routines now return status
 *	.08 joh 091192	made write callbacks be oneshots in keeping
 *			with select() system call operation	
 *	.09 joh 091192	fixed delete pending field uninitialized if the
 *			fdentry is reused
 *	.10 joh 091192	added SCCS ID	
 *	.11 joh	092192	added std header
 *	.12 joh	092192	return an error if the timeout supplied to fdmgr
 *			pend event is out of range	
 *	.13 joh	111292	turned on task delete disable around select
 *			after reading warning about deleting a task
 *			while it is in select() in the V5 vxWorks
 *			reference manual.	
 *	.14 joh	011993	If its a poll dont add a timeout (and more delays)
 *			if we are in fdmgr_pend_event()	
 *	.15 joh	011993	Created fdmgr header file	
 *	.16 joh	011993	Converted to ANSI C
 *
 *	NOTES:
 *
 *	.01 joh 091192	Write fd callbacks are one shots consistent 
 *			with the lower likelyhood of select blocking 
 *			on a fd write.	 
 *
 *      .02 joh 012093  **** WARNING ****
 *                      Assume that a timer id returned by fdmgr_add_timeout()
 *                      will be reused (and returned from a subsequent call to
 *                      fdmgr_add_timeout()) after either of the following two
 *                      circumstances occur:
 *                      1) You delete the timer referenced by the timer id with
 *                              fdmgr_clear_timeout()
 *                      2) The timer expires (and the handler which you 
 *				have established for the timer id is executed)
 *                      Take care not to attempt to delete a timer which
 *                      has already expired (and therefore executed your timer
 *                      expiration handler subroutine). Attempting to
 *                      delete a timer which has already expired could
 *                      result in deletion of the wrong timer.
 *
 *	.03 joh 012193	terse DOCUMENTATION has been added to the header file 
 *			share/epicsH/fdmgr.h
 *
 */

static char	*pSccsId = "@(#) $Id$";

/*
 * ANSI
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#ifdef vxWorks
#include <vxWorks.h>
#include <taskLib.h>
#include <sysLib.h>
#include <tickLib.h>
#include <logLib.h>
#include <selectLib.h>
#endif

#include <fdmgr.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef OK
#define OK 0
#endif
#ifndef ERROR
#define ERROR (-1)
#endif
#ifndef NULL
#define NULL 0
#endif

#ifndef LOCAL
#define LOCAL static
#endif

#ifndef max
#define max(x, y)  (((x) < (y)) ? (y) : (x))
#endif

typedef struct{
	ELLNODE		node;
	int		fd;
	enum fdi_type	fdi;		/* the type of fd interest */
	fd_set		*pfds;
	void		(*pfunc)();
	void		*param;
	int		delete_pending;
}fdentry;

#if defined(vxWorks)
#	define LOCK(PFDCTX)	FASTLOCK(&(PFDCTX)->lock)
#	define UNLOCK(PFDCTX)	FASTUNLOCK(&(PFDCTX)->lock)
#	define UNLOCK_FDMGR_PEND_EVENT(PFDCTX) \
		FASTUNLOCK(&(PFDCTX)->fdmgr_pend_event_lock) \
		(PFDCTX)->fdmgr_pend_event_tid = NULL;
#	define LOCK_EXPIRED(PFDCTX) \
		FASTLOCK(&(PFDCTX)->expired_alarm_lock)
#	define UNLOCK_EXPIRED(PFDCTX) \
		FASTUNLOCK(&(PFDCTX)->expired_alarm_lock)	
#	define LOCK_FD_HANDLER(PFDCTX) \
		FASTLOCK(&(PFDCTX)->fd_handler_lock)
#	define UNLOCK_FD_HANDLER(PFDCTX) \
		FASTUNLOCK(&(PFDCTX)->fd_handler_lock)	
#elif defined(UNIX) || defined(VMS) || defined(_WIN32)
#	define LOCK(PFDCTX)
#	define UNLOCK(PFDCTX)
#	define UNLOCK_FDMGR_PEND_EVENT(PFDCTX) \
		{(PFDCTX)->fdmgr_pend_event_in_use--;}
#	define LOCK_EXPIRED(PFDCTX)
#	define UNLOCK_EXPIRED(PFDCTX)
#	define LOCK_FD_HANDLER(PFDCTX)
#	define UNLOCK_FD_HANDLER(PFDCTX)
#else
#error Please define the host OS type
#endif


#define USEC_PER_SEC 1000000

#define printf @@@@ Please dont use printf in this source @@@@
LOCAL int fdmgrPrintf(char *pformat, ...);

LOCAL int fdmgr_gettimeval(
fdctx           *pfdctx,
struct timeval  *pt
);

LOCAL void select_alarm(
fdctx   *pfdctx
);

LOCAL int fdmgr_select(
fdctx		*pfdctx,
struct timeval	*ptimeout
);

LOCAL void fdmgr_finish_off_fdentry(
fdctx                   *pfdctx,
register fdentry        *pfdentry
);

LOCAL void lockFDMGRPendEvent (fdctx *pfdctx);

/*
 * This routine is to be only called from fdmgr_pend_event()
 * If other uses are needed then locking issues must be
 * reinvestigated
 */
LOCAL void process_alarm_queue(
	fdctx           *pfdctx,
	struct timeval  *poffset
);


/*
 *
 *	fdmgr_init()
 *
 */
fdctx *fdmgr_init(void)
{
	fdctx 		*pfdctx;

	pfdctx = (fdctx *) calloc(1, sizeof(fdctx));
#	if defined(vxWorks)
		FASTLOCKINIT(&pfdctx->lock);
		FASTLOCKINIT(&pfdctx->fdmgr_pend_event_lock);
		FASTLOCKINIT(&pfdctx->expired_alarm_lock);
		FASTLOCKINIT(&pfdctx->fd_handler_lock);
		pfdctx->clk_rate = sysClkRateGet();
		pfdctx->last_tick_count = tickGet();
#	endif
	ellInit(&pfdctx->fdentry_list);
	ellInit(&pfdctx->fdentry_in_use_list);
	ellInit(&pfdctx->fdentry_free_list);
	ellInit(&pfdctx->alarm_list);
	ellInit(&pfdctx->expired_alarm_list);
	ellInit(&pfdctx->free_alarm_list);

	/*
 	 * returns NULL if unsuccessfull
	 */
	return pfdctx;
}


/*
 *
 *	fdmgr_delete()
 *
 */
int fdmgr_delete(fdctx *pfdctx)
{
	if(!pfdctx){
		return ERROR;
	}

#	if defined(vxWorks)
		FASTLOCKFREE(&pfdctx->lock);
		FASTLOCKFREE(&pfdctx->fdmgr_pend_event_lock);
		FASTLOCKFREE(&pfdctx->expired_alarm_lock);
		FASTLOCKFREE(&pfdctx->fd_handler_lock);
#	endif

	return OK;
}


/*
 * 	fdmgr_add_timeout()
 */
fdmgrAlarm *fdmgr_add_timeout(
fdctx 		*pfdctx,
struct timeval 	*ptimeout,
void		(*func)(),
void		*param
)
{
	fdmgrAlarm		*palarm=NULL;
	fdmgrAlarm		*pa;
	struct timeval	t;
	int		status;

	if(ptimeout->tv_sec < 0)
		return NULL;
	if(ptimeout->tv_usec < 0)
		return NULL;

	status = fdmgr_gettimeval(pfdctx, &t);
	if(status < 0)
		return NULL;

	LOCK(pfdctx);
	palarm = (fdmgrAlarm *) ellGet(&pfdctx->free_alarm_list);
	UNLOCK(pfdctx);
	if(!palarm){
		palarm = (fdmgrAlarm *) malloc(sizeof(fdmgrAlarm));
		if(!palarm){
			return NULL;
		}
	}

	/*
	 * force all fields to a known state
	 */
	memset((char *)palarm, 0, sizeof(*palarm));

	ptimeout->tv_sec += ptimeout->tv_usec/USEC_PER_SEC;
	ptimeout->tv_usec = ptimeout->tv_usec%USEC_PER_SEC;

	palarm->func = func;
	palarm->param = param;
	
	palarm->t.tv_sec = 
		t.tv_sec + ptimeout->tv_sec + 
		(t.tv_usec + ptimeout->tv_usec)/USEC_PER_SEC;

	palarm->t.tv_usec = (t.tv_usec + ptimeout->tv_usec)%USEC_PER_SEC;

	LOCK(pfdctx);
	for(	pa=(fdmgrAlarm *)pfdctx->alarm_list.node.next; 
		pa; 
		pa=(fdmgrAlarm *)pa->node.next){
		if(pa->t.tv_sec > palarm->t.tv_sec){
			break;
		}
		else {
			if(pa->t.tv_sec == palarm->t.tv_sec){
				if(pa->t.tv_usec > palarm->t.tv_usec){
					break;
				}
			}
		}
	}
	if(pa){
	ellInsert(&pfdctx->alarm_list, pa->node.previous, (ELLNODE*)palarm);
	}
	else{
		ellAdd(&pfdctx->alarm_list,(ELLNODE*) palarm);
	}
	palarm->alt = alt_alarm;
	UNLOCK(pfdctx);

	return (void *) palarm;
}


/*
 *
 *	fdmgr_clear_timeout()
 *
 */
int fdmgr_clear_timeout(
fdctx 		*pfdctx,
fdmgrAlarm		*palarm
)
{
	int 			status;
	enum alarm_list_type	alt;

	status = ERROR;

	LOCK(pfdctx);
	alt = palarm->alt;
	if(alt == alt_alarm){
		ellDelete(&pfdctx->alarm_list, (ELLNODE*)palarm);
		ellAdd(&pfdctx->free_alarm_list, (ELLNODE*)palarm);
		palarm->alt = alt_free;
		status = OK;
	}
	else if(alt == alt_expired){
		/*
		 * disable futher use of this alarm
		 */	
		palarm->func = NULL;
	}
	UNLOCK(pfdctx);

	if(alt == alt_expired){

		/*
		 * If we are not called below fdmgr_pend_event()
		 * then wait for completion before returning.
		 * 
		 * waiting for completion of this event routine
		 * when we are in it would cause a deadlock.
		 */
		/*
		 * Wait for it to expire
		 * (and be added to the free list)
		 *
		 * (only applies to multithreaded OS)
		 *
		 * taking the EXPIRED lock forces this routine
		 * to wait until any timer experation
		 * handlers in progress complete
		 *
		 * The primary LOCK is not applied here to 
		 * avoid deadlocking with any fdmgr
		 * routines which might be potentially
		 * called from within an expired alarm 
		 * handler
		 *
		 */
#		ifdef vxWorks
			if(pfdctx->fdmgr_pend_event_tid != taskIdCurrent){
				LOCK_EXPIRED(pfdctx);
				UNLOCK_EXPIRED(pfdctx);
			}
#		endif

		status = OK;
	}

	return status;
}


/*
 *
 *	fdmgr_add_fd()
 *
 *	this rouitine is supplied solely for compatibility	
 *	with earlier versions of this software
 */
int fdmgr_add_fd(
fdctx 	*pfdctx,
int	fd,
void	(*pfunc)(),
void	*param
)
{
	int status;

	status = fdmgr_add_callback(
			pfdctx, 
			fd, 
			fdi_read,
			pfunc, 
			param);

	return status;
}


/*
 *
 *	fdmgr_add_fd_callback()
 *
 */
int fdmgr_add_callback(
fdctx 		*pfdctx,
int		fd,
enum fdi_type	fdi,
void		(*pfunc)(),
void		*param
)
{
	fdentry		*pfdentry;
	fd_set		*pfds;

	switch(fdi){
	case fdi_read:
		pfds = &pfdctx->readch;
		break;
	case fdi_write:
     		pfds = &pfdctx->writech;
		break;
	case fdi_excp:
     		pfds = &pfdctx->excpch;
		break;
	default:
		return ERROR;
	}

	pfdctx->maxfd = max(pfdctx->maxfd, fd+1);
	LOCK(pfdctx);
	pfdentry = (fdentry *) ellGet(&pfdctx->fdentry_free_list);
	UNLOCK(pfdctx);

	if(!pfdentry){
		pfdentry = (fdentry *) malloc(sizeof(fdentry));
		if(!pfdentry){
			return ERROR;
		}
	}

	/*
	 * force all fields to a known state
	 */
	memset((char *)pfdentry, 0, sizeof(*pfdentry));

	pfdentry->fd = fd;
	pfdentry->fdi = fdi;
	pfdentry->pfds = pfds;
	pfdentry->pfunc = pfunc;
	pfdentry->param = param;
	pfdentry->delete_pending = FALSE;

	LOCK(pfdctx);
	ellAdd(&pfdctx->fdentry_list, (ELLNODE*)pfdentry);
	UNLOCK(pfdctx);

	return OK;
}


/*
 *
 *	fdmgr_clear_fd()
 *
 *	included solely for compatibility with previous release
 *
 */
int fdmgr_clear_fd(
fdctx 	*pfdctx,
int	fd 
)
{
	return fdmgr_clear_callback(pfdctx, fd, fdi_read);
}


/*
 *
 *	fdmgr_clear_callback()
 *
 */
int fdmgr_clear_callback(
fdctx 		*pfdctx,
int		fd,
enum fdi_type	fdi
)
{
	register fdentry	*pfdentry;
	int			status;
	int			delete_pending;

	delete_pending = FALSE;
	status = ERROR;

	LOCK(pfdctx);
	for(	pfdentry = (fdentry *) pfdctx->fdentry_list.node.next;
		pfdentry;
		pfdentry = (fdentry *) pfdentry->node.next){

		if(pfdentry->fd == fd && pfdentry->fdi == fdi){
			ellDelete(&pfdctx->fdentry_list, (ELLNODE*)pfdentry);
			fdmgr_finish_off_fdentry(pfdctx, pfdentry);
			status = OK;
			break;
		}
	}

	/*
	 * also check entries which are currently in use
	 * and therefore undeletable until after the
	 * fdmgr gets thru fdmgr_pend_event
	 */
        for(    pfdentry = (fdentry *) pfdctx->fdentry_in_use_list.node.next;
                pfdentry;
                pfdentry = (fdentry *) pfdentry->node.next){

		if(pfdentry->fd == fd && pfdentry->fdi == fdi){
			delete_pending = TRUE;
			pfdentry->delete_pending = TRUE;
                        status = OK;
                        break;
                }
        }
	UNLOCK(pfdctx);
	
	/*
	 * wait for it to finish if it is in progress
	 * when running in a multithreaded environment
	 */
#	ifdef vxWorks
		if(delete_pending == TRUE){
@@@@ review all of this delete pending stuff
			LOCK_FD_HANDLER(pfdctx);
			UNLOCK_FD_HANDLER(pfdctx);
		}
#	endif

	/*
	 * If it is an ukn fd its a problem worth printing out
	 */
	if(status != OK){
		fdmgrPrintf("fdmg: delete of ukn fd failed\n");
	}

	return status;
}


/*
 *
 *	fdmgr_finish_off_fdentry()
 *
 *	!! LOCK(pfdctx) must be applied !!
 *
 */
LOCAL void fdmgr_finish_off_fdentry(
fdctx 			*pfdctx,
register fdentry	*pfdentry
)
{
     	FD_CLR(pfdentry->fd, pfdentry->pfds);
	ellAdd(&pfdctx->fdentry_free_list, (ELLNODE*)pfdentry);
}


/*
 *
 *	fdmgr_pend_event()
 *
 */
int fdmgr_pend_event(
fdctx 				*pfdctx,
struct timeval 			*ptimeout 
)
{
	int			status;
	extern			errno;
	struct timeval		t;
	fdmgrAlarm			*palarm;


	lockFDMGRPendEvent(pfdctx);

	/*
	 * If its a poll dont add a timeout
	 * (and more delays) 
	 */
	if(ptimeout->tv_sec == 0 && ptimeout->tv_usec == 0){
		pfdctx->select_tmo = TRUE;
		t.tv_sec = 0;
		t.tv_usec = 0;	
		/*
		 * silence gcc warnings
		 */
		palarm = NULL;
	}
	else{
		pfdctx->select_tmo = FALSE;
		palarm = fdmgr_add_timeout(
				pfdctx, 
				ptimeout, 
				select_alarm,
				pfdctx);
		if(!palarm){
			return ERROR;
		}
		process_alarm_queue(pfdctx, &t);
	}

	while(TRUE){
		status = fdmgr_select(pfdctx, &t);
		process_alarm_queue(pfdctx, &t);
		if(status){
			break;
		}
		if(pfdctx->select_tmo)
			break;
	}

	if(pfdctx->select_tmo==FALSE)
		fdmgr_clear_timeout(pfdctx, palarm);

	UNLOCK_FDMGR_PEND_EVENT(pfdctx);

	return OK;
}



/*
 *
 *	fdmgr_select()
 *
 *	returns TRUE if any labor was performed, otherwise FALSE
 *
 */
LOCAL int fdmgr_select(
fdctx 				*pfdctx,
struct timeval 			*ptimeout
)
{
	register fdentry	*pfdentry;
	int			labor_performed;
	int			status;

	labor_performed = FALSE;

	LOCK(pfdctx);
	for(	pfdentry = (fdentry *) pfdctx->fdentry_list.node.next;
		pfdentry;
		pfdentry = (fdentry *) pfdentry->node.next){

     		FD_SET(pfdentry->fd, pfdentry->pfds);
	}
	UNLOCK(pfdctx);

	/*
	 * V5 vxWorks ref man has an ominous
	 * comment about deleting a task while
	 * it is in select() so I am turning
	 * on task delete disable to be safe
	 */
#	ifdef vxWorks 
		taskSafe();
#	endif
  	status = select(
			pfdctx->maxfd,
			&pfdctx->readch,
			&pfdctx->writech,
			&pfdctx->excpch,
			ptimeout);
#	ifdef vxWorks 
		taskUnsafe();
#	endif
	if(status == 0){
		return labor_performed;
	}
	else if(status < 0){
		if(errno == EINTR)
			;
		else if(errno == EINVAL)
			fdmgrPrintf(	
				"fdmgr: bad select args ? %d %d %d\n",
				pfdctx->maxfd,
				ptimeout->tv_sec,
				ptimeout->tv_usec);
		else
			fdmgrPrintf(	
				"fdmgr: error from select %d\n",
				errno);

		return labor_performed;
	}

	pfdentry = (fdentry *) &pfdctx->fdentry_list.node;
	while(TRUE){

		LOCK(pfdctx)
		pfdentry = (fdentry *) pfdentry->node.next;
		if(pfdentry){
			ellDelete(&pfdctx->fdentry_list, (ELLNODE*)pfdentry);
			/*
			 *
			 * holding place where it can be marked 
			 * pending delete but not deleted
 			 *
			 */
			ellAdd(&pfdctx->fdentry_in_use_list, (ELLNODE*)pfdentry);
		}
		UNLOCK(pfdctx)
	
		if(!pfdentry){
			break;
		}

		/*
		 * check for fd active
		 */
		if(FD_ISSET(pfdentry->fd, pfdentry->pfds)){
     			FD_CLR(pfdentry->fd, pfdentry->pfds);

			/*
			 * sync with clear
			 */
			LOCK_FD_HANDLER(pfdctx);

			/*
			 * Dont execute if they have cleared
			 * it
			 */
			if(!pfdentry->delete_pending){
     				(*pfdentry->pfunc)(pfdentry->param);
				labor_performed = TRUE;
			}
			UNLOCK_FD_HANDLER(pfdctx);
		}

		LOCK(pfdctx)
		ellDelete(&pfdctx->fdentry_in_use_list, (ELLNODE*)pfdentry);

		/*
		 * if it is marked pending delete
		 * reset it and place it on the free list	
		 *
		 * write fd interest is treated as a one shot
		 * so remove them after each action
		 */
		if(pfdentry->delete_pending || pfdentry->fdi==fdi_write){
			fdmgr_finish_off_fdentry(pfdctx, pfdentry);
		}
		else{
			ellAdd(&pfdctx->fdentry_list, (ELLNODE*)pfdentry);
		}
		UNLOCK(pfdctx)

	}

	return labor_performed;
}



/*
 *
 *	process_alarm_queue()
 *
 *	only to be called by fdmgr_pend_event(). If other uses
 *	come up then the locking must be revisited
 */
LOCAL void process_alarm_queue(
fdctx 		*pfdctx,
struct timeval	*poffset
)
{
	struct timeval	t;
	int		status;
	fdmgrAlarm		*pa;
	fdmgrAlarm		*nextpa;

	status = fdmgr_gettimeval(pfdctx, &t);
	assert (status >= 0);

	LOCK(pfdctx);
	for(pa = (fdmgrAlarm*)pfdctx->alarm_list.node.next; pa; pa = nextpa){
		if(pa->t.tv_sec > t.tv_sec)
			break;
		else if(pa->t.tv_sec == t.tv_sec)
			if(pa->t.tv_usec > t.tv_usec)
				break;

		nextpa = (fdmgrAlarm*)pa->node.next;
		ellDelete(&pfdctx->alarm_list, (ELLNODE*)pa);
		ellAdd(&pfdctx->expired_alarm_list, (ELLNODE*)pa);
		pa->alt = alt_expired;
	}
	UNLOCK(pfdctx);

	/*
	 * Done this way so I am not traversing the alarm que while
	 * they are in an alarm handler (and potentially modifying
 	 * the alarm queue).
	 */
	/*
 	 * no need to lock here since this list is only touched
	 * by this routine, this routine is not exported
	 * and I am only allowing one thread in fdmgr_pend_event()
	 * at at time.
	 */
	/*
	 * I dont want the primary LOCK to be applied while in their
 	 * alarm handler as this would prevent them from
	 * calling fdmgr routines from within a handler
	 */
	/*
	 * applying the expired lock here synchronizes
	 * properly with the clear event routine
	 * on multithreaded OS
	 */
	LOCK_EXPIRED(pfdctx);
	pa = (fdmgrAlarm*) pfdctx->expired_alarm_list.node.next;
	while(pa){
		void	(*pfunc)();

		/*
		 * check to see if it has been disabled 
		 * by a clear timer call
		 */
		pfunc = pa->func;
		if(pfunc){
			(*pfunc)(pa->param);
		}
		pa = (fdmgrAlarm*)pa->node.next;
	}
	UNLOCK_EXPIRED(pfdctx);

	/*
	 * mark it as a member of the free list while the 
	 * LOCK is applied and then force the entire
	 * expired list onto the free list
	 */
	LOCK(pfdctx);
	pa = (fdmgrAlarm*) pfdctx->expired_alarm_list.node.next;
	while(pa){
		pa->alt = alt_free;
		pa = (fdmgrAlarm *) pa->node.next;
	}
	ellConcat(&pfdctx->free_alarm_list, &pfdctx->expired_alarm_list);

	pa = (fdmgrAlarm *)pfdctx->alarm_list.node.next;
	if(pa){
		if(pa->t.tv_usec >= t.tv_usec){
			poffset->tv_sec = pa->t.tv_sec - t.tv_sec;
			poffset->tv_usec = pa->t.tv_usec - t.tv_usec;
		}else{
			/* else borrow */
			poffset->tv_sec = pa->t.tv_sec - t.tv_sec - 1;
			poffset->tv_usec = pa->t.tv_usec 
				+ USEC_PER_SEC - t.tv_usec;
		}
	}else{
		poffset->tv_sec = 0;
		poffset->tv_usec = 0;
	}
	UNLOCK(pfdctx);
}


/*
 *
 *	select_alarm()
 *
 */
LOCAL void select_alarm(
fdctx 	*pfdctx
)
{
	pfdctx->select_tmo = TRUE;
}




/*
 *
 * UNIX & VMS
 * fdmgr_gettimeval
 *
 */
#if defined(UNIX) || defined(VMS)
LOCAL int fdmgr_gettimeval(
fdctx 		*pfdctx,
struct timeval	*pt 
)
{
	struct timezone		tz;

	return gettimeofday(pt, &tz);
}
#endif


/*
 *
 * WIN32
 * fdmgr_gettimeval
 *
 *
 */
#ifdef _WIN32
LOCAL int fdmgr_gettimeval(
fdctx           *pfdctx,
struct timeval  *pt
)
{
        SYSTEMTIME st;
        GetSystemTime(&st);
        pt->tv_sec = (long)st.wSecond + (long)st.wMinute*60 + (long)st.wHour*360
0;
        pt->tv_usec = st.wMilliseconds*1000;

        return 0;
}
#endif


/*
 * vxWorks
 * fdmgr_gettimeval
 *
 */
#ifdef vxWorks
LOCAL int fdmgr_gettimeval(
fdctx 		*pfdctx,
struct timeval	*pt
)
{
	unsigned long		current;

	LOCK(pfdctx);
	current = tickGet();
	if(current<pfdctx->last_tick_count){
		pfdctx->sec_offset += (~(unsigned long)0)/pfdctx->clk_rate;
	}
	pfdctx->last_tick_count = current;

	pt->tv_sec = current/pfdctx->clk_rate;
	pt->tv_sec += pfdctx->sec_offset;
	pt->tv_usec = ((current%pfdctx->clk_rate)*
			USEC_PER_SEC)/pfdctx->clk_rate;
	UNLOCK(pfdctx);

	return OK;
}
#endif


/*
 * lockFDMGRPendEvent()
 */
LOCAL void lockFDMGRPendEvent (fdctx *pfdctx)
{
#	if defined(vxWorks)
		FASTLOCK(&pfdctx->fdmgr_pend_event_lock); 
		pfdctx->fdmgr_pend_event_tid = taskIdCurrent;
#	else
		assert (pfdctx->fdmgr_pend_event_in_use==0); 
		pfdctx->fdmgr_pend_event_in_use++; 
#	endif
}


/*
 *
 *
 *      fdmgrPrintf()
 *
 *      Dump error messages to the appropriate place
 *
 */
LOCAL int fdmgrPrintf(char *pformat, ...)
{
        va_list         args;
        int             status;

        va_start(args, pformat);

#ifndef vxWorks
        status = vfprintf(
                        stderr,
                        pformat,
                        args);
#else /*vxWorks*/
        {
                int     logMsgArgs[6];
                int     i;

                for(i=0; i< NELEMENTS(logMsgArgs); i++){
                        logMsgArgs[i] = va_arg(args, int);
                }

                status = logMsg(
                                pformat,
                                logMsgArgs[0],
                                logMsgArgs[1],
                                logMsgArgs[2],
                                logMsgArgs[3],
                                logMsgArgs[4],
                                logMsgArgs[5]);

        }
#endif /*vxWorks*/

        va_end(args);

        return status;
}

