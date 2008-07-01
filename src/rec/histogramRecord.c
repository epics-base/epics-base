/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recHistogram.c - Record Support Routines for Histogram records */
/*
 *      Author:      Janet Anderson
 *      Date:        5/20/91
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
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "histogramRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(histogramRecord *, int);
static long process(histogramRecord *);
static long special(DBADDR *, int);
#define get_value NULL
static long cvt_dbaddr(DBADDR *);
static long get_array_info(DBADDR *, long *, long *);
#define  put_array_info NULL
#define get_units NULL
static long get_precision(DBADDR *paddr,long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_alarm_double NULL
static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd);
static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd);

rset histogramRSET={
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
epicsExportAddress(rset,histogramRSET);

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
typedef struct myCallback {
     CALLBACK callback;
     histogramRecord *phistogram;
}myCallback;

static long add_count(histogramRecord *);
static long clear_histogram(histogramRecord *);
static void monitor(histogramRecord *);
static long readValue(histogramRecord *);

static void wdogCallback(CALLBACK *arg)
{
     myCallback *pcallback;
     histogramRecord *phistogram;

     callbackGetUser(pcallback,arg);
     phistogram = pcallback->phistogram;
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
          callbackRequestDelayed(&pcallback->callback,(double)phistogram->sdel);
     }

     return;
}
static long wdogInit(histogramRecord *phistogram)
{
     myCallback *pcallback;

     if(phistogram->wdog==NULL && phistogram->sdel>0) {
          /* initialize a watchdog timer */
          pcallback = (myCallback *)(calloc(1,sizeof(myCallback)));
          pcallback->phistogram = phistogram;
	  if(!pcallback) return -1;
          callbackSetCallback(wdogCallback,&pcallback->callback);
          callbackSetUser(pcallback,&pcallback->callback);
          callbackSetPriority(priorityLow,&pcallback->callback);
          phistogram->wdog = (void *)pcallback;
     }

     if (!phistogram->wdog) return -1;
     pcallback = (myCallback *)phistogram->wdog;
     if(!pcallback) return -1;
     if( phistogram->sdel>0) {
         /* start new watchdog timer on monitor */
         callbackRequestDelayed(&pcallback->callback,(double)phistogram->sdel);
     }
     return 0;
}

static long init_record(histogramRecord *phistogram, int pass)
{
     struct histogramdset *pdset;
     long status;

     if (pass==0){

          /* allocate space for histogram array */
          if(phistogram->bptr==NULL) {
               if(phistogram->nelm<=0) phistogram->nelm=1;
               phistogram->bptr = (epicsUInt32 *)calloc(phistogram->nelm,sizeof(epicsUInt32));
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

static long process(histogramRecord *phistogram)
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

static long special(DBADDR *paddr,int after)
{
     histogramRecord   *phistogram = (histogramRecord *)(paddr->precord);
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

static void monitor(histogramRecord *phistogram)
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

static long cvt_dbaddr(DBADDR *paddr)
{
    histogramRecord *phistogram=(histogramRecord *)paddr->precord;

    paddr->pfield = (void *)(phistogram->bptr);
    paddr->no_elements = phistogram->nelm;
    paddr->field_type = DBF_ULONG;
    paddr->field_size = sizeof(epicsUInt32);
    paddr->dbr_field_type = DBF_ULONG;
    return(0);
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    histogramRecord *phistogram=(histogramRecord *)paddr->precord;

    *no_elements =  phistogram->nelm;
    *offset = 0;
    return(0);
}

static long add_count(histogramRecord *phistogram)
{
     double            temp;
     epicsUInt32       *pdest;
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
     if (*pdest == (epicsUInt32) ULONG_MAX) *pdest=0.0;
     (*pdest)++;
     phistogram->mcnt++;

     return(0);
}

static long clear_histogram(histogramRecord *phistogram)
{
     int    i;

     for (i=0;i<=phistogram->nelm-1;i++)
           *(phistogram->bptr+i)=0.0;
     phistogram->mcnt=phistogram->mdel+1;
     phistogram->udf=FALSE;

     return(0);
}

static long readValue(histogramRecord *phistogram)
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

static long get_precision(DBADDR *paddr,long *precision)
{
    histogramRecord *phistogram=(histogramRecord *)paddr->precord;
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
static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    histogramRecord *phistogram=(histogramRecord *)paddr->precord;
    int     fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == histogramRecordBPTR){
        pgd->upper_disp_limit = phistogram->hopr;
        pgd->lower_disp_limit = phistogram->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    histogramRecord *phistogram=(histogramRecord *)paddr->precord;
    int     fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == histogramRecordBPTR){
        pcd->upper_ctrl_limit = phistogram->hopr;
        pcd->lower_ctrl_limit = phistogram->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
