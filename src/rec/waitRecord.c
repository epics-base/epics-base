/* recWait.c */   
/*
 *      Original Author: Ned Arnold 
 *      Date:            05-31-94
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 *
 * Modification Log:
 * -----------------
 * 1.01  05-31-94        nda    initial try            
 * 1.02  07-11-94    mrk/nda    added "process on input change" feature
 * 1.03  08-16-94    mrk/nda    continuing "process on input change" feature
 * 1.04  08-16-94        nda    record does not get notified when a SCAN 
 *                              related field changes,so for now we have to
 *                              always add Monitors
 * 1.05  08-18-94        nda    Starting with R3.11.6, dbGetField locks the
 *                              record before fetching the data. This can
 *                              cause deadlocks within a database. Change all
 *                              dbGetField() to dbGet()
 * 1.06  08-19-94        nda    added Output data option of VAL or DOL
 * 1.07  09-14-94        nda    corrected bug that caused SCAN_DISABLE to 
 *                              lock up the record forever
 * 1.08  02-01-95        nda    added VERS and ODLY (output execution delay)
 * 1.09  02-15-95        nda    added INxP to specify which inputs should
 *                              cause the record to process when in I/O INTR
 * 2.00  02-20-95        nda    added queuing to SCAN_IO_EVENT mode so no 
 *                              transitions of data would be missed.
 * 2.01  08-07-95        nda    Multiple records with DOLN's didn't work, 
 *                              added calloc for dola structure.
 * 3.00  08-28-95        nda    Significant rewrite to add Channel Access 
 *                              for dynamic links using recDynLink.c . All
 *                              inputs are now "monitored" via Channel Access.
 *                              Removed some "callbacks" because recDynLink
 *                              lib uses it's own task context.
 *                              INxV field is used to keep track of PV 
 *                              connection status: 0-PV_OK, 
 *                              1-NotConnected,  2-NO_PV
 * 3.01  10-03-95        nda    Also post monitors on .la, .lb, .lc etc
 *                              when new values are written
 *
 *
 */

#define VERSION 3.01



#include <vxWorks.h> 
#include <types.h>
#include <stdioLib.h>
#include <lstLib.h>
#include <string.h>
#include <math.h>
#include <tickLib.h>
#include <semLib.h>
#include <taskLib.h>
#include <wdLib.h>
#include <sysLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbScan.h"
#include "dbDefs.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "rngLib.h"
#include "recSup.h"
#include "special.h"
#include "callback.h"
#include "taskwd.h"
#include "postfix.h"

#include "choiceWait.h"
#define GEN_SIZE_OFFSET
#include "waitRecord.h"
#undef  GEN_SIZE_OFFSET
#include "recDynLink.h"


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL 
#define put_array_info NULL 
#define get_units NULL 
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL 
#define put_enum_str NULL
static long get_graphic_double();
#define get_control_double NULL 
static long get_alarm_double(); 
 
struct rset waitRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double
};

/* Create DSET for "soft channel" to allow for IO Event (this is to implement
   the feature of processing the record when an input changes) */

static long get_ioint_info();
struct {
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init_dev;
        DEVSUPFUN       dev_init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_event;
}devWaitIoEvent={
        5,
        NULL,
        NULL,
        NULL,
        get_ioint_info,
        NULL};


/* DEFINES */
#define   ARG_MAX   12  /* Number of input arguments of the record */
#define   IN_PVS     1  /* Number of other input dynamic links(DOLN) */
#define   OUT_PVS    1  /* Number of "non-input" dynamic links(OUTN) */
#define   DOL_INDEX  ARG_MAX 
#define   OUT_INDEX  (ARG_MAX + IN_PVS)
#define   NUM_LINKS  (ARG_MAX + IN_PVS + OUT_PVS)
#define   PVN_SIZE  40  /*must match the length defined in waitRecord.db*/
#define   Q_SIZE    50
#define   PV_OK     REC_WAIT_DYNL_OK     /* from choiceWait.h */
#define   PV_NC     REC_WAIT_DYNL_NC     /* from choiceWait.h */
#define   NO_PV     REC_WAIT_DYNL_NO_PV  /* from choiceWait.h */

/**********************************************
  Declare constants and structures 
***********************************************/

/* callback structures and record private data */
struct cbStruct {
    CALLBACK           doOutCb;   /* cback struct for doing the OUT link*/
    CALLBACK           ioProcCb;  /* cback struct for io_event scanning */
    struct waitRecord *pwait;     /* pointer to wait record */
    WDOG_ID            wd_id;     /* Watchdog used for delays */
    recDynLink         caLinkStruct[NUM_LINKS]; /* req'd for recDynLink*/
    RING_ID            monitorQ;  /* queue to store ca callback data */
    IOSCANPVT          ioscanpvt; /* used for IO_EVENT scanning */
    int                outputWait;/* waiting to do output */
    int                procPending;/*record processing is pending */
    unsigned long      tickStart; /* used for timing  */
};


typedef struct recDynLinkPvt {
        struct waitRecord *pwait;     /* pointer to wait record */
        unsigned short     linkIndex; /* specifies which dynamic link */
}recDynLinkPvt;


static long get_ioint_info(cmd,pwait,ppvt)
    int                     cmd;
    struct waitRecord       *pwait;
    IOSCANPVT               *ppvt;
{
    *ppvt = (((struct cbStruct *)pwait->cbst)->ioscanpvt);
    return(0);
}


/* This is the data that will be put on the work queue ring buffer */
struct qStruct {
    char               inputIndex;
    double             monData;
};


int    recWaitDebug=0;
int    recWaitCacheMode=0;
static void schedOutput(struct waitRecord *pwait);
static void execOutput(struct cbStruct *pcbst);
static int fetch_values(struct waitRecord *pwait);
static void monitor(struct waitRecord *pwait);
static long initSiml();
static void ioIntProcess(CALLBACK *pioProcCb);

static void pvSearchCallback(recDynLink *precDynLink);
static void inputChanged(recDynLink *precDynLink);


static long init_record(pwait,pass)
    struct waitRecord	*pwait;
    int pass;
{
    struct cbStruct *pcbst;
    long    status = 0;
    int i;

    char            *ppvn[PVN_SIZE];
    unsigned short  *pPvStat;

    recDynLinkPvt   *puserPvt;

    short error_number;

    if (pass==0) {
      pwait->vers = VERSION;
 
      pwait->cbst = calloc(1,sizeof(struct cbStruct));

      /* init the private area of the caLinkStruct's   */
      for(i=0;i<NUM_LINKS; i++) {
        ((struct cbStruct *)pwait->cbst)->caLinkStruct[i].puserPvt
                 = calloc(1,sizeof(struct recDynLinkPvt));
        puserPvt = ((struct cbStruct *)pwait->cbst)->caLinkStruct[i].puserPvt;
        puserPvt->pwait = pwait;
        puserPvt->linkIndex = i;
      }

    /* do scanIoInit here because init_dev doesn't know which record */
    scanIoInit(&(((struct cbStruct *)pwait->cbst)->ioscanpvt));

    return(0);
    }

    /* This is pass == 1, so pwait->cbst is valid */

    pcbst = (struct cbStruct *)pwait->cbst;

    pwait->clcv=postfix(pwait->calc,pwait->rpcl,&error_number);
    if(pwait->clcv){
        recGblRecordError(S_db_badField,(void *)pwait,
                          "wait: init_record: Illegal CALC field");
    }
    db_post_events(pwait,&pwait->clcv,DBE_VALUE);

    callbackSetCallback(execOutput, &pcbst->doOutCb);
    callbackSetPriority(pwait->prio, &pcbst->doOutCb);
    callbackSetCallback(ioIntProcess, &pcbst->ioProcCb);
    callbackSetPriority(pwait->prio, &pcbst->ioProcCb);
    callbackSetUser(pwait, &pcbst->ioProcCb);
    pcbst->pwait = pwait;
    pcbst->wd_id = wdCreate();
    if((pcbst->monitorQ=rngCreate(sizeof(struct qStruct)*Q_SIZE)) == NULL) {
        errMessage(0,"recWait can't create ring buffer");
        exit(1);
    }

    if (status=initSiml(pwait)) return(status);

    /* reset miscellaneous flags */
    pcbst->outputWait = 0;
    pcbst->procPending = 0;

    pwait->init = TRUE;

    /* Do initial lookup of PV Names using recDynLink lib */

    *ppvn = &pwait->inan[0];
    pPvStat = &pwait->inav;

    /* check all dynLinks for non-NULL  */
    for(i=0;i<NUM_LINKS; i++, pPvStat++, *ppvn += PVN_SIZE) {
        if(*ppvn[0] != NULL) {
            *pPvStat = PV_NC;
            if(i<OUT_INDEX) {
                recDynLinkAddInput(&pcbst->caLinkStruct[i], *ppvn, 
                 DBR_DOUBLE, rdlSCALAR, pvSearchCallback, inputChanged);
            }
            else {
                recDynLinkAddOutput(&pcbst->caLinkStruct[i], *ppvn,
                DBR_DOUBLE, rdlSCALAR, pvSearchCallback);
            }
            if(recWaitDebug > 5) printf("Search during init\n");
        }
        else {
            *pPvStat = NO_PV;
        }
    } 
    pwait->init = TRUE;
    return(0);
}

static long process(pwait)
	struct waitRecord	*pwait;
{
        short async    = FALSE;
        long  status;

        pwait->pact = TRUE;

        /* Check for simulation mode */
        status=dbGetLink(&(pwait->siml),DBR_ENUM,&(pwait->simm),0,0);

        /* reset procPending before getting values */
        ((struct cbStruct *)pwait->cbst)->procPending = 0;

        if(pwait->simm == NO) {
            if(fetch_values(pwait)==0) {
                if(calcPerform(&pwait->a,&pwait->val,pwait->rpcl)) {
                        recGblSetSevr(pwait,CALC_ALARM,INVALID_ALARM);
                } else pwait->udf = FALSE;
            }
            else {
                recGblSetSevr(pwait,READ_ALARM,INVALID_ALARM);
            }
        }
        else {      /* SIMULATION MODE */
            status = dbGetLink(&(pwait->siol),DBR_DOUBLE,&(pwait->sval),0,0);
            if (status==0){
                pwait->val=pwait->sval;
                pwait->udf=FALSE;
            }
            recGblSetSevr(pwait,SIMM_ALARM,pwait->sims);
        }

        /* decide whether to write Output PV  */
        switch(pwait->oopt) {
          case REC_WAIT_OUT_OPT_EVERY:
            schedOutput(pwait);
            async = TRUE;
            break;
          case REC_WAIT_OUT_OPT_CHANGE:
            if(fabs(pwait->oval - pwait->val) > pwait->mdel)  {
              schedOutput(pwait);
              async = TRUE;
            }
            break;
          case REC_WAIT_OUT_OPT_CHG_TO_ZERO:
            if((pwait->oval != 0) && (pwait->val == 0)) {
              schedOutput(pwait);
              async = TRUE;
            }
            break;
          case REC_WAIT_OUT_OPT_CHG_TO_NZERO:
            if((pwait->oval == 0) && (pwait->val != 0)) {
              schedOutput(pwait);
              async = TRUE;
            }
            break;
          case REC_WAIT_OUT_OPT_WHEN_ZERO:
            if(!pwait->val) {
              schedOutput(pwait);
              async = TRUE;
            }
            break;
          case REC_WAIT_OUT_OPT_WHEN_NZERO:
            if(pwait->val) {
              schedOutput(pwait);
              async = TRUE;
            }
            break;
          default:
            break;
        }

        pwait->oval = pwait->val;

        recGblGetTimeStamp(pwait); 
        /* check event list */
        monitor(pwait);

        if (!async) {
          pwait->pact = FALSE;
        }
        return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    struct waitRecord  	*pwait = (struct waitRecord *)(paddr->precord);
    struct cbStruct     *pcbst = (struct cbStruct *)pwait->cbst;
    int           	special_type = paddr->special;
    char                *ppvn[PVN_SIZE];
    unsigned short      *pPvStat;
    unsigned short       oldStat;
    int  index;
    short error_number;

    if(recWaitDebug) printf("entering special %d \n",after);

    if(!after) {  /* this is called before ca changes the field */
        
        /* check if changing any dynamic link names  */
        /* This is where one would do a recDynLinkClear, but it is
           not required prior to a new search */
     return(0);
     }

    /* this is executed after ca changed the field */
    if((special_type >= REC_WAIT_A) && 
       (special_type < (REC_WAIT_A + NUM_LINKS))) {
        index = special_type - REC_WAIT_A; /* index of input */
        pPvStat = &pwait->inav + index; /* pointer arithmetic */
        oldStat = *pPvStat;
        *ppvn = &pwait->inan[0] + (index*PVN_SIZE); 
        if(*ppvn[0] != NULL) {
            if(recWaitDebug > 5) printf("Search during special \n");
            *pPvStat = PV_NC;
            /* need to post_event before recDynLinkAddXxx because
               SearchCallback could happen immediatley */
            if(*pPvStat != oldStat) {
                db_post_events(pwait,pPvStat,DBE_VALUE);
            }
            if(index<OUT_INDEX) {
                recDynLinkAddInput(&pcbst->caLinkStruct[index], *ppvn, 
                DBR_DOUBLE, rdlSCALAR, pvSearchCallback, inputChanged);
            }
            else {
                recDynLinkAddOutput(&pcbst->caLinkStruct[index], *ppvn,
                DBR_DOUBLE, rdlSCALAR, pvSearchCallback);
            }
        }
        else if(*pPvStat != NO_PV) {
            /* PV is now NULL but didn't used to be */
            *pPvStat = NO_PV;              /* PV just cleared */
            if(*pPvStat != oldStat) {
                db_post_events(pwait,pPvStat,DBE_VALUE);
            }
            recDynLinkClear(&pcbst->caLinkStruct[index]);
        }
        return(0);
    }
    else if(special_type == SPC_CALC) {
        pwait->clcv=postfix(pwait->calc,pwait->rpcl,&error_number);
        if(pwait->clcv){
                recGblRecordError(S_db_badField,(void *)pwait,
                        "wait: init_record: Illegal CALC field");
        }
        db_post_events(pwait,&pwait->clcv,DBE_VALUE);
        return(0);
    }
    else if(paddr->pfield==(void *)&pwait->prio) {
        callbackSetPriority(pwait->prio, &pcbst->doOutCb);
        callbackSetPriority(pwait->prio, &pcbst->ioProcCb);
        return(0);
    }
    else {
	recGblDbaddrError(S_db_badChoice,paddr,"wait: special");
	return(S_db_badChoice);
        return(0);
    }
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct waitRecord	*pwait=(struct waitRecord *)paddr->precord;

    *precision = pwait->prec;
    if(paddr->pfield == (void *)&pwait->val) {
        *precision = pwait->prec;
    }
    else if(paddr->pfield == (void *)&pwait->odly) {
        *precision = 3;
    }
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct waitRecord	*pwait=(struct waitRecord *)paddr->precord;

    if (paddr->pfield==(void *)&pwait->val) { 
          pgd->upper_disp_limit = pwait->hopr;
          pgd->lower_disp_limit = pwait->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble *pad;
{
    recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void monitor(pwait)
    struct waitRecord   *pwait;
{
        unsigned short  monitor_mask;
        double          delta;
        double          *pnew;
        double          *pprev;
        int             i;

        monitor_mask = recGblResetAlarms(pwait);
        /* check for value change */
        delta = pwait->mlst - pwait->val;
        if(delta<0.0) delta = -delta;
        if (delta > pwait->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pwait->mlst = pwait->val;
        }
        /* check for archive change */
        delta = pwait->alst - pwait->val;
        if(delta<0.0) delta = -delta;
        if (delta > pwait->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pwait->alst = pwait->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pwait,&pwait->val,monitor_mask);
        }
        /* check all input fields for changes */
        for(i=0, pnew=&pwait->a, pprev=&pwait->la; i<ARG_MAX;
            i++, pnew++, pprev++) {
            if(*pnew != *pprev) {
                 db_post_events(pwait,pnew,monitor_mask|DBE_VALUE);
                 *pprev = *pnew;
                 db_post_events(pwait,pprev,monitor_mask|DBE_VALUE);
            }
        }
        return;
}


static long initSiml(pwait)
struct waitRecord   *pwait;
{ 

    /* wait.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pwait->siml.type == CONSTANT) {
	recGblInitConstantLink(&pwait->siml,DBF_USHORT,&pwait->simm);
    }

    /* wait.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pwait->siol.type == CONSTANT) {
	recGblInitConstantLink(&pwait->siol,DBF_DOUBLE,&pwait->sval);
    }

    return(0);
}

static int fetch_values(pwait)
struct waitRecord *pwait;
{
        struct cbStruct *pcbst = (struct cbStruct *)pwait->cbst;
        double          *pvalue;
        unsigned short  *pPvStat;
        unsigned short  *piointInc;   /* include for IO_INT ? */
        long            status=0;
	size_t		nRequest=1;
        int             i;

        piointInc  = &pwait->inap;
        for(i=0,  pvalue=&pwait->a, pPvStat = &pwait->inav;
            i<ARG_MAX; i++, pvalue++, pPvStat++, piointInc++) {

            /* if any input should be connected, but is not, return */
            if(*pPvStat == PV_NC) {
                 status = ERROR;
            }

            /* only fetch a value if the connection is valid */
            /* if not in SCAN_IO_EVENT, fetch all valid inputs */
            /* if in SCAN_IO_EVENT, only fetch inputs if INxP ==  0 */
            /* The data from those with INxP=1 comes from the ring buffer */
            else if((*pPvStat == PV_OK)&&
                    ((pwait->scan != SCAN_IO_EVENT) || 
                     ((pwait->scan == SCAN_IO_EVENT) && !*piointInc))) {
               if(recWaitDebug > 5) printf("Fetching input %d \n",i);
               status = recDynLinkGet(&pcbst->caLinkStruct[i], pvalue,
                                      &nRequest, 0, 0, 0);
            }
            if (!RTN_SUCCESS(status)) return(status);
        }
        return(0);
}

/***************************************************************************
 *
 * The following functions schedule and/or request the execution of the
 * output PV and output event based on the Output Execution Delay (ODLY).
 * If .odly > 0, a watchdog is scheduled; if 0, execOutput() is called 
 * immediately.
 * NOTE: THE RECORD REMAINS "ACTIVE" WHILE WAITING ON THE WATCHDOG
 *
 **************************************************************************/
static void schedOutput(pwait)
    struct waitRecord   *pwait;
{

struct cbStruct *pcbst = (struct cbStruct *)pwait->cbst;

int                   wdDelay;
 
    if(pwait->odly > 0.0) {
      /* Use the watch-dog as a delay mechanism */
      pcbst->outputWait = 1;
      wdDelay = pwait->odly * sysClkRateGet();
      wdStart(pcbst->wd_id, wdDelay, (FUNCPTR)execOutput, (int)(pwait->cbst));
    } else {
      execOutput(pwait->cbst);
    }
}

/***************************************************************************
 *
 * This code calls recDynLinkPut to execute the output link. Since requests   
 * recDynLinkPut are done via another task, one need not worry about
 * lock sets.
 *
 ***************************************************************************/
void execOutput(pcbst)
   struct cbStruct *pcbst;
{
long status;
size_t nRequest = 1;
double oldDold; 

    /* if output link is valid , decide between VAL and DOL */
    if(!pcbst->pwait->outv) {
        if(pcbst->pwait->dopt) {
            if(!pcbst->pwait->dolv) {
                oldDold = pcbst->pwait->dold;
                status = recDynLinkGet(&pcbst->caLinkStruct[DOL_INDEX],
                               &(pcbst->pwait->dold), &nRequest, 0, 0, 0);
                if(pcbst->pwait->dold != oldDold)
                  db_post_events(pcbst->pwait,&pcbst->pwait->dold,DBE_VALUE);
            }
            status = recDynLinkPut(&pcbst->caLinkStruct[OUT_INDEX],
                                &(pcbst->pwait->dold), 1);
        } else {
            status = recDynLinkPut(&pcbst->caLinkStruct[OUT_INDEX],
                                &(pcbst->pwait->val), 1);
        }
    }
   
    /* post event if output event != 0 */
    if(pcbst->pwait->oevt > 0) {
        post_event((int)pcbst->pwait->oevt);
    }

    recGblFwdLink(pcbst->pwait);
    pcbst->pwait->pact = FALSE;
    pcbst->outputWait = 0;

    /* If I/O Interrupt scanned, see if any inputs changed during delay */
    if((pcbst->pwait->scan == SCAN_IO_EVENT) && (pcbst->procPending == 1)) {
       scanOnce(pcbst->pwait);
    }

    return;
}



/* This routine is called by the recDynLink task whenver a monitored input
 * changes. If the particular input is flagged to cause record processing, 
 * The input index and new data are put on a work queue, and a callback
 * request is issued to the routine ioIntProcess
 */

static void inputChanged(recDynLink *precDynLink)
{
  struct waitRecord *pwait = ((recDynLinkPvt *)precDynLink->puserPvt)->pwait;
  struct cbStruct   *pcbst = (struct cbStruct   *)pwait->cbst;
  double             monData;
  size_t      	     nRequest;
  char               index;
  unsigned short    *piointInc;

    if(pwait->scan != SCAN_IO_EVENT) return; 

    index = (char)((recDynLinkPvt *)precDynLink->puserPvt)->linkIndex;

    piointInc = &pwait->inap + index;    /* pointer arithmetic */
    if(*piointInc == 0) return;     /* input cause processing ???*/

    /* put input index and monitored data on processing queue */
    recDynLinkGet(precDynLink, &monData, &nRequest, 0, 0, 0); 
    if(recWaitDebug>5)
        printf("queuing monitor on %d = %f\n",index,monData);
    if(rngBufPut(pcbst->monitorQ, (void *)&index, sizeof(char))
        != sizeof(char)) errMessage(0,"recWait rngBufPut error");
    if(rngBufPut(pcbst->monitorQ, (void *)&monData, sizeof(double))
        != sizeof(double)) errMessage(0,"recWait rngBufPut error");
    callbackRequest(&pcbst->ioProcCb);
}


/* This routine performs the record processing when in SCAN_IO_EVENT. An
   event queue is built by inputChanged() and emptied here so each change
   of an input causes the record to process.
*/
static void ioIntProcess(CALLBACK *pioProcCb)
{

    struct waitRecord *pwait;
    struct cbStruct   *pcbst;

    char     inputIndex;
    double   monData;
    double   *pInput; 

    callbackGetUser(pwait, pioProcCb);
    pcbst = (struct cbStruct *)pwait->cbst;
    pInput = &pwait->a;  /* a pointer to the first input field */

    if(pwait->scan != SCAN_IO_EVENT) return;

    if(!recWaitCacheMode) {
      if(rngBufGet(pcbst->monitorQ, (void *)&inputIndex, sizeof(char))
          != sizeof(char)) errMessage(0, "recWait: rngBufGet error");
      if(rngBufGet(pcbst->monitorQ, (void *)&monData, sizeof(double))
          != sizeof(double)) errMessage(0, "recWait: rngBufGet error");

      if(recWaitDebug>=5)
          printf("processing on %d = %f  (%f)\n",
                  inputIndex, monData,pwait->val);

      pInput += inputIndex;   /* pointer arithmetic for appropriate input */ 
      dbScanLock((struct dbCommon *)pwait);
      *pInput = monData;      /* put data in input data field */
      
      /* Process the record, unless busy waiting to do the output link */
      if(pcbst->outputWait) {
          pcbst->procPending = 1;
          if(recWaitDebug) printf("record busy, setting procPending\n");
      }
      else {
          dbProcess((struct dbCommon *)pwait);     /* process the record */
      }
      dbScanUnlock((struct dbCommon *)pwait);
    }
    else {
      if(recWaitDebug>=5) printf("processing (cached)\n");
      dbScanLock((struct dbCommon *)pwait);
      dbProcess((struct dbCommon *)pwait);     /* process the record */
      dbScanUnlock((struct dbCommon *)pwait);
    } 

}
    

LOCAL void pvSearchCallback(recDynLink *precDynLink)
{

    recDynLinkPvt     *puserPvt = (recDynLinkPvt *)precDynLink->puserPvt;
    struct waitRecord *pwait    = puserPvt->pwait;
    unsigned short     index    = puserPvt->linkIndex;
    unsigned short    *pPvStat;
    unsigned short     oldValid;

    pPvStat = &pwait->inav + index;    /* pointer arithmetic */
    puserPvt = (recDynLinkPvt *)precDynLink->puserPvt;

    oldValid = *pPvStat;
    if(recDynLinkConnectionStatus(precDynLink)) {
        *pPvStat = PV_NC;
        if(recWaitDebug) printf("Search Callback: No Connection\n");
    }
    else {
        *pPvStat = PV_OK;
        if(recWaitDebug) printf("Search Callback: Success\n");
    }
    if(*pPvStat != oldValid) {
        db_post_events(pwait, pPvStat, DBE_VALUE);
    }
        
}


