/* recHistogram.c */
/* share/src/rec $Id$ */
 
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
 */

#include     <vxWorks.h>
#include     <types.h>
#include     <stdioLib.h>
#include     <lstLib.h>
#include     <strLib.h>
#include     <math.h>
#include     <limits.h>
#include     <wdLib.h>

#include     <alarm.h>
#include     <callback.h>
#include     <dbDefs.h>
#include     <dbAccess.h>
#include     <dbFldTypes.h>
#include     <devSup.h>
#include     <errMdef.h>
#include     <special.h>
#include     <recSup.h>
#include     <histogramRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
static long process();
long special();
long get_value();
long cvt_dbaddr();
long get_array_info();
#define  put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

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
     DEVSUPFUN     read_histogram;/*(0,1,2)=> success and */
               /*(add_count,don't continue, don't add_count)*/
               /* if add_count then sgnl added to array */
     DEVSUPFUN     special_linconv;
};

/* control block for callback*/
struct callback {
     void (*callback)();
     int priority;
     struct dbAddr dbAddr;
     WDOG_ID wd_id;
     void (*process)();
};

void callbackRequest();
void monitor();

void convert();
long  add_count();
long  clear_histogram();

static void wdCallback(pcallback)
     struct callback *pcallback;
{
     struct histogramRecord *phistogram=(struct histogramRecord *)(pcallback->dbAddr.precord);
     float         wait_time;

     /* force post events for any count change */
     if(phistogram->mcnt>0){
          dbScanLock((struct dbCommon *)phistogram);
          tsLocalTime(&phistogram->time);
          db_post_events(phistogram,&phistogram->bptr,DBE_VALUE);
          phistogram->mcnt=0;
          dbScanUnlock((struct dbCommon *)phistogram);
     }

     if(phistogram->sdel>0) {
          /* start new watchdog timer on monitor */
          wait_time = (float)(phistogram->sdel * vxTicksPerSecond);
          wdStart(pcallback->wd_id,wait_time,callbackRequest,(int)pcallback);
     }

     return;
}

static long init_record(phistogram)
     struct histogramRecord     *phistogram;
{
     struct histogramdset *pdset;
     long status;
     struct callback *pcallback=(struct callback *)(phistogram->wdog);
     float         wait_time;
     void (*process)();

     /* This routine may get called twice. Once by cvt_dbaddr. Once by iocInit*/

     if(phistogram->wdog==NULL && phistogram->sdel!=0) {
          /* initialize a watchdog timer */
          pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
          phistogram->wdog = (caddr_t)pcallback;
          pcallback->callback = wdCallback;
          pcallback->priority = priorityLow;
          if(dbNameToAddr(phistogram->name,&(pcallback->dbAddr))) {
               logMsg("dbNameToAddr failed in init_record for recHistogram\n");
               exit(1);
          }
          pcallback->wd_id = wdCreate();
          pcallback->process = process;
 
          /* start new watchdog timer on monitor */
          wait_time = (float)(phistogram->sdel * vxTicksPerSecond);
          wdStart(pcallback->wd_id,wait_time,callbackRequest,(int)pcallback);
     }

     /* allocate space for histogram array */
     if(phistogram->bptr==NULL) {
          if(phistogram->nelm<=0) phistogram->nelm=1;
          phistogram->bptr = (unsigned long *)calloc(phistogram->nelm,sizeof(long));
     }

     /* calulate width of array element */
     phistogram->wdth=(phistogram->ulim-phistogram->llim)/phistogram->nelm;

     if(!(pdset = (struct histogramdset *)(phistogram->dset))) {
          recGblRecordError(S_dev_noDSET,phistogram,"histogram: init_record");
          return(S_dev_noDSET);
     }
     /* must have read_histogram function defined */
     if( (pdset->number < 6) || (pdset->read_histogram == NULL) ) {
          recGblRecordError(S_dev_missingSup,phistogram,"histogram: init_record");
          return(S_dev_missingSup);
         }

     /* call device support init_record */
     if( pdset->init_record ) {
          if((status=(*pdset->init_record)(phistogram,process))) return(status);
     }
     return(0);
}

static long process(phistogram)
     struct histogramRecord     *phistogram;
{
     struct histogramdset     *pdset = (struct histogramdset *)(phistogram->dset);
     long           status;

     if( (pdset==NULL) || (pdset->read_histogram==NULL) ) {
          phistogram->pact=TRUE;
          recGblRecordError(S_dev_missingSup,phistogram,"read_histogram");
          return(S_dev_missingSup);
     }

     /*pact must not be set true until read_histogram is called*/
     status=(*pdset->read_histogram)(phistogram);
     phistogram->pact = TRUE;

     /* status is one if an asynchronous record is being processed*/
     if (status==1) return(0);

     tsLocalTime(&phistogram->time);

     if(status==0)add_count(phistogram);
     else if(status==2)status=0;

     /* check event list */
     monitor(phistogram);

     /* process the forward scan link record */
     if (phistogram->flnk.type==DB_LINK) dbScanPassive(((struct dbAddr *)phistogram->flnk.value.db_link.pdbAddr)->precord);

     phistogram->pact=FALSE;
     return(status);
}

static long special(paddr,after)
     struct dbAddr *paddr;
     int           after;
{
     struct histogramRecord   *phistogram = (struct histogramRecord *)(paddr->precord);
     struct histogramdset      *pdset = (struct histogramdset *) (phistogram->dset);
     int                special_type = paddr->special;

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
          phistogram->wdth=(phistogram->ulim-phistogram->llim)/phistogram->nelm;
          clear_histogram(phistogram);
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
     short           stat,sevr,nsta,nsev;
     struct callback *pcallback=(struct callback *)(phistogram->dpvt);
 
     /* get previous stat and sevr  and new stat and sevr*/
     stat=phistogram->stat;
     sevr=phistogram->sevr;
     nsta=phistogram->nsta;
     nsev=phistogram->nsev;
     /*set current stat and sevr*/
     phistogram->stat = nsta;
     phistogram->sevr = nsev;
     phistogram->nsta = 0;
     phistogram->nsev = 0;
 
     /* Flags which events to fire on the value field */
     monitor_mask = 0;
 
     /* alarm condition changed this scan */
     if (stat!=nsta || sevr!=nsev) {
          /* post events for alarm condition change*/
          monitor_mask = DBE_ALARM;
          /* post stat and nsev fields */
          db_post_events(phistogram,&phistogram->stat,DBE_VALUE);
          db_post_events(phistogram,&phistogram->sevr,DBE_VALUE);
     }

     /* post events for count change */
     if(phistogram->mcnt>phistogram->mdel){
          /* post events for count change */
          monitor_mask |= DBE_VALUE;
          /* reset counts since monitor */
          phistogram->mcnt = 0;
     }
     /* send out monitors connected to the value field */
     if(monitor_mask) db_post_events(phistogram,phistogram->bptr,monitor_mask);

     return;
}

static long get_value(phistogram,pvdes)
    struct histogramRecord *phistogram;
    struct valueDes     *pvdes;
{

    pvdes->no_elements=phistogram->nelm;
    (unsigned long *)(pvdes->pvalue) = phistogram->bptr;
    pvdes->field_type = DBF_ULONG;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;

    /* This may get called before init_record. If so just call it*/
    if(phistogram->bptr==NULL) init_record(phistogram);
    paddr->pfield = (caddr_t)(phistogram->bptr);
    paddr->no_elements = phistogram->nelm;
    paddr->field_type = DBF_ULONG;
    paddr->field_size = sizeof(long);
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
     double            sgnl;

     if(phistogram->csta==FALSE) return(0);

     if(phistogram->llim >= phistogram->ulim) {
               if (phistogram->nsev<VALID_ALARM) {
                    phistogram->stat = SOFT_ALARM;
                    phistogram->sevr = VALID_ALARM;
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
