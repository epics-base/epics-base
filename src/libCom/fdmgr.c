/*
 *
 *	F D M G R . C
 *
 *	Author	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		505 665 1831
 *
 *
 *	A manager for use with the UNIX system call select
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
 *	.07 joh 050792	allroutines now return status
 *	.08 joh 091192	made write callbacks be oneshots in keeping
 *			with select() system call operation	
 *	.09 joh 091192	fixed delete pending field uninitialized if the
 *			fdentry is reused
 *
 *	NOTES:
 *	.01 write fd callbacks are one shots consistent with the lower
 *	likelyhood of select blocking on a fd write.	 
 *
 */

#if defined(vxWorks)
#	include <vxWorks.h>
#	include	<vxTypes.h>
#	include	<systime.h>
#	include	<errno.h>
#	include	<fast_lock.h>
#elif defined(UNIX)
#	include	<sys/types.h>
#	include	<sys/time.h>
#	include	<errno.h>
#else
@@@@ Dont compile in this case @@@@
#endif

#ifndef INClstLibh
#include 	<lstLib.h>
#endif

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

#ifndef max
#define max(x, y)  (((x) < (y)) ? (y) : (x))
#endif

enum fdi_type { fdi_read, fdi_write, fdi_excp};

typedef struct{
	NODE		node;
	int		fd;
	enum fdi_type	fdi;		/* the type of fd interest */
	fd_set		*pfds;
	void		(*pfunc)();
	void		*param;
	int		delete_pending;
}fdentry;

typedef struct{
	unsigned	maxfd;
	LIST		fdentry_list;
	LIST		fdentry_in_use_list;
	LIST		fdentry_free_list;
	LIST		alarm_list;
	LIST		expired_alarm_list;
	LIST		free_alarm_list;
	int		select_tmo;
#	if defined(vxWorks)
	FAST_LOCK	lock;
	FAST_LOCK	fdmgr_pend_event_lock;
	unsigned	clk_rate;	/* ticks per sec */
#	elif defined(UNIX)
	unsigned	fdmgr_pend_event_in_use;
#	endif
	fd_set		readch;
	fd_set		writech;
	fd_set		excpch;
}fdctx;

typedef struct{
	NODE		node;
	struct timeval	t;
	void		(*func)();
	void		*param;
}alarm;

#if defined(vxWorks)
#	define LOCK(pfdctx)(PFDCTX)	FASTLOCK(&(PFDCTX)->lock)
#	define UNLOCK(pfdctx)(PFDCTX)	FASTUNLOCK(&(PFDCTX)->lock)
#	define LOCK_FDMGR_PEND_EVENT(PFDCTX) \
		FASTLOCK(&(PFDCTX)->fdmgr_pend_event_lock)
#	define UNLOCK_FDMGR_PEND_EVENT(PFDCTX) \
		FASTLOCK(&(PFDCTX)->fdmgr_pend_event_lock)
#	define printf	logMsg
#	define fdmgr_gettimeval		fdmgr_vxWorks_gettimeval
#elif defined(UNIX)
#	define LOCK(PFDCTX)
#	define UNLOCK(PFDCTX)
#	define LOCK_FDMGR_PEND_EVENT(PFDCTX) \
	{ \
		if((PFDCTX)->fdmgr_pend_event_in_use){ \
			printf("Double invocation of fdmgr_pend_event()\n"); \
			abort(); \
		} \
		(PFDCTX)->fdmgr_pend_event_in_use++; \
	}
#	define UNLOCK_FDMGR_PEND_EVENT(PFDCTX) \
		{(PFDCTX)->fdmgr_pend_event_in_use--;}
#	define fdmgr_gettimeval		fdmgr_UNIX_gettimeval
#else
@@@@ dont compile in this case @@@@
#endif

#define USEC_PER_SEC 1000000

void 	select_alarm();
void	fdmgr_finish_off_fdentry();


#if defined(vxWorks)
#	define abort(A)	taskSuspend(taskIdSelf())
#endif


/*
 *
 *	fdmgr_init()
 *
 */
fdctx	
*fdmgr_init()
{
	fdctx 		*pfdctx;

	pfdctx = (fdctx *) calloc(1, sizeof(fdctx));

	/*
 	 * returns NULL if unsuccessfull
	 */
	return pfdctx;
}




/*
 *
 * 	fdmgr_add_timeout()
 *
 *
 */
alarm 
*fdmgr_add_timeout(pfdctx, ptimeout, func ,param)
fdctx 		*pfdctx;
struct timeval 	*ptimeout;
void		(*func)();
void		*param;
{
	alarm		*palarm=NULL;
	alarm		*pa;
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
	palarm = (alarm *) lstGet(&pfdctx->free_alarm_list);
	UNLOCK(pfdctx);
	if(!palarm){
		palarm = (alarm *) malloc(sizeof(alarm));
		if(!palarm){
			return NULL;
		}
	}

	/*
	 * force all fields to a known state
	 */
	memset(palarm, 0, sizeof(*palarm));

	ptimeout->tv_sec += ptimeout->tv_usec/USEC_PER_SEC;
	ptimeout->tv_usec = ptimeout->tv_usec%USEC_PER_SEC;

	palarm->func = func;
	palarm->param = param;
	
	palarm->t.tv_sec = 
		t.tv_sec + ptimeout->tv_sec + 
		(t.tv_usec + ptimeout->tv_usec)/USEC_PER_SEC;

	palarm->t.tv_usec = (t.tv_usec + ptimeout->tv_usec)%USEC_PER_SEC;

	LOCK(pfdctx);
	for(	pa=(alarm *)pfdctx->alarm_list.node.next; 
		pa; 
		pa=(alarm *)pa->node.next){
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
		lstInsert(&pfdctx->alarm_list, pa->node.previous, palarm);
	}
	else{
		lstAdd(&pfdctx->alarm_list, palarm);
	}
	UNLOCK(pfdctx);

	return (void *) palarm;
}


/*
 *
 *	fdmgr_clear_timeout()
 *
 */
int
fdmgr_clear_timeout(pfdctx, palarm)
fdctx 		*pfdctx;
alarm		*palarm;
{
	int status;

	LOCK(pfdctx);
	status = lstFind(&pfdctx->alarm_list, palarm);
	if(status != ERROR){
		lstDelete(&pfdctx->alarm_list, palarm);
		lstAdd(&pfdctx->free_alarm_list, palarm);
	}
	UNLOCK(pfdctx);

	return status;
}


/*
 *
 *	fdmgr_add_fd()
 *
 *	this rouitine is supplied solely for compatibility	
 *	with earlier versions of this software
 */
int 
fdmgr_add_fd(pfdctx, fd, pfunc, param)
fdctx 	*pfdctx;
int	fd;
void	(*pfunc)();
void	*param;
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
int 
fdmgr_add_callback(pfdctx, fd, fdi, pfunc, param)
fdctx 		*pfdctx;
int		fd;
enum fdi_type	fdi;
void		(*pfunc)();
void		*param;
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
	pfdentry = (fdentry *) lstGet(&pfdctx->fdentry_free_list);
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
	memset(pfdentry, 0, sizeof(*pfdentry));

	pfdentry->fd = fd;
	pfdentry->fdi = fdi;
	pfdentry->pfds = pfds;
	pfdentry->pfunc = pfunc;
	pfdentry->param = param;
	pfdentry->delete_pending = FALSE;

	LOCK(pfdctx);
	lstAdd(&pfdctx->fdentry_list, pfdentry);
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
int
fdmgr_clear_fd(pfdctx, fd)
fdctx 	*pfdctx;
int	fd;
{
	return fdmgr_clear_callback(pfdctx, fd, fdi_read);
}


/*
 *
 *	fdmgr_clear_callback()
 *
 */
int
fdmgr_clear_callback(pfdctx, fd, fdi)
fdctx 		*pfdctx;
int		fd;
enum fdi_type	fdi;
{
	register fdentry	*pfdentry;
	int			status;

	status = ERROR;

	LOCK(pfdctx);
	for(	pfdentry = (fdentry *) pfdctx->fdentry_list.node.next;
		pfdentry;
		pfdentry = (fdentry *) pfdentry->node.next){

		if(pfdentry->fd == fd && pfdentry->fdi == fdi){
			lstDelete(&pfdctx->fdentry_list, pfdentry);
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
			pfdentry->delete_pending = TRUE;
                        status = OK;
                        break;
                }
        }
	UNLOCK(pfdctx);
	
	/*
	 * If it is an ukn fd its a bug worth printing out
	 */
	if(status != OK){
		printf("fdmg: delete of ukn fd failed\n");
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
static void
fdmgr_finish_off_fdentry(pfdctx, pfdentry)
fdctx 			*pfdctx;
register fdentry	*pfdentry;
{
     	FD_CLR(pfdentry->fd, pfdentry->pfds);
	lstAdd(&pfdctx->fdentry_free_list, pfdentry);
}


/*
 *
 *	fdmgr_pend_event()
 *
 */
int
fdmgr_pend_event(pfdctx, ptimeout)
fdctx 				*pfdctx;
struct timeval 			*ptimeout;
{
	int			status;
	extern			errno;
	struct timeval		t;
	alarm			*palarm;

	/*
 	 * This routine is declared here since is only 
	 * intended for use by this routine. If other
	 * uses are needed then the locking issues must be
	 * reinvestigated
	 */
	void 			process_alarm_queue();

	LOCK_FDMGR_PEND_EVENT(pfdctx);

	pfdctx->select_tmo = FALSE;
	palarm = fdmgr_add_timeout(pfdctx, ptimeout, select_alarm ,pfdctx);

	process_alarm_queue(pfdctx, &t);

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
static int
fdmgr_select(pfdctx, ptimeout)
fdctx 				*pfdctx;
struct timeval 			*ptimeout;
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

  	status = select(
			pfdctx->maxfd,
			&pfdctx->readch,
			&pfdctx->writech,
			&pfdctx->excpch,
			ptimeout);
	if(status == 0){
		return labor_performed;
	}
	else if(status < 0){
		if(errno == EINTR)
			;
		else if(errno == EINVAL)
			printf(	"fdmgr: bad select args ? %d %d %d\n",
				pfdctx->maxfd,
				ptimeout->tv_sec,
				ptimeout->tv_usec);
		else
			printf(	"fdmgr: error from select %d\n",
				errno);

		return labor_performed;
	}

	pfdentry = (fdentry *) &pfdctx->fdentry_list.node;
	while(TRUE){
		void		*pfunc;

		LOCK(pfdctx)
		pfdentry = (fdentry *) pfdentry->node.next;
		if(pfdentry){
			lstDelete(&pfdctx->fdentry_list, pfdentry);
			/*
			 *
			 * holding place where it can be marked 
			 * pending delete but not deleted
 			 *
			 */
			lstAdd(&pfdctx->fdentry_in_use_list, pfdentry);
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

     			(*pfdentry->pfunc)(pfdentry->param);
			labor_performed = TRUE;
		}

		LOCK(pfdctx)
		lstDelete(&pfdctx->fdentry_in_use_list, pfdentry);

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
			lstAdd(&pfdctx->fdentry_list, pfdentry);
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
static  void 
process_alarm_queue(pfdctx, poffset)
fdctx 		*pfdctx;
struct timeval	*poffset;
{
	struct timeval	t;
	int		status;
	alarm		*pa;
	alarm		*nextpa;

	status = fdmgr_gettimeval(pfdctx, &t);
	if(status < 0)
		abort();

	LOCK(pfdctx);
	for(pa = (alarm*)pfdctx->alarm_list.node.next; pa; pa = nextpa){
		if(pa->t.tv_sec > t.tv_sec)
			break;
		else if(pa->t.tv_sec == t.tv_sec)
			if(pa->t.tv_usec > t.tv_usec)
				break;

		nextpa = (alarm*)pa->node.next;
		lstDelete(&pfdctx->alarm_list, pa);
		lstAdd(&pfdctx->expired_alarm_list, pa);
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
	pa = (alarm*) pfdctx->expired_alarm_list.node.next;
	while(pa){
		(*pa->func)(pa->param);
		pa = (alarm*)pa->node.next;
	}

	LOCK(pfdctx);
	lstConcat(&pfdctx->free_alarm_list, &pfdctx->expired_alarm_list);

	pa = (alarm *)pfdctx->alarm_list.node.next;
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
static void 
select_alarm(pfdctx)
fdctx 	*pfdctx;
{
	pfdctx->select_tmo = TRUE;
}




/*
 *
 * fdmgr_gettimeval
 *
 *
 */
#ifdef UNIX
fdmgr_UNIX_gettimeval(pfdctx, pt)
fdctx 		*pfdctx;
struct timeval	*pt;
{
	struct timezone		tz;

	return gettimeofday(pt, &tz);
}
#endif


/*
 *
 * fdmgr_vxWorks_gettimeval
 *
 *
 */
#ifdef vxWorks
fdmgr_vxWorks_gettimeval(pfdctx, pt)
fdctx 		*pfdctx;
struct timeval	*pt;
{
	static unsigned	long	last;
	static unsigned	long	offset;
	unsigned		current;

	current = tickGet();
	if(current<last){
		offset += (~(unsigned long)0)/pfdctx->clk_rate;
	}
	last = current;

	pt->sec = current/pfdctx->clk_rate;
	pt->sec += offset;
	pt->usec = ((current%pfdctx->clk_rate)*
			USEC_PER_SEC)/pfdctx->clk_rate;

}
#endif
