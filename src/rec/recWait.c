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
 * .01  05-31-94        nda     initial try            
 * .02  07-11-94    mrk/nda     added "process on input change" feature
 * .03  08-16-94    mrk/nda     continuing "process on input change" feature
 * .04  08-16-94        nda     record does not get notified when a SCAN related field changes,
 *                              so for now we have to always add monitors. Search for MON_ALWAYS
 *                              Modifications for this are flagged with MON_ALWAYS
 * .05  08-18-94        nda     Starting with R3.11.6, dbGetField locks the record before fetching
 *                              the data. This can cause deadlocks within a database. Change all
 *                              dbGetField() to dbGet()
 * .06  08-19-94        nda     added Output data option of VAL or DOL                         
 * .07  09-14-94        nda     corrected bug that caused SCAN_DISABLE to lock up the record forever
 * .08  02-01-95        nda     added VERS and ODLY (output execution delay)
 *
 *
 */

#define VERSION 1.08



#include	<vxWorks.h> 
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
#include	<math.h>
#include	<tickLib.h>
#include	<semLib.h>
#include	<taskLib.h>
#include        <wdLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include        <dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<callback.h>
#include	<taskwd.h>

#include	<choiceWait.h>
#include	<waitRecord.h>
#include	<caMonitor.h>


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
static long get_value();
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
#define   ARG_MAX   12
#define   PVN_SIZE     40   /* must match the string length defined in waitRecord.ascii */

/***************************
  Declare constants
***************************/
int    waitRecDebug=0;
static unsigned long tickStart;
static void schedOutput();
static void reqOutput();
static void execOutput();
static int fetch_values();
static void monitor();
static long initSiml();
static void inputChanged();


/* callback structure and miscellaneous data */
struct cbStruct {
        CALLBACK           callback;  /* code assumes CALLBACK is 1st in structure */
        struct waitRecord *pwait;     /* pointer to wait record which needs work done */
        WDOG_ID            wd_id;     /* Watchdog used for delays */
        IOSCANPVT          ioscanpvt; /* used for IO_EVENT scanning */
        CAMONITOR          inpMonitor[12];  /* required structures for each input variable */
        int                procPending;     /* flag to indicate record processing is pending */
};
 

static long get_ioint_info(cmd,pwait,ppvt)
        int                     cmd;
        struct waitRecord       *pwait;
        IOSCANPVT               *ppvt;
{
    *ppvt = (((struct cbStruct *)pwait->cbst)->ioscanpvt);
    return(0);
}


static long init_record(pwait,pass)
    struct waitRecord	*pwait;
    int pass;
{
    struct cbStruct *pcbst;
    long    status = 0;
    int i;

    char          *ppvn[PVN_SIZE];
    struct dbAddr **ppdbAddr;   /* ptr to a ptr to dbAddr */
    long          *paddrValid;

    char rpbuf[184];
    short error_number;

    if (pass==0) {
      pwait->vers = VERSION;
      pwait->inaa = calloc(1,sizeof(struct dbAddr));
      pwait->inba = calloc(1,sizeof(struct dbAddr));
      pwait->inca = calloc(1,sizeof(struct dbAddr));
      pwait->inda = calloc(1,sizeof(struct dbAddr));
      pwait->inea = calloc(1,sizeof(struct dbAddr));
      pwait->infa = calloc(1,sizeof(struct dbAddr));
      pwait->inga = calloc(1,sizeof(struct dbAddr));
      pwait->inha = calloc(1,sizeof(struct dbAddr));
      pwait->inia = calloc(1,sizeof(struct dbAddr));
      pwait->inja = calloc(1,sizeof(struct dbAddr));
      pwait->inka = calloc(1,sizeof(struct dbAddr));
      pwait->inla = calloc(1,sizeof(struct dbAddr));
      pwait->outa = calloc(1,sizeof(struct dbAddr));
 
      pwait->cbst = calloc(1,sizeof(struct cbStruct));

      /* init as much as we can  */
      *ppvn = &pwait->inan[0];
      for(i=0;i<ARG_MAX; i++, *ppvn += PVN_SIZE) {
        ((struct cbStruct *)pwait->cbst)->inpMonitor[i].channame = (char *)*ppvn;
        ((struct cbStruct *)pwait->cbst)->inpMonitor[i].callback = inputChanged;
        ((struct cbStruct *)pwait->cbst)->inpMonitor[i].userPvt = pwait;
      }
      
    /* do scanIoInit here because init_dev doesn't know which record */
    scanIoInit(&(((struct cbStruct *)pwait->cbst)->ioscanpvt));

    return(0);
    }

    /* Do initial lookup of PV Names to dbAddr's */

    *ppvn = &pwait->inan[0];
    ppdbAddr = (struct dbAddr **)&pwait->inaa;
    paddrValid = &pwait->inav;

    for(i=0;i<ARG_MAX; i++, *ppvn += PVN_SIZE, ppdbAddr++, paddrValid++) {
        *paddrValid = dbNameToAddr(*ppvn, *ppdbAddr);  
    } 

    pwait->outv = dbNameToAddr(pwait->outn, (struct dbAddr *)pwait->outa);
    pwait->dolv = dbNameToAddr(pwait->doln, (struct dbAddr *)pwait->dola);

    pwait->clcv=postfix(pwait->calc,rpbuf,&error_number);
    if(pwait->clcv){
        recGblRecordError(S_db_badField,(void *)pwait,
                          "wait: init_record: Illegal CALC field");
    }
    else {
        memcpy(pwait->rpcl,rpbuf,sizeof(pwait->rpcl));
    }
    db_post_events(pwait,&pwait->clcv,DBE_VALUE);

    callbackSetCallback(execOutput, &((struct cbStruct *)pwait->cbst)->callback);
    callbackSetPriority(pwait->prio, &((struct cbStruct *)pwait->cbst)->callback);
    pcbst = (struct cbStruct *)pwait->cbst;
    pcbst->pwait = pwait;
    pcbst->wd_id = wdCreate();

    /* Set up monitors on input channels if scan type is IO Event */

/* MON_ALWAYS  if(pwait->scan == SCAN_IO_EVENT)  {  */
    if(1) {
        paddrValid = &pwait->inav;

        for(i=0;i<ARG_MAX; i++, paddrValid++) {
            if(!(*paddrValid)) {
                if(waitRecDebug) printf("adding monitor\n");
                status = caMonitorAdd(&(((struct cbStruct *)pwait->cbst)->inpMonitor[i]));
                if(status) errMessage(status,"caMonitorAdd error");
            }
        }
    }

    if (status=initSiml(pwait)) return(status);

    /* now reset procPending so the next monitor processes the record */
    ((struct cbStruct *)pwait->cbst)->procPending = 0;

    pwait->init = TRUE;
    return(0);
}

static long process(pwait)
	struct waitRecord	*pwait;
{
        short async    = FALSE;
        long  status;
        long  nRequest = 1;
        long  options  = 0;

        pwait->pact = TRUE;
        pwait->oval = pwait->val;

        /* Check for simulation mode */
        status=recGblGetLinkValue(&(pwait->siml),
               (void *)pwait,DBR_ENUM,&(pwait->simm),&options,&nRequest);

        /* reset procPending before getting values so we don't miss any monitors */
        ((struct cbStruct *)pwait->cbst)->procPending = 0;

        if(pwait->simm == NO) {
            if(fetch_values(pwait)==0) {
                if(calcPerform(&pwait->a,&pwait->val,pwait->rpcl)) {
                        recGblSetSevr(pwait,CALC_ALARM,INVALID_ALARM);
                } else pwait->udf = FALSE;
            }
        }
        else {      /* SIMULATION MODE */
            nRequest = 1;
            status  = recGblGetLinkValue(&(pwait->siol),
                     (void *)pwait,DBR_DOUBLE,&(pwait->sval),&options,&nRequest);
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
    int           	special_type = paddr->special;
    char                *ppvn[PVN_SIZE];
    struct dbAddr       **ppdbAddr;   /* ptr to a ptr to dbAddr */
    long                *paddrValid;
    int  i;
    long status;
    long odbv  =0;    
    short error_number;
    char rpbuf[184];

    if(waitRecDebug) printf("entering special \n");
    if(!after) {  /* this is called before ca changes the field */
        switch(special_type) {
          case(SPC_SCAN):   /* about to change SCAN mechanism ... */
              if(pwait->scan == SCAN_IO_EVENT) { 
                  /* Leaving IO_EVENT, delete monitors */
                  paddrValid = &pwait->inav;
                  for(i=0;i<ARG_MAX; i++, paddrValid++) {
                      if(!(*paddrValid)) {
                        if(waitRecDebug) printf("deleting monitor\n");
                        status = caMonitorDelete(&(((struct cbStruct *)pwait->cbst)->inpMonitor[i]));
                        if(status) errMessage(status,"caMonitorDelete error");
                      }
                  }
              }
              break;

          case(SPC_MOD):     /* check if changing any PV names while monitored */
/* MON_ALWAYS if(pwait->scan == SCAN_IO_EVENT) { */
              if(1) {
                  *ppvn = &pwait->inan[0];
                  paddrValid = &pwait->inav;
                  for(i=0;i<ARG_MAX; i++, *ppvn += PVN_SIZE, paddrValid++) {
                      if((paddr->pfield==*ppvn) && !(*paddrValid)) {
                          if(waitRecDebug) printf("deleting monitor\n");
                          status = caMonitorDelete(&(((struct cbStruct *)pwait->cbst)->inpMonitor[i]));
                          if(status) errMessage(status,"caMonitorDelete error");
                      }
                  }
               }
               break;
        }
        return(0);
    }

    /* this is executed after ca changed the field */
    switch(special_type) {
      case(SPC_SCAN):   /* Changed SCAN mechanism, set monitors on input links */
          if(pwait->scan == SCAN_IO_EVENT) { 
              paddrValid = &pwait->inav;
              for(i=0;i<ARG_MAX; i++, paddrValid++) {
                  if(!(*paddrValid)) {
                     if(waitRecDebug) printf("adding monitor\n");
                     status = caMonitorAdd(&(((struct cbStruct *)pwait->cbst)->inpMonitor[i]));
                     if(status) errMessage(status,"caMonitorAdd error");
                  }
              }
          }
          return(0);
      case(SPC_MOD):        /* check if changing any PV names */
        *ppvn = &pwait->inan[0];
        ppdbAddr = (struct dbAddr **)&pwait->inaa;
        paddrValid = &pwait->inav;
      
        for(i=0;i<ARG_MAX; i++, *ppvn += PVN_SIZE, ppdbAddr++, paddrValid++) {
            if(paddr->pfield==*ppvn) {
                odbv = *paddrValid;
                *paddrValid = dbNameToAddr(*ppvn, *ppdbAddr);
                if (odbv != *paddrValid) {
                    db_post_events(pwait,paddrValid,DBE_VALUE);
                }
/* MON_ALWAYS   if((pwait->scan == SCAN_IO_EVENT) && !(*paddrValid)) { */
                if((1) && !(*paddrValid)) {
                    if(waitRecDebug) printf("adding monitor\n");
                    status = caMonitorAdd(&(((struct cbStruct *)pwait->cbst)->inpMonitor[i]));
                    if(status) errMessage(status,"caMonitorAdd error");
                }
                return(0);
            }
        }
        if(paddr->pfield==pwait->outn) {  /* this is the output link */ 
            odbv = pwait->outv;
            pwait->outv = dbNameToAddr(pwait->outn,(struct dbAddr *)pwait->outa);
            if (odbv != pwait->outv) {
               db_post_events(pwait,&pwait->outv,DBE_VALUE);
            }
        } 
        else if(paddr->pfield==pwait->doln) {  /* this is the DOL link */ 
            odbv = pwait->dolv;
            pwait->dolv = dbNameToAddr(pwait->doln,(struct dbAddr *)pwait->dola);
            if (odbv != pwait->dolv) {
               db_post_events(pwait,&pwait->dolv,DBE_VALUE);
            }
        }
        else if(paddr->pfield==(void *)&pwait->prio) {
            callbackSetPriority(pwait->prio, &((struct cbStruct *)pwait->cbst)->callback);
        }

        return(0);

      case(SPC_CALC):
        pwait->clcv=postfix(pwait->calc,rpbuf,&error_number);
        if(pwait->clcv){
                recGblRecordError(S_db_badField,(void *)pwait,
                        "wait: init_record: Illegal CALC field");
                db_post_events(pwait,&pwait->clcv,DBE_VALUE);
                return(S_db_badField);
        }
        db_post_events(pwait,&pwait->clcv,DBE_VALUE);
        memcpy(pwait->rpcl,rpbuf,sizeof(pwait->rpcl));
        db_post_events(pwait,pwait->calc,DBE_VALUE);
        db_post_events(pwait,pwait->clcv,DBE_VALUE);
        return(0);
    default:
	recGblDbaddrError(S_db_badChoice,paddr,"wait: special");
	return(S_db_badChoice);
        return(0);
    }
}

static long get_value(pwait,pvdes)
    struct waitRecord	*pwait;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &pwait->val;
    return(0);
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
        for(i=0, pnew=&pwait->a, pprev=&pwait->la; i<ARG_MAX; i++, pnew++, pprev++) {
             if(*pnew != *pprev) {
                  db_post_events(pwait,pnew,monitor_mask|DBE_VALUE);
                  *pprev = *pnew;
                     }
                }
        return;
}


static long initSiml(pwait)
struct waitRecord   *pwait;
{ 
    long status;
    /* wait.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pwait->siml.type) {
    case (CONSTANT) :
        pwait->simm = pwait->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pwait->siml), (void *) pwait, "SIMM");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pwait,
                "wait: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* wait.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pwait->siol.type) {
    case (CONSTANT) :
        pwait->sval = pwait->siol.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pwait->siol), (void *) pwait, "SVAL");
        if(status) return(status);
        break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pwait,
                "wait: init_record Illegal SIOL field");
        return(S_db_badField);
    }

    return(0);
}

static int fetch_values(pwait)
struct waitRecord *pwait;
{
        struct dbAddr   **ppdba; /* a ptr to a ptr to dbAddr  */
        double          *pvalue;
        long            *pvalid;
        long            status=0,options=0,nRequest=1;
        int             i;

        for(i=0, ppdba= (struct dbAddr **)&pwait->inaa, pvalue=&pwait->a, pvalid = &pwait->inav;
            i<ARG_MAX; i++, ppdba++, pvalue++, pvalid++) {

            /* only fetch a value if the dbAddr is valid, otherwise, leave it alone */
            if(!(*pvalid)) {
                status = dbGet(*ppdba, DBR_DOUBLE,
                               pvalue, &options, &nRequest, NULL);
            }

            if (!RTN_SUCCESS(status)) return(status);
        }
        return(0);
}

/******************************************************************************
 *
 * The following functions schedule and/or request the execution of the output
 * PV and output event based on the Output Execution Delay (ODLY).
 * If .odly > 0, a watchdog is scheduled; if 0, reqOutput() is called immediately.
 *
 ******************************************************************************/
static void schedOutput(pwait)
    struct waitRecord   *pwait;
{

struct cbStruct *pcbst = (struct cbStruct *)pwait->cbst;

int                   wdDelay;
 
    if(pwait->odly > 0.0) {
      /* Use the watch-dog as a delay mechanism */
      wdDelay = pwait->odly * sysClkRateGet();
      wdStart(pcbst->wd_id, wdDelay, (FUNCPTR)reqOutput, (int)(pwait));
    } else {
      reqOutput(pwait);
    }
}

static void reqOutput(pwait)
    struct waitRecord   *pwait;
{

    callbackRequest(pwait->cbst);

}
/******************************************************************************
 *
 * This is the code that is executed by the callback task to do the record    
 * outputs. It is done with a separate task so one need not worry about
 * lock sets.
 *
 ******************************************************************************/
void execOutput(pcbst)
   struct cbStruct *pcbst;
{
static long status;
static long nRequest = 1;
static long options  = 0;
static double oldDold; 

    /* if output link is valid , decide between VAL and DOL */
    if(!pcbst->pwait->outv) {
        if(pcbst->pwait->dopt) {
            if(!pcbst->pwait->dolv) {
                oldDold = pcbst->pwait->dold;
                status = dbGet(pcbst->pwait->dola,DBR_DOUBLE,
                               &(pcbst->pwait->dold), &options, &nRequest, NULL);
                if(pcbst->pwait->dold != oldDold)
                    db_post_events(pcbst->pwait,&pcbst->pwait->dold,DBE_VALUE);
            }
            status = dbPutField(pcbst->pwait->outa,DBR_DOUBLE,
                                &(pcbst->pwait->dold), 1);
        } else {
            status = dbPutField(pcbst->pwait->outa,DBR_DOUBLE,
                                &(pcbst->pwait->val), 1);
        }
    }
   
    /* post event if output event != 0 */
    if(pcbst->pwait->oevt > 0) {
        post_event((int)pcbst->pwait->oevt);
    }

    recGblFwdLink(pcbst->pwait);
    pcbst->pwait->pact = FALSE;

    /* If I/O Interrupt scanned, see if any inputs changed during delay */
    if((pcbst->pwait->scan == SCAN_IO_EVENT) && (pcbst->procPending == 1)) {
       scanOnce(pcbst->pwait);
    }

    return;
}



static void inputChanged(struct caMonitor *pcamonitor)
{

    struct waitRecord *pwait = (struct waitRecord *)pcamonitor->userPvt;
    struct cbStruct   *pcbst = (struct cbStruct   *)pwait->cbst;

/* the next line is here because the monitors are always active ... MON_ALWAYS */
    if(pwait->scan != SCAN_IO_EVENT) return; 


    /* if record hasn't been processed or is DISABLED, don't set procPending yet */
    if((pwait->stat == DISABLE_ALARM) || pwait->udf) { 
        if(waitRecDebug) printf("processing due to monitor\n");
        scanIoRequest(pcbst->ioscanpvt);
    } else if(pcbst->procPending) {
        if(waitRecDebug) printf("discarding monitor\n");
        return;
    } else {
        pcbst->procPending = 1;
        if(waitRecDebug) printf("processing due to monitor\n");
        scanIoRequest(pcbst->ioscanpvt);
    } 
}
