/* recPulseCounter.c */
/* share/src/rec $Id$ */

/* recPulseCounter.c - Record Support Routines for PulseCounter records */
/*
 * Author:      Janet Anderson
 * Date:        10-17-91
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
 * .01  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 */ 

#include     <vxWorks.h>
#include     <types.h>
#include     <stdioLib.h>
#include     <lstLib.h>

#include        <alarm.h>
#include     <dbDefs.h>
#include     <dbAccess.h>
#include     <dbRecDes.h>
#include     <dbFldTypes.h>
#include     <devSup.h>
#include     <errMdef.h>
#include     <recSup.h>
#include     <pulseCounterRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
#define special NULL
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
#define get_alarm_double NULL

struct rset pulseCounterRSET={
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


struct pcdset { /* pulseCounter input dset */
     long          number;
     DEVSUPFUN     dev_report;
     DEVSUPFUN     init;
     DEVSUPFUN     init_record; /*returns: (-1,0)=>(failure,success)*/
     DEVSUPFUN     get_ioint_info;
     DEVSUPFUN     cmd_pc;/*(-1,0,1)=>(failure,success,don't Continue*/
};

/* def for gsrc field */
#define SOFTWARE 1

/* defs for counter commands */
#define CTR_READ        0
#define CTR_CLEAR       1
#define CTR_START       2
#define CTR_STOP        3
#define CTR_SETUP       4

void monitor();

static long init_record(ppc)
    struct pulseCounterRecord     *ppc;
{
    struct pcdset *pdset;
    long status=0;

    /* must have device support */
    if(!(pdset = (struct pcdset *)(ppc->dset))) {
         recGblRecordError(S_dev_noDSET,ppc,"pc: init_record");
         return(S_dev_noDSET);
    }
    /* get the gate value if sgl is a constant*/
    if (ppc->sgl.type == CONSTANT && ppc->gsrc == SOFTWARE){
         ppc->sgv = ppc->sgl.value.value;
    }

    /* must have cmd_pc functions defined */
    if( (pdset->number < 5) || (pdset->cmd_pc == NULL) ) {
         recGblRecordError(S_dev_missingSup,ppc,"pc: cmd_pc");
         return(S_dev_missingSup);
    }
    /* call device support init_record */
    if( pdset->init_record ) {
         if((status=(*pdset->init_record)(ppc,process))) return(status);
    }
    return(0);
}

static long process(ppc)
    struct pulseCounterRecord        *ppc;
{
    struct pcdset     *pdset = (struct pcdset *)(ppc->dset);
    long           status=0;
    long             options,nRequest;
    unsigned short   save;

    /* must have  cmd_pc functions defined */
    if( (pdset==NULL) || (pdset->cmd_pc==NULL) ) {
         ppc->pact=TRUE;
         recGblRecordError(S_dev_missingSup,ppc,"cmd_pc");
         return(S_dev_missingSup);
    }

    /* get soft gate value when sgl is a DB_LINK and gsrc from Software */
    if (!ppc->pact && ppc->gsrc == SOFTWARE){
         if (ppc->sgl.type == DB_LINK){
              options=0;
              nRequest=1;
              ppc->pact = TRUE;
              status=dbGetLink(&ppc->sgl.value.db_link,(struct dbCommon *)ppc,DBR_SHORT,
                   &ppc->sgv,&options,&nRequest);
              ppc->pact = FALSE;
              if(status!=0) {
                  recGblSetSevr(ppc,LINK_ALARM,VALID_ALARM);
              }
         }
         if(status=0){
              if(ppc->sgv != ppc->osgv){ /* soft gate changed */
                   save=ppc->cmd;
                   if(ppc->sgv!=0){
                        ppc->cmd=CTR_START;
                   } else {
                        ppc->cmd=CTR_STOP;
                   }
                   status=(*pdset->cmd_pc)(ppc);
                   ppc->cmd=save;
                   ppc->osgv=ppc->sgv;
                   if(status!=0) {
                       recGblSetSevr(ppc,SOFT_ALARM,VALID_ALARM);
                   }
              }
         }
     }

     if(ppc->cmd>0){
          ppc->scmd=ppc->cmd;
          status=(*pdset->cmd_pc)(ppc);
          ppc->cmd=CTR_READ;
     }
     if (status==0) status=(*pdset->cmd_pc)(ppc);

     ppc->pact = TRUE;

     /* status is one if an asynchronous record is being processed*/
     if (status==1) return(0);

     ppc->udf=FALSE;
     tsLocalTime(&ppc->time);

     /* check event list */
     monitor(ppc);

     /* process the forward scan link record */
     if (ppc->flnk.type==DB_LINK) dbScanPassive(((struct dbAddr *)ppc->flnk.value.db_link.pdbAddr)->precord);

     ppc->pact=FALSE;
     return(status);
}

static long get_value(ppc,pvdes)
    struct pulseCounterRecord             *ppc;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_ULONG;
    pvdes->no_elements=1;
    (short *)pvdes->pvalue = &ppc->val;
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct pulseCounterRecord    *ppc=(struct pulseCounterRecord *)paddr->precord;
    struct fldDes               *pfldDes=(struct fldDes *)(paddr->pfldDes);

    if(((void *)(paddr->pfield))==((void *)&(ppc->clks))){
         pgd->upper_disp_limit = (double)pfldDes->range2.value.short_value;
         pgd->lower_disp_limit = (double)pfldDes->range1.value.short_value;
         return(0);
    }
    if(((void *)(paddr->pfield))==((void *)&(ppc->gate))){
         pgd->upper_disp_limit = (double)pfldDes->range2.value.short_value;
         pgd->lower_disp_limit = (double)pfldDes->range1.value.short_value;
         return(0);
    }
    pgd->upper_disp_limit = ppc->hopr;
    pgd->lower_disp_limit = ppc->lopr;

    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct pulseCounterRecord     *ppc=(struct pulseCounterRecord *)paddr->precord;
    struct fldDes               *pfldDes=(struct fldDes *)(paddr->pfldDes);

    if(((void *)(paddr->pfield))==((void *)&(ppc->clks))){
         pcd->upper_ctrl_limit = (double)pfldDes->range2.value.short_value;
         pcd->lower_ctrl_limit = (double)pfldDes->range1.value.short_value;
         return(0);
    }
    if(((void *)(paddr->pfield))==((void *)&(ppc->gate))){
         pcd->upper_ctrl_limit = (double)pfldDes->range1.value.short_value;
         pcd->lower_ctrl_limit = (double)pfldDes->range2.value.short_value;
         return(0);
    }
    pcd->upper_ctrl_limit = ppc->hopr;
    pcd->lower_ctrl_limit = ppc->lopr;

    return(0);
}

static void monitor(ppc)
    struct pulseCounterRecord             *ppc;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    recGblResetSevr(ppc,stat,sevr,nsta,nsev);

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
         /* post events for alarm condition change*/
         monitor_mask = DBE_ALARM;
         /* post stat and nsev fields */
         db_post_events(ppc,&ppc->stat,DBE_VALUE);
         db_post_events(ppc,&ppc->sevr,DBE_VALUE);
    }

    monitor_mask |= (DBE_VALUE | DBE_LOG);
    db_post_events(ppc,&ppc->val,monitor_mask);
    if (ppc->scmd != ppc->cmd) {
          db_post_events(ppc,&ppc->scmd,monitor_mask);
          ppc->scmd=ppc->cmd;
    }
    return;
}
