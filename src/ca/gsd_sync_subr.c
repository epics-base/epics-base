/*
***************************************************
* File: /home/lando/slentz/sync/unix_gsd_sync.c
* Modifications:
*	09/18/90	ges	creation
*	09/21/90	ges	desk check
*	10/15/90	ges	add gsd_copy_buffer routine
*	10/17/90	ges	clean-up and add to documentation
*				add timeout parameter to gsd_sync_read
*	11/01/90	ges	add gsd_sync_clear function
*	11/05/90	ges	add linked list event data approach
*	11/16/90	ges	make special UNIX version for OPI
*	11/20/90	ges	make gsd_sync_init return VOID * for consistency
*				between UNIX and VxWorks versions
*	11/26/90	ges	use ca_build_and_connect in gsd_sync_init
*	11/27/90	ges	add NULL ptr return 0 to gsd_tss_eq
*				add NULL ptr return 0 to gsd_tss_gtr
*				change from (gsd_tss_gtr) to (gsd_tss_gtr ||
*				  gsd_tss_eq) in gsd_findlargest_ts
*	12/14/90	ges	add events at gsd_sync_read instead of gsd_sync_init
*				return most recent chan data if chan failed to
*				  produce data with the associated time stamp
*	09/03/91	joh	fixed includes for V5 vxWorks
*	06/26/92	joh	now uses ca_printf instead of printf	
*
*
* Compile:
*   for Unix: 
*	compile:
*		make
*	execute:
*		unix_gsd_sync
*-------------------------------------------------
* Usage:
*	1) A single initial call to gsd_sync_init must be made before
*	   any additional calls to gsd_sync_read".
*	2) Calls to UNIX version of "gsd_sync_read":
*		1) returns 1 if done, either timed out or has valid 
*		   	synchronous data on all channels of interest
*		   returns 0 if not done, allowing the calling program
*			to continue polling for done by repeated
*			calls to gsd_sync_read
*		2) with NEXTSET_SYNC_DATA: Pushes any currently saved valid
*		   event data down to the "previous" level store.
*		   At invocation, time stamp data monitors are established
*		   for each channel named.
*		   All event data is then received and stored in sequence
*		   until either:
*			1) event data containg identical time stamps has been
*			   received from all channels named (this time stamp
*			   is selected to be associated with this sync
*			   data call) or
*			2) time out occurs (in this case the most recent
*			   time stamp of all the collected event data
*			   is selected as the time stamp associated with
*			   this sync data call)
*		   Data that has the associated time stamp
*		   will be marked as valid and will be copied to the
*		   synchronous data buffers.  If a channel has no data with
*		   the associated time stamp, then its most recent received
*		   data will be copied to the synchronous data buffers and
*		   will be marked not valid.
*		3) with PREVIOUS_SYNC_DATA: yields the collected and
*		   saved data that currently reside in the "previous"
*		   level store.
*	3) Interpretation of flags:
*		1) gsd_sync_data.svalid: is 1 if new sync event data has come in
*		   over that channel during its synchronous read window.
*		   The pointer gsd_sync_data.pSdata points to the
*	    	   DBR_TIME_XXXX structure containing this new data.
*				       :  is 0 if no new synchronous
*						event data has come in
*		   over that channel during its synchronous read window.
*		   The pointer gsd_sync_data.pSdata points the the
*    		   DBR_TIME_XXXX structure and will contain the remaining
*		   old residual data unchanged from the most recent
*		   successful synchronous event.
*		2) gsd_sync_data.pSdata:  is 0 (i.e. NULL) if that channel
*		   has never established network connection.  If NULL then
*		   no DBR_TIME_XXXX data is yet available. 
***************************************************
*/

static char *sccsId = "$Id$";

#if defined(UNIX)
#	include	<sys/types.h>
#	include	<stdio.h>
#	include	<sys/time.h>
#	include	<time.h>
#else
#  if defined(vxWorks)
#	define abort(A) 	taskSuspend(taskIdSelf())
#    ifdef V5vxWorks
#	include	<Vxtypes.h>
#    else
#	include	<types.h>
#    endif
#	if 0 /* needed ?? */
#		include	<stdioLib.h>
#		include	<sys/time.h>
#		include	<time.h>
#	endif
#  else
	@@@@ dont compile in this case @@@@
#  endif
#endif

#include	<cadef.h>
#include	<gsd_sync_defs.h>



/*
*--------------------------------------------
* Function prototypes
*--------------------------------------------
*/
VOID gsd_fd_register();
VOID gsd_ca_service();

VOID gsd_ca_task_initialize();
VOID gsd_ca_pend_event();
VOID *gsd_sync_init();
VOID gsd_connevent_handler();
VOID gsd_event_handler();
int gsd_sync_read();
TS_STAMP *gsd_complete_set();
VOID gsd_copy_buffer();
int gsd_timeout();
VOID gsd_sync_clear();
int gsd_tss_within_delta();
int gsd_tss_eq();
int gsd_tss_gtr();
TS_STAMP *gsd_findlargest_ts();
VOID gsd_freeevent_mem();
VOID gsd_syncset_findcopy();
TS_STAMP *gsd_get_ts();


/*-------------------------------------------*/

struct gsd_sync_linked {
	struct gsd_sync_linked *pNstruct; /* ptr to next gsd_sync_linked struct */
	VOID *pNdata;	/* ptr to event TBR_TIME_xxxx data */
};

struct gsd_sync_ctrl {
	struct gsd_sync_data *pDstruct;	/* ptr to assoc gsd_sync_data struc */
	int nvalid;	/* next data's valid flag */
	int avalid;	/* already data's valid flag */
	VOID *pNdata;	/* ptr to next data set */
	VOID *pAdata;	/* ptr to immediately previous (already) data set */
	struct gsd_sync_linked *pNstruct; /* ptr to head linked data sets */
	int en_event;	/* enable the storage of event data */
	evid event_id;	/* event id storage for this channel */
};

struct gsd_sync_compctrl {
	struct timeval start_time; /* sys time store at initial call */
	struct gsd_sync_ctrl *pCtrl; /* ptr to gsd_sync_ctrl array */
};

/*---------------------------------------------*/
#define	ONESEC_IN_TICKS (sysClkRateGet())	
#define	PEND_EVENT_DELAY	0.0001	/* standard 0.0001sec event delay */
#define	USEC_TIME_OUT	100		/* 100 usec timeval's timeout */




/* test only !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*
*------------------------------------------------------
* For stand-alone operation remove the comments that bracket this
*  mainline driver and call:
* 	main()
*------------------------------------------------------
*/
#define	CHAN_COUNT	4	/* 4 channels for this example test code */
#define SYNC_TIMEOUT	1.0	/* timeout for data reception in float secs */

struct gsd_sync_data sync_data_array[CHAN_COUNT];
struct gsd_sync_data *pSync_data_array = &sync_data_array[0];
/* 
main()
{
	VOID *pfdctx = NULL;
	struct gsd_sync_compctrl *pSync_compctrl;
	struct gsd_sync_ctrl *pSync_ctrl;
	int i;

	sync_data_array[0].pName = "IOC21:CALC:S00:00";
	sync_data_array[1].pName = "IOC21:CALC:S00:01";
	sync_data_array[2].pName = "BSD_41N:00:00";
	sync_data_array[3].pName = "BSD_21:00:00";

	SEVCHK(ca_task_initialize(),
		"main: C.A. initialize failure.\n");
	pSync_compctrl = (struct gsd_sync_compctrl *) 
		gsd_sync_init(pSync_data_array, CHAN_COUNT, &pfdctx, NULL);
	if(NULL == pSync_compctrl) {
		ca_printf("unable to continue, gsd_sync_init failed\n");
		return 1;
	}
	pSync_ctrl = pSync_compctrl->pCtrl;


	gsd_sync_read(PREVIOUS_SYNC_DATA, pSync_compctrl, CHAN_COUNT, &pfdctx,
		SYNC_TIMEOUT);
	ca_printf("result of sync_read(PREVIOUS_SYNC_DATA)\n");
	for (i = 0; i < CHAN_COUNT; ++i) {
		ca_printf("%s  valid = %d  ptr = %d  \n",
			(pSync_ctrl->pDstruct)->pName, (pSync_ctrl->pDstruct)->svalid,
			(pSync_ctrl->pDstruct)->pSdata);
		if((pSync_ctrl->pDstruct)->pSdata) {
		  ca_printf("ts sec = %ld  ts nsec = %ld  value = %f\n",
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.secPastEpoch,
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.nsec,
			((struct dbr_time_float*)
			 ((pSync_ctrl->pDstruct)->pSdata))->value);
		}
		++pSync_ctrl;
	}
	pSync_ctrl = pSync_compctrl->pCtrl;
	ca_printf("\n");


	while(!gsd_sync_read(NEXTSET_SYNC_DATA, pSync_compctrl, CHAN_COUNT, &pfdctx,
		SYNC_TIMEOUT)) {
		;
	}
	ca_printf("result of sync_read(NEXTSET_SYNC_DATA)\n");
	for (i = 0; i < CHAN_COUNT; ++i) {
		ca_printf("%s  valid = %d  ptr = %d  \n",
			(pSync_ctrl->pDstruct)->pName, (pSync_ctrl->pDstruct)->svalid,
			(pSync_ctrl->pDstruct)->pSdata);
		if((pSync_ctrl->pDstruct)->pSdata) {
		  ca_printf("ts sec = %ld  ts nsec = %ld  value = %f\n",
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.secPastEpoch,
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.nsec,
			((struct dbr_time_float*)
			 ((pSync_ctrl->pDstruct)->pSdata))->value);
		}
		++pSync_ctrl;
	}
	pSync_ctrl = pSync_compctrl->pCtrl;
	ca_printf("\n");


	gsd_sync_read(PREVIOUS_SYNC_DATA, pSync_compctrl, CHAN_COUNT, &pfdctx,
		SYNC_TIMEOUT);
	ca_printf("result of sync_read(PREVIOUS_SYNC_DATA)\n");
	for (i = 0; i < CHAN_COUNT; ++i) {
		ca_printf("%s  valid = %d  ptr = %d  \n",
			(pSync_ctrl->pDstruct)->pName, (pSync_ctrl->pDstruct)->svalid,
			(pSync_ctrl->pDstruct)->pSdata);
		if((pSync_ctrl->pDstruct)->pSdata) {
		  ca_printf("ts sec = %ld  ts nsec = %ld  value = %f\n",
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.secPastEpoch,
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.nsec,
			((struct dbr_time_float*)
			 ((pSync_ctrl->pDstruct)->pSdata))->value);
		}
		++pSync_ctrl;
	}
	pSync_ctrl = pSync_compctrl->pCtrl;
	ca_printf("\n");


	while(!gsd_sync_read(NEXTSET_SYNC_DATA, pSync_compctrl, CHAN_COUNT, &pfdctx,
		SYNC_TIMEOUT)) {
		;
	}
	ca_printf("result of sync_read(NEXTSET_SYNC_DATA)\n");
	for (i = 0; i < CHAN_COUNT; ++i) {
		ca_printf("%s  valid = %d  ptr = %d  \n",
			(pSync_ctrl->pDstruct)->pName, (pSync_ctrl->pDstruct)->svalid,
			(pSync_ctrl->pDstruct)->pSdata);
		if((pSync_ctrl->pDstruct)->pSdata) {
		  ca_printf("ts sec = %ld  ts nsec = %ld  value = %f\n",
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.secPastEpoch,
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.nsec,
			((struct dbr_time_float*)
			 ((pSync_ctrl->pDstruct)->pSdata))->value);
		}
		++pSync_ctrl;
	}
	pSync_ctrl = pSync_compctrl->pCtrl;
	ca_printf("\n");


	gsd_sync_read(PREVIOUS_SYNC_DATA, pSync_compctrl, CHAN_COUNT, &pfdctx,
		SYNC_TIMEOUT);
	ca_printf("result of sync_read(PREVIOUS_SYNC_DATA)\n");
	for (i = 0; i < CHAN_COUNT; ++i) {
		ca_printf("%s  valid = %d  ptr = %d  \n",
			(pSync_ctrl->pDstruct)->pName, (pSync_ctrl->pDstruct)->svalid,
			(pSync_ctrl->pDstruct)->pSdata);
		if((pSync_ctrl->pDstruct)->pSdata) {
		  ca_printf("ts sec = %ld  ts nsec = %ld  value = %f\n",
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.secPastEpoch,
			((struct dbr_time_float *)
			 ((pSync_ctrl->pDstruct)->pSdata))->stamp.nsec,
			((struct dbr_time_float*)
			 ((pSync_ctrl->pDstruct)->pSdata))->value);
		}
		++pSync_ctrl;
	}
	pSync_ctrl = pSync_compctrl->pCtrl;
	ca_printf("\n");


	gsd_sync_clear(pSync_compctrl, CHAN_COUNT);
	ca_printf("End of test and demo.\n\n");

	return 0;
}
*/
/* end test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

/*
*******************************************************
* Description:
*	1) Entry with pSync containing the base address of the array of
*	   gsd_sync_data structures (chan_count of these)
*	2) Alloc memory for a composite control structure instance and
*	   an array of gsd_sync_ctrl structures,
*	   one for each gsd_sync_data structure
*	3) Establish a connection event handler
*	4) For each gsd_sync_ctrl structure
*		1) init pCtrl->Dstruct to point to its associated
*	           gsd_sync_data structure
*		2) init to NULL the pSdata ptr in associated pSync
*		   (this is used as a flag to indicate if channel has
*		   ever been up in function "gsd_connevent_handler")
*		3) broadcast chan access search
*	5) Flush i/o for channel access
*	6) Returns pCompctrl, the address of the composite control struct
*	   which must be used for any subsequent gsd_sync_read(flag,pCtrl,
*	   chan_count) calls
*		pCompctrl == NULL: error, unable to allocate memory.
*		pCompctrl != NULL: success, normal execution.
*******************************************************
*/


VOID *gsd_sync_init(pSync, chan_count, pfdctx, pMac_pairs)
struct gsd_sync_data *pSync;
unsigned int chan_count;
VOID **pfdctx;
char  *pMac_pairs[];
{
	int i;
	struct gsd_sync_compctrl *pCompctrl;
	struct gsd_sync_ctrl *pCtrl;

	if(*pfdctx == NULL) {
		gsd_ca_task_initialize(pfdctx);
	}

	pCompctrl = (struct gsd_sync_compctrl *)
		calloc(1, sizeof(struct gsd_sync_compctrl));
	if(pCompctrl == NULL) {
		ca_printf("gsd_sync_init: mem alloc failed\n");
		return NULL;
	}

	pCtrl = (struct gsd_sync_ctrl *) calloc(chan_count, sizeof(struct gsd_sync_ctrl));
	if(pCtrl == NULL) {
		ca_printf("gsd_sync_init: mem alloc failed\n");
		free(pCompctrl);
		return NULL;
	}
	pCompctrl->pCtrl = pCtrl;

	for(i = 0; i < chan_count; i++) {
		pCtrl->pDstruct = pSync;
		pSync->pSdata = NULL;
		if(strlen(pSync->pName) > 0){
		    char  name_str[db_name_dim];
		    if(pMac_pairs != NULL){
		    	gsdl_main_macro(name_str, pSync->pName, pMac_pairs, db_name_dim);
		    	strcpy(pSync->pName, name_str);
		    }
/*		    ca_printf("new_str = %s\n", pSync->pName);*/
		    SEVCHK(ca_build_and_connect(pSync->pName, TYPENOTCONN, 0,
			&(pSync->pChid), (VOID *) NULL, 
			gsd_connevent_handler, (VOID *) pCtrl),
			"gs_sync_init: broadcast search failed\n");
		    ca_flush_io();
		}
		++pSync;
		++pCtrl;
	}

	gsd_ca_pend_event(pfdctx);
	return (VOID *) pCompctrl;
}
/*
****************************************************
* Description:
*	1) If first time channel connect has been up (i.e. no valid
*	   memory address for pSdata) then
*		1) Allocate memory for the sync data and fill pSdata
*		   with its address. If fail, return. (no monitor started)
*		2) Allocate memory for the next data.  If fail, 
*		   free memory gotten so far, and return.
*		   (no monitor started)
*		3) Allocate memory for the already data.  If fail, 
*		   free memory gotten so far, and return.
*		   (no monitor started)
*		4) Start event monitor on channel
*		5) Flush i/o for channel access
*****************************************************
*/
VOID gsd_connevent_handler(args)
struct connection_handler_args args;
{
	struct gsd_sync_ctrl *pCtrl = ca_puser(args.chid);
	struct gsd_sync_data *pSync = pCtrl->pDstruct;

	if(args.op == CA_OP_CONN_UP) {
		if(pSync->pSdata == NULL) {

			pSync->pSdata = (VOID *) calloc(1, dbr_size_n(
				dbf_type_to_DBR_TIME(ca_field_type(args.chid)),
				ca_element_count(args.chid)));
			if(pSync->pSdata == NULL) {
				ca_printf("gsd_connevent_handler: mem alloc fail\n");
				return;
			}
			pCtrl->pNdata = (VOID *) calloc(1, dbr_size_n(
				dbf_type_to_DBR_TIME(ca_field_type(args.chid)),
				ca_element_count(args.chid)));
			if(pCtrl->pNdata == NULL) {
				free(pSync->pSdata);
				pSync->pSdata = NULL;
				ca_printf("gsd_connevent_handler: mem alloc fail\n");
				return;
			}
			pCtrl->pAdata = (VOID *) calloc(1, dbr_size_n(
				dbf_type_to_DBR_TIME(ca_field_type(args.chid)),
				ca_element_count(args.chid)));
			if(pCtrl->pAdata == NULL) {
				free(pSync->pSdata);
				pSync->pSdata = NULL;
				free(pCtrl->pNdata);
				pCtrl->pNdata = NULL;
				ca_printf("gsd_connevent_handler: mem alloc fail\n");
				return;
			}

			pSync->time_type = dbf_type_to_DBR_TIME(
				ca_field_type(pSync->pChid));
			pSync->count = ca_element_count(pSync->pChid);

/*
			ca_printf("%s  type = %d  count = %d\n",
				pSync->pName, pSync->time_type, pSync->count);
*/
		}
	}
	return;
}
/*
***********************************************************
* Description:
*	1) if (event is enabled) then
*		1) if (event is the 1st for this channel (pN == NULL)) then
*			1) alloc mem for a gsd_sync_linked structure and
*			   place its addr into the pN ptr
*		   else
*			1) traverse linked list until pN points to last
*			   gsd_sync_linked structure
*			2) allocate memory for a new gsd_sync_linked
*			   structure and append it to end of linked list
*			3) Update pN to point to this new last structure
*		2) allocate memory to hold the data
*		3) copy the data into this buffer
***********************************************************
*/
VOID gsd_event_handler(args)
struct event_handler_args args;
{
	struct gsd_sync_ctrl *pCtrl = args.usr;
	struct gsd_sync_linked *pN = pCtrl->pNstruct;
	struct gsd_sync_linked *pAlloc_linked;
	VOID *pAlloc_data;

	if(pCtrl->en_event) {
		pAlloc_linked = (struct gsd_sync_linked *) calloc(1,
			sizeof(struct gsd_sync_linked));
		pAlloc_data = (VOID *) calloc(1, dbr_size_n(args.type,args.count));
		if((NULL == pAlloc_linked) || (NULL == pAlloc_data)) {
			ca_printf("gsd_event_handler: mem alloc failed.\n");
			/* clean-up unused memory */
			if(pAlloc_linked != NULL) free(pAlloc_linked);
			if(pAlloc_data != NULL) free(pAlloc_data);
			return;
		}

		if(pN == NULL) {
			pCtrl->pNstruct = pAlloc_linked;
			pN = pAlloc_linked;
		}
		else {
			while(pN->pNstruct) pN = pN->pNstruct;
			pN->pNstruct = pAlloc_linked;
			pN = pN->pNstruct;
		}
	
		pN->pNdata = pAlloc_data;

		gsd_copy_buffer(args.dbr, pN->pNdata, dbr_size_n(args.type,
			args.count));

/*
		ca_printf("%s  ts sec = %ld  ts nsec = %ld  value = %f\n",
			(pCtrl->pDstruct)->pName,
			((struct dbr_time_float *) (pN->pNdata))->stamp.secPastEpoch,
			((struct dbr_time_float *) (pN->pNdata))->stamp.nsec,
			((struct dbr_time_float *) (pN->pNdata))->value);
*/

	}

	return;
}

/*
*****************************************************
* Description:
*	1) if (PREVIOUS_SYNC_DATA) then
*		1) for each channel
*			1) if channel has ever been up then
*				1) copy already data to sync data
*				2) copy already valid flag to sync valid flag
*			   else
*				1) place NULL in sync data ptr
*				2) clear sync valid flag to zero
*	2) if (NEXTSET_SYNC_DATA) then
*		1) if first call indicated by 0 start time then
*			1) for each channel
*				1) if channel has ever been up then
*					1) copy old next data to already level data
*	       		  		   copy old next valid flags to already 
*					   level valid flags
*					2) clear next valid flags
*					3) enable capture of monitor data until
*			  		   a full set is acquired or timeout
*					4) add monitor event
*			2) fill with system time
*			3) return 0 indicating not done
*		2) if complete set received or timed out then
*			1) clear start time to 0 to prepare for
*			   possible next call
*			2) for each channel
*				1) disable further capture of data
*				2) clear monitor event
*			3) copy chosen time stamp data to next level
*			   (if no data received, don't over copy to
*			   next level, but clear valid flag on next levl
*			4) for each channel
*				1) if channel has ever been up then
*					1) copy next level data to sync data
*					2) copy next level valid flag to 
*					   sync valid flag
*					3) free memory of linked list and 
*					   its associated data buffer
*				   else
*					1) place NULL in sync data ptr
*					2) set sync valid flag to zero
*			5) return 1 indicating done
*		3) return 0 indicating not done
*	3) return 1 indicating done with unknown request
*****************************************************
*/


int gsd_sync_read(flag, pCompctrl,chan_count, pfdctx, timeout_secs)
char flag;
struct gsd_sync_compctrl *pCompctrl;
unsigned int chan_count;
VOID *pfdctx;
float timeout_secs;
{
	struct gsd_sync_ctrl *pCtrl = pCompctrl->pCtrl;
	TS_STAMP *pTs;
	struct timezone zone;
	int i;

	if(NULL == pCompctrl) {
		ca_printf("gsd_sync_read: error NULL arg for pCompctrl\n");
		return 0;
	}

	if(flag == PREVIOUS_SYNC_DATA) {
		for (i = 0; i < chan_count; i++) {
			if(pCtrl->pAdata != NULL) {
				gsd_copy_buffer(pCtrl->pAdata,
				  (pCtrl->pDstruct)->pSdata,
				  dbr_size_n((pCtrl->pDstruct)->time_type,
					(pCtrl->pDstruct)->count));
				(pCtrl->pDstruct)->svalid = pCtrl->avalid;
			}
			else {
				(pCtrl->pDstruct)->pSdata = NULL;
				(pCtrl->pDstruct)->svalid = 0;
			}
			++pCtrl;
		}
		return 1; /* done with PREVIOUS_SYNC_DATA request */
	}
	
	if(flag == NEXTSET_SYNC_DATA) {
		if(((pCompctrl->start_time).tv_sec == 0) &&
		   ((pCompctrl->start_time).tv_usec == 0)) {
			for (i = 0; i < chan_count; i++) {
				if(pCtrl->pNdata != NULL) {
					gsd_copy_buffer(pCtrl->pNdata,
					  pCtrl->pAdata,
					  dbr_size_n((pCtrl->pDstruct)->time_type,
						(pCtrl->pDstruct)->count));
					pCtrl->avalid = pCtrl->nvalid;
					pCtrl->nvalid = 0;
					pCtrl->en_event = 1;

					SEVCHK(ca_add_array_event(
						(pCtrl->pDstruct)->time_type,
						(pCtrl->pDstruct)->count,
						(pCtrl->pDstruct)->pChid, gsd_event_handler,
						pCtrl, (float) 0, (float) 0, (float) 0, 
						&(pCtrl->event_id)),
						"gsd_connevent_handler: add event failed\n");
				}
				++pCtrl;
			}
			ca_flush_io();
			gettimeofday(&(pCompctrl->start_time), &zone);
			return 0;  /* not done */
		}

		if((pTs = gsd_complete_set(pCompctrl->pCtrl, chan_count)) ||
		   (gsd_timeout(pCompctrl, timeout_secs, pfdctx))) {

			(pCompctrl->start_time).tv_sec = 0;
			(pCompctrl->start_time).tv_usec = 0;

			pCtrl = pCompctrl->pCtrl;
			for (i = 0; i < chan_count; ++i) {
				pCtrl->en_event = 0;
				if(pCtrl->event_id) ca_clear_event(pCtrl->event_id);
				++pCtrl;
			}
			ca_flush_io();

			pCtrl = pCompctrl->pCtrl;

			if(pTs != NULL) {
/*
				ca_printf("got sync data set\n");
*/
				gsd_syncset_findcopy(pCtrl,
					chan_count, pTs);
			}
			else {
/*
				ca_printf("settle for last \n");
*/
				pTs = gsd_findlargest_ts(pCtrl,
					chan_count);
				gsd_syncset_findcopy(pCtrl, chan_count,
					pTs);
			}

			for(i = 0; i < chan_count; i++) {
				if(pCtrl->pNdata != NULL) {
					gsd_copy_buffer(pCtrl->pNdata,
					  (pCtrl->pDstruct)->pSdata,
			 		 dbr_size_n((pCtrl->pDstruct)->time_type,
						(pCtrl->pDstruct)->count));
					(pCtrl->pDstruct)->svalid = pCtrl->nvalid;
					gsd_freeevent_mem(pCtrl);
				}
				else {
					(pCtrl->pDstruct)->pSdata = NULL;
					(pCtrl->pDstruct)->svalid = 0;
				}
				++pCtrl;
			}
			return 1;  /* done with NEXT_SYNC_DATA request, */
					/* either fullset or timeout */
		}

		return 0;  /* not done with NEXT_SYNC_DATA request */
	}
	return 1;  /* done, unknown request */
}
/*
************************************************
* Description:
*	1) If a set of identically time stamped data is found in
*	   each channel of interest then
*		1) return a ptr to one of the identical time stamps
*	2) Otherwise return NULL ptr
************************************************
*/
TS_STAMP *gsd_complete_set(pCtrl, chan_count)
struct gsd_sync_ctrl *pCtrl;
unsigned int chan_count;
{
	struct gsd_sync_linked *pNfirst_chan = pCtrl->pNstruct;
	struct gsd_sync_linked *pNchan = NULL;
	TS_STAMP *pEqual = NULL;

	int i;
	struct gsd_sync_ctrl *pCtrl_copy;

	while(pNfirst_chan != NULL) {	/* for every data rec receiv in 1st chn*/
		pCtrl_copy = pCtrl;
		for(i = 0; i < chan_count; ++i) { /* for every channel in chan array */
			pEqual = NULL;

			pNchan = pCtrl_copy->pNstruct;
			while(pNchan != NULL) {
			/* for every data record received by this channel */
			/* may want to use gsd_tss_within_delta instead of eq */
				if(gsd_tss_eq(
					gsd_get_ts((pCtrl->pDstruct)->time_type,
						pNfirst_chan->pNdata),
					gsd_get_ts((pCtrl_copy->pDstruct)->time_type,
						pNchan->pNdata))) {
					pEqual = gsd_get_ts(
						(pCtrl->pDstruct)->time_type, 
						pNfirst_chan->pNdata);
					break;
				}
				pNchan = pNchan->pNstruct;
			}
			if(NULL == pEqual) break;
			/* no ts match in this chan, exit the for, */
			/* and try next 1st channel time stamp */
			pCtrl_copy++;
		}
		if(pEqual != NULL) return pEqual; /* success */
		pNfirst_chan = pNfirst_chan->pNstruct; 
		/* try next data record of 1st channel */
	}
	return NULL; /* failed, returning NULL ptr */
}
/*
***************************
* Description:
*	1) returns ptr to the time stamp structure within the data buffer of
*	   types DBR_TIME_xxxx  (an accessor routine)
******************************
*/
TS_STAMP *gsd_get_ts(time_type, pNdata)
int time_type;
void *pNdata;
{
	switch(time_type) {
		case DBR_TIME_STRING:
			return &(((struct dbr_time_string *) pNdata)->stamp);
		case DBR_TIME_SHORT:
			return &(((struct dbr_time_short *) pNdata)->stamp);
		case DBR_TIME_FLOAT:
			return &(((struct dbr_time_float *) pNdata)->stamp);
		case DBR_TIME_ENUM:
			return &(((struct dbr_time_enum *) pNdata)->stamp);
		case DBR_TIME_CHAR:
			return &(((struct dbr_time_char *) pNdata)->stamp);
		case DBR_TIME_LONG:
			return &(((struct dbr_time_long *) pNdata)->stamp);
		case DBR_TIME_DOUBLE:
			return &(((struct dbr_time_double *) pNdata)->stamp);
		default:
			return NULL;
	}
}

/*
************************************************
* Description:
*	1) returns ptr to the largest time stamp found in
*	   all of the channel data collected. If no data then
*	   returns NULL.
************************************************
*/
TS_STAMP *gsd_findlargest_ts(pCtrl, chan_count)
struct gsd_sync_ctrl *pCtrl;
int chan_count;
{
	TS_STAMP ts;
	TS_STAMP *pTs = &ts;
	struct gsd_sync_linked *pNchan;
	int i;
	
	ts.secPastEpoch = 0;
	ts.nsec = 0;
	for (i = 0; i < chan_count; ++i) {
		pNchan = pCtrl->pNstruct;
		while(pNchan != NULL) {
/*
			ca_printf(" %ld %ld >? %ld %ld\n",
				(gsd_get_ts((pCtrl->pDstruct)->time_type,
					pNchan->pNdata))->secPastEpoch,
				(gsd_get_ts((pCtrl->pDstruct)->time_type,
					pNchan->pNdata))->nsec,
				pTs->secPastEpoch, pTs->nsec);
*/
			if(gsd_tss_gtr(
				gsd_get_ts((pCtrl->pDstruct)->time_type,pNchan->pNdata),
				pTs) ||
				gsd_tss_eq(
				gsd_get_ts((pCtrl->pDstruct)->time_type,
					pNchan->pNdata), pTs)) {
				pTs = gsd_get_ts((pCtrl->pDstruct)->time_type,pNchan->pNdata);
			}
			pNchan = pNchan->pNstruct;
		}
		pCtrl++;
	}
	if(pTs == &ts) {
		return NULL;
	}
	else {
		return pTs;
	}
}
/*
*******************************************
* Description:
*	1) Frees all event memory that was allocated on the fly by
*	   following each channel's linked list of gsd_sync_linked structures
*******************************************
*/
VOID gsd_freeevent_mem(pCtrl)
struct gsd_sync_ctrl *pCtrl;
{
	struct gsd_sync_linked *pN;
	struct gsd_sync_linked *pN_copy;
	
	pN = pCtrl->pNstruct;
	
	while(pN != NULL) {
		if(pN->pNdata != NULL) {
/*
			ca_printf("free %s  %ld  %ld\n", (pCtrl->pDstruct)->pName,
				((struct dbr_time_float *) (pN->pNdata))->stamp.secPastEpoch,
				((struct dbr_time_float *) (pN->pNdata))->stamp.nsec);
*/
			free(pN->pNdata);
		}
		pN_copy = pN;
		pN = pN->pNstruct;
		free(pN_copy);
	}
	pCtrl->pNstruct = NULL;
	return;
}

/*
*******************************************
* Description:
*	1) for every channel
*		1) If finds linked list data that is time stamped the
*		   same as pTs.
*			1) copies this data to the Next data buffer of 
*			   the gsd_sync_ctrl structure
*			2) sets nvalid = 1
*		   else
*			1) copies most recently received data to the
*			   Next data buffer of the gsd_sync_ctrl structure
*			2) clears nvalid = 0
*******************************************
*/
VOID gsd_syncset_findcopy(pCtrl, chan_count, pTs)
struct gsd_sync_ctrl *pCtrl;
int chan_count;
TS_STAMP *pTs;
{
	struct gsd_sync_linked *pNchan;
	int i;

	for (i = 0; i < chan_count; ++i) {
		pCtrl->nvalid = 0;
		pNchan = pCtrl->pNstruct;
		while(pNchan != NULL) {
			if(gsd_tss_eq(gsd_get_ts((pCtrl->pDstruct)->time_type,
				pNchan->pNdata), pTs)) {
				gsd_copy_buffer(pNchan->pNdata, pCtrl->pNdata,
					dbr_size_n((pCtrl->pDstruct)->time_type,
						(pCtrl->pDstruct)->count));
				pCtrl->nvalid = 1;
				break;
			}
			pNchan = pNchan->pNstruct;
		}

		if((pCtrl->nvalid) != 1) {
			if(pNchan = pCtrl->pNstruct) {
				while(pNchan->pNstruct) pNchan = pNchan->pNstruct;
				gsd_copy_buffer(pNchan->pNdata, pCtrl->pNdata,
					dbr_size_n((pCtrl->pDstruct)->time_type,
						(pCtrl->pDstruct)->count));
			}
		}

		pCtrl++;
	}
	return;
}

/*
*************************************************
* Description:
*	1) byte block transfer
*************************************************
*/
VOID gsd_copy_buffer(pFrom, pTo, byte_count)
char *pFrom;
char *pTo;
unsigned int byte_count;
{
	unsigned int i;
	for(i = 0; i < byte_count; ++i) {
		*pTo++ = *pFrom++;
	}
	return;
}
/*
**********************************************
* Description:
*	1) Frees all allocated memory which resulted from calls to
*	   gsd_sync_init
*	   gsd_sync_read
**********************************************
*/
VOID gsd_sync_clear(pSync_compctrl, chan_count)
struct gsd_sync_compctrl *pSync_compctrl;
unsigned int chan_count;
{
	int i;
	struct gsd_sync_ctrl *pSync_ctrl = pSync_compctrl->pCtrl;

	if(NULL == pSync_compctrl) {
		ca_printf("gsd_sync_clear:  error NULL pSync_compctrl\n");
		return;
	}
	if(NULL == pSync_ctrl) {
		free(pSync_compctrl);
		ca_printf("gsd_sync_clear:  error NULL pSync_ctrl\n");
		return;
	}

	for(i = 0; i < chan_count; ++i) {
		ca_clear_channel((pSync_ctrl->pDstruct)->pChid);
		ca_flush_io();

		if(pSync_ctrl->pNdata != NULL) free(pSync_ctrl->pNdata);
		if(pSync_ctrl->pAdata != NULL) free(pSync_ctrl->pAdata);
		if((pSync_ctrl->pDstruct)->pSdata != NULL)
			free((pSync_ctrl->pDstruct)->pSdata);
		(pSync_ctrl->pDstruct)->pSdata = NULL;

		pSync_ctrl++;
	}

	free(pSync_compctrl->pCtrl);
	free(pSync_compctrl);

	return;
}
/*
**********************************************
* Description:
*	1) if the two time stamps are within delta of each other then
*		1) return 1
*	2) otherwise return 0
**********************************************
*/
int gsd_tss_within_delta(pTs_s, pTs_l, pDelta)
TS_STAMP *pTs_s;
TS_STAMP *pTs_l;
TS_STAMP *pDelta;
{
	TS_STAMP temp;
	TS_STAMP *pTemp;

	if(gsd_tss_eq(pTs_s, pTs_l)) return 1;	/* equal, so within delta */
	/* ensure that *pTs_l is the larger time stamp */
	if(gsd_tss_gtr(pTs_s, pTs_l)) {
		pTemp = pTs_l;
		pTs_l = pTs_s;
		pTs_s = pTemp;
	}

	temp.secPastEpoch = pTs_s->secPastEpoch + pDelta->secPastEpoch;
	temp.nsec = pTs_s->nsec + pDelta->nsec;

	if((gsd_tss_gtr(&temp, pTs_l)) || (gsd_tss_eq(&temp, pTs_l)))
		return 1;	/* within delta */
	return 0;		/* not within delta */
}
/*
**********************************************
* Description:
*	1) if the two time stamps are equal then
*		1) return 1
*	2) otherwise return 0
**********************************************
*/
int gsd_tss_eq(pTs_s, pTs_l)
TS_STAMP *pTs_s;
TS_STAMP *pTs_l;
{
	if((pTs_s == NULL) || (pTs_l == NULL)) return 0; /* not equal ts */
	if((pTs_s->secPastEpoch == pTs_l->secPastEpoch) &&
	   (pTs_s->nsec == pTs_l->nsec)) return 1; /* equal time stamps */
	return 0;				/* not equal time stamps */
}
/*
**********************************************
* Description:
*	1) if time stamp 1 > time stamp 2 then
*		1) return 1
*	2) otherwise return 0
**********************************************
*/
int gsd_tss_gtr(pTs_l, pTs_s)
TS_STAMP *pTs_l;
TS_STAMP *pTs_s;
{
	if((pTs_l == NULL) || (pTs_s == NULL)) return 0; /* not gtr than */
	if(pTs_l->secPastEpoch > pTs_s->secPastEpoch) return 1; /* gtr than */
	if((pTs_l->secPastEpoch == pTs_s->secPastEpoch) &&
	   (pTs_l->nsec > pTs_s->nsec)) return 1;	/* greater than */
	return 0;					/* not greater than */
}



/*
* Note: UNIX specific routines below: !!!!!!!!!!!!!!!!!!!!!!!!!
*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/
/*
************************************************
* Description:
*	1) indicate when timeout occurs
* Note: UNIX gettimeofday apparently is the only C/system time
*	routine to yield resolution below 1 second, but it is
*	represented as being inaccurate in its usec element
*	(see man gettimeofday)
************************************************
*/
int gsd_timeout(pCompctrl, timeout_secs, pfdctx)
struct gsd_sync_compctrl *pCompctrl;
float timeout_secs;
VOID **pfdctx;
{
	struct timeval new_gmt;
	struct timezone zone;

	float dif_secs;

	gsd_ca_pend_event(pfdctx); /* allow C.A. events to occur */
				   /* in the Unix environment */

	gettimeofday(&new_gmt, &zone);

	if(new_gmt.tv_usec >= (pCompctrl->start_time).tv_usec) {
		/* no borrow */
		dif_secs = ((float) (new_gmt.tv_sec - (pCompctrl->start_time).tv_sec)) +
		  (((float) (new_gmt.tv_usec - (pCompctrl->start_time).tv_usec)) /
			1000000.0);
	}
	else {
		/* borrow */
		dif_secs = ((float) (new_gmt.tv_sec - (pCompctrl->start_time).tv_sec - 1)) +
		  (((float) (1000000 + new_gmt.tv_usec - (pCompctrl->start_time).tv_usec)) /
			1000000.0);
	}

	if(dif_secs > timeout_secs) {
		return 1;
	}


	return 0;
}
/*
**********************************************
**********************************************
*/
void gsd_ca_task_initialize(pfdctx)
void **pfdctx;
{
	*pfdctx = (void *) fdmgr_init();
	if(!(*pfdctx)) {
		ca_printf("gsd_ca_task_initialize: fdmgr_init failed.\n");
		abort();
	}
	SEVCHK(ca_add_fd_registration(gsd_fd_register, *pfdctx),
		"gsd_ca_task_initialize: Fd registration failed.\n");
	return;
}
/*
**********************************************
**********************************************
*/
void gsd_ca_pend_event(pfdctx)
void **pfdctx;
{
	static struct timeval timeout = {0, USEC_TIME_OUT};
	fdmgr_pend_event(*pfdctx, &timeout);
	return;
}


/*
************************************************
* NAME
*	gsd_fd_register(pfdcts, fd, condition)
* DESCRIPTION
*	1) if (condition is true) then
*		1) add file descriptor to fd manager
*	else
*		1) delete file descriptor from fd manager
************************************************
*/
void gsd_fd_register(pfdctx, fd, condition)
void **pfdctx;
int fd;
int condition;
{
	if(condition) {
		fdmgr_add_fd(pfdctx, fd, gsd_ca_service, NULL);
	}
	else {
		fdmgr_clear_fd(pfdctx, fd);
	}
}

/*
*************************************************
* NAME
*	gsd_ca_service()
* DESCRIPTION
*	1) call ca_pend_event to allow event handler execution
***************************************************
*/
void gsd_ca_service()
{
	ca_pend_event(PEND_EVENT_DELAY);
}

