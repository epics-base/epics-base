/* recHistogram.c */
/* base/src/rec  $Id$ */
 
/* recHistogram.c - Record Support Routines for Histogram records */
/*
 *      Author:      Janet Anderson
 *      Date:        5/20/91
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
 * Modification Log:
 * -----------------
 * .01  10-14-91        jba     Added dev sup  crtl fld and wd timer
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03  02-28-92	jba	ANSI C changes
 * .04  04-10-92        jba     pact now used to test for asyn processing, not status
 * .05  04-18-92        jba     removed process from dev init_record parms
 * .06  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .07  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .08  08-13-92        jba     Added simulation processing
 * .09	01-24-94	mcn	Converted to Fast Links
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "osiWatchdog.h"
#include "osiClock.h"
#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "epicsPrint.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "special.h"
#include "recSup.h"
#define GEN_SIZE_OFFSET
#include "histogramRecord.h"
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
#define  put_array_info NULL
#define get_units NULL
static long get_precision(struct dbAddr *paddr,long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_alarm_double NULL
static long get_graphic_double(struct dbAddr *paddr,struct dbr_grDouble *pgd);
static long get_control_double(struct dbAddr *paddr,struct dbr_ctrlDouble *pcd);

struct rset histogramRSET={
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
     get_alarm_double };

struct histogramdset { /* histogram input dset */
     long          number;
     DEVSUPFUN     dev_report;
     DEVSUPFUN     init;
     DEVSUPFUN     init_record; /*returns: (-1,0)=>(failure,success)*/
     DEVSUPFUN     get_ioint_info;
     DEVSUPFUN     read_histogram;/*(0,2)=> success and add_count, don't add_count)*/
               /* if add_count then sgnl added to array */
     DEVSUPFUN     special_linconv;
};

/* control block for callback*/
struct callback {
     void (*callback)();
     int priority;
     struct dbAddr dbAddr;
     watchdogId wd_id;
};

/*
void callbackRequest();
*/

static long add_count();
static long clear_histogram();
static void monitor();
static long readValue();

static void wdogCallback(pcallback)
     struct callback *pcallback;
{
     struct histogramRecord *phistogram=(struct histogramRecord *)(pcallback->dbAddr.precord);
     /* force post events for any count change */
     if(phistogram->mcnt>0){
          dbScanLock((struct dbCommon *)phistogram);
          recGblGetTimeStamp(phistogram);
          db_post_events(phistogram,phistogram->bptr,DBE_VALUE|DBE_LOG);
          phistogram->mcnt=0;
          dbScanUnlock((struct dbCommon *)phistogram);
     }

     if(phistogram->sdel>0) {
          /* start new watchdog timer on monitor */
          watchdogStart(pcallback->wd_id,
              (int)(phistogram->sdel *clockGetRate()),
              (WATCHDOGFUNC)callbackRequest,(void *)pcallback);
     }

     return;
}
static long wdogInit(phistogram)
    struct histogramRecord	*phistogram;
{
     struct callback *pcallback;

     if(phistogram->wdog==NULL && phistogram->sdel>0) {
          /* initialize a watchdog timer */
          pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
          phistogram->wdog = (void *)pcallback;
	  if(!pcallback) return -1;
          pcallback->callback = wdogCallback;
          pcallback->priority = priorityLow;
          pcallback->wd_id = watchdogCreate();
          dbNameToAddr(phistogram->name,&(pcallback->dbAddr));
     }

     if (!phistogram->wdog) return -1;
     pcallback = (struct callback *)phistogram->wdog;
     if(!pcallback) return -1;

     watchdogCancel(pcallback->wd_id);
  
     if( phistogram->sdel>0) {
         /* start new watchdog timer on monitor */
         watchdogStart(pcallback->wd_id,
             (int)(phistogram->sdel * clockGetRate()),
             (WATCHDOGFUNC)callbackRequest,(void *)pcallback);
     }
     return 0;
}

static long init_record(phistogram,pass)
    struct histogramRecord	*phistogram;
    int pass;
{
     struct histogramdset *pdset;
     long status;

     if (pass==0){

          /* allocate space for histogram array */
          if(phistogram->bptr==NULL) {
               if(phistogram->nelm<=0) phistogram->nelm=1;
               phistogram->bptr = (unsigned long *)calloc(phistogram->nelm,sizeof(long));
          }

          /* calulate width of array element */
          phistogram->wdth=(phistogram->ulim-phistogram->llim)/phistogram->nelm;

          return(0);
     }

    wdogInit(phistogram);

    if (phistogram->siml.type == CONSTANT) {
	recGblInitConstantLink(&phistogram->siml,DBF_USHORT,&phistogram->simm);
    }

    if (phistogram->siol.type == CONSTANT) {
	recGblInitConstantLink(&phistogram->siol,DBF_DOUBLE,&phistogram->sval);
    }

     /* must have device support defined */
     if(!(pdset = (struct histogramdset *)(phistogram->dset))) {
          recGblRecordError(S_dev_noDSET,(void *)phistogram,"histogram: init_record");
          return(S_dev_noDSET);
     }
     /* must have read_histogram function defined */
     if( (pdset->number < 6) || (pdset->read_histogram == NULL) ) {
          recGblRecordError(S_dev_missingSup,(void *)phistogram,"histogram: init_record");
          return(S_dev_missingSup);
     }
     /* call device support init_record */
     if( pdset->init_record ) {
          if((status=(*pdset->init_record)(phistogram))) return(status);
     }
     return(0);
}

static long process(phistogram)
     struct histogramRecord     *phistogram;
{
     struct histogramdset     *pdset = (struct histogramdset *)(phistogram->dset);
     long           status;
     unsigned char    pact=phistogram->pact;

     if( (pdset==NULL) || (pdset->read_histogram==NULL) ) {
          phistogram->pact=TRUE;
          recGblRecordError(S_dev_missingSup,(void *)phistogram,"read_histogram");
          return(S_dev_missingSup);
     }

     status=readValue(phistogram); /* read the new value */
     /* check if device support set pact */
     if ( !pact && phistogram->pact ) return(0);
     phistogram->pact = TRUE;

     recGblGetTimeStamp(phistogram);

     if(status==0)add_count(phistogram);
     else if(status==2)status=0;

     /* check event list */
     monitor(phistogram);

     /* process the forward scan link record */
     recGblFwdLink(phistogram);

     phistogram->pact=FALSE;
     return(status);
}

static long special(paddr,after)
     struct dbAddr *paddr;
     int           after;
{
     struct histogramRecord   *phistogram = (struct histogramRecord *)(paddr->precord);
     int                special_type = paddr->special;
     int                fieldIndex = dbGetFieldIndex(paddr);


     if(!after) return(0);
     switch(special_type) {
     case(SPC_CALC):
          if(phistogram->cmd <=1){
               clear_histogram(phistogram);
               phistogram->cmd =0;
          } else if (phistogram->cmd ==2){
               phistogram->csta=TRUE;
               phistogram->cmd =0;
          } else if (phistogram->cmd ==3){
               phistogram->csta=FALSE;
               phistogram->cmd =0;
          }
          return(0);
     case(SPC_MOD):
          /* increment frequency in histogram array */
          add_count(phistogram);
          return(0);
     case(SPC_RESET):
          if (fieldIndex == histogramRecordSDEL) {
              wdogInit(phistogram);
          } else {
              phistogram->wdth=(phistogram->ulim-phistogram->llim)/phistogram->nelm;
              clear_histogram(phistogram);
          }
          return(0);
     default:
          recGblDbaddrError(S_db_badChoice,paddr,"histogram: special");
          return(S_db_badChoice);
     }
}

static void monitor(phistogram)
     struct histogramRecord             *phistogram;
{
     unsigned short  monitor_mask;
 
     monitor_mask = recGblResetAlarms(phistogram);
     /* post events for count change */
     if(phistogram->mcnt>phistogram->mdel){
          /* post events for count change */
          monitor_mask |= DBE_VALUE|DBE_LOG;
          /* reset counts since monitor */
          phistogram->mcnt = 0;
     }
     /* send out monitors connected to the value field */
     if(monitor_mask) db_post_events(phistogram,phistogram->bptr,monitor_mask);

     return;
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;

    paddr->pfield = (void *)(phistogram->bptr);
    paddr->no_elements = phistogram->nelm;
    paddr->field_type = DBF_ULONG;
    paddr->field_size = sizeof(unsigned long);
    paddr->dbr_field_type = DBF_ULONG;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long       *no_elements;
    long       *offset;
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;

    *no_elements =  phistogram->nelm;
    *offset = 0;
    return(0);
}

static long add_count(phistogram)
     struct histogramRecord *phistogram;
{
     double            temp;
     unsigned long     *pdest;
     int               i;

     if(phistogram->csta==FALSE) return(0);

     if(phistogram->llim >= phistogram->ulim) {
               if (phistogram->nsev<INVALID_ALARM) {
                    phistogram->stat = SOFT_ALARM;
                    phistogram->sevr = INVALID_ALARM;
                    return(-1);
               }
     }
     if(phistogram->sgnl<phistogram->llim || phistogram->sgnl >= phistogram->ulim) return(0);

     temp=phistogram->sgnl-phistogram->llim;
     for (i=1;i<=phistogram->nelm;i++){
          if (temp<=(double)i*phistogram->wdth) break;
     }
     pdest=phistogram->bptr+i-1;
     if ( *pdest==ULONG_MAX) *pdest=0.0;
     (*pdest)++;
     phistogram->mcnt++;

     return(0);
}

static long clear_histogram(phistogram)
    struct histogramRecord *phistogram;
{
     int    i;

     for (i=0;i<=phistogram->nelm-1;i++)
           *(phistogram->bptr+i)=0.0;
     phistogram->mcnt=phistogram->mdel+1;
     phistogram->udf=FALSE;

     return(0);
}

static long readValue(phistogram)
        struct histogramRecord *phistogram;
{
        long            status;
        struct histogramdset   *pdset = (struct histogramdset *) (phistogram->dset);

        if (phistogram->pact == TRUE){
                status=(*pdset->read_histogram)(phistogram);
                return(status);
        }

        status=dbGetLink(&(phistogram->siml),DBR_USHORT,&(phistogram->simm),0,0);

        if (status)
                return(status);

        if (phistogram->simm == NO){
                status=(*pdset->read_histogram)(phistogram);
                return(status);
        }
        if (phistogram->simm == YES){
        	status=dbGetLink(&(phistogram->siol),DBR_DOUBLE,
			&(phistogram->sval),0,0);

                if (status==0){
                         phistogram->sgnl=phistogram->sval;
                }
        } else {
                status=-1;
                recGblSetSevr(phistogram,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(phistogram,SIMM_ALARM,phistogram->sims);

        return(status);
}
static long get_precision(struct dbAddr *paddr,long *precision)
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;
    int     fieldIndex = dbGetFieldIndex(paddr);

    *precision = phistogram->prec;
    if(fieldIndex == histogramRecordULIM
    || fieldIndex == histogramRecordLLIM
    || fieldIndex == histogramRecordSDEL
    || fieldIndex == histogramRecordSGNL
    || fieldIndex == histogramRecordSVAL
    || fieldIndex == histogramRecordWDTH) {
       *precision = phistogram->prec;
    }
    recGblGetPrec(paddr,precision);
    return(0);
}
static long get_graphic_double(struct dbAddr *paddr,struct dbr_grDouble *pgd)
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;
    int     fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == histogramRecordBPTR){
        pgd->upper_disp_limit = phistogram->hopr;
        pgd->lower_disp_limit = phistogram->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(struct dbAddr *paddr,struct dbr_ctrlDouble *pcd)
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;
    int     fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == histogramRecordBPTR){
        pcd->upper_ctrl_limit = phistogram->hopr;
        pcd->lower_ctrl_limit = phistogram->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
