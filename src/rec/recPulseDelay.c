/* recPulseDelay.c */
/* share/src/rec $Id$ */

/* recPulser.c - Record Support Routines for PulseDelay records */
/*
 * Author:      Janet Anderson
 * Date:     6/21/91
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
 */ 

#include     <vxWorks.h>
#include     <types.h>
#include     <stdioLib.h>
#include     <lstLib.h>

#include        <alarm.h>
#include     <dbAccess.h>
#include     <dbDefs.h>
#include     <dbRecDes.h>
#include     <dbFldTypes.h>
#include     <devSup.h>
#include     <errMdef.h>
#include     <link.h>
#include     <recSup.h>
#include     <pulseDelayRecord.h>

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
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
#define get_alarm_double NULL

struct rset pulseDelayRSET={
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


struct pddset { /* pulseDelay input dset */
     long          number;
     DEVSUPFUN     dev_report;
     DEVSUPFUN     init;
     DEVSUPFUN     init_record; /*returns: (-1,0)=>(failure,success)*/
     DEVSUPFUN     get_ioint_info;
     DEVSUPFUN     write_pd;/*(-1,0,1)=>(failure,success,don't Continue*/
};
void monitor();

static long init_record(ppd)
    struct pulseDelayRecord     *ppd;
{
    struct pddset *pdset;
    long status=0;

    /* must have device support */
    if(!(pdset = (struct pddset *)(ppd->dset))) {
         recGblRecordError(S_dev_noDSET,ppd,"pd: init_record");
         return(S_dev_noDSET);
    }

    /* must have write_pd functions defined */
    if( (pdset->number < 5) || (pdset->write_pd == NULL) ) {
         recGblRecordError(S_dev_missingSup,ppd,"pd: write_pd");
         return(S_dev_missingSup);
    }
    /* call device support init_record */
    if( pdset->init_record ) {
         if((status=(*pdset->init_record)(ppd,process))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr     *paddr;
{
    struct pulseDelayRecord     *ppd=(struct pulseDelayRecord *)(paddr->precord);
    struct pddset     *pdset = (struct pddset *)(ppd->dset);
    long           status=0;
    long             options,nRequest;

    /* must have  write_pd functions defined */
    if( (pdset==NULL) || (pdset->write_pd==NULL) ) {
         ppd->pact=TRUE;
         recGblRecordError(S_dev_missingSup,ppd,"write_pd");
         return(S_dev_missingSup);
    }

     if (status==0) status=(*pdset->write_pd)(ppd); /* write the new value */

     ppd->pact = TRUE;

     /* status is one if an asynchronous record is being processed*/
     if (status==1) return(0);

     ppd->udf=FALSE;
     tsLocalTime(&ppd->time);

     /* check event list */
     monitor(ppd);

     /* process the forward scan link record */
     if (ppd->flnk.type==DB_LINK) dbScanPassive(ppd->flnk.value.db_link.pdbAddr);

     ppd->pact=FALSE;
     return(status);
}

static long get_value(ppd,pvdes)
    struct pulseDelayRecord             *ppd;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_SHORT;
    pvdes->no_elements=1;
    (short *)pvdes->pvalue = &ppd->val;
    return(0);
}
static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long          *precision;
{
    struct pulseDelayRecord    *ppd=(struct pulseDelayRecord *)paddr->precord;

    *precision = ppd->prec;
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct pulseDelayRecord     *ppd=(struct pulseDelayRecord *)paddr->precord;
    struct fldDes               *pfldDes=(struct fldDes *)(paddr->pfldDes);

    if(((void *)(paddr->pfield))==((void *)&(ppd->clks))){
         pgd->upper_disp_limit = (double)pfldDes->range2.value.short_value;
         pgd->lower_disp_limit = (double)pfldDes->range1.value.short_value;
    } else {
         pgd->upper_disp_limit = ppd->hopr;
         pgd->lower_disp_limit = ppd->lopr;
    }

    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct pulseDelayRecord     *ppd=(struct pulseDelayRecord *)paddr->precord;
    struct fldDes               *pfldDes=(struct fldDes *)(paddr->pfldDes);

    if(((void *)(paddr->pfield))==((void *)&(ppd->clks))){
         pcd->upper_ctrl_limit = (double)pfldDes->range2.value.short_value;
         pcd->lower_ctrl_limit = (double)pfldDes->range1.value.short_value;
    } else {
         pcd->upper_ctrl_limit = ppd->hopr;
         pcd->lower_ctrl_limit = ppd->lopr;
    }

    return(0);
}

static void monitor(ppd)
    struct pulseDelayRecord             *ppd;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    recGblResetSevr(ppd,stat,sevr,nsta,nsev);

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
         /* post events for alarm condition change*/
         monitor_mask = DBE_ALARM;
         /* post stat and nsev fields */
         db_post_events(ppd,&ppd->stat,DBE_VALUE);
         db_post_events(ppd,&ppd->sevr,DBE_VALUE);
    }

    monitor_mask |= (DBE_VALUE | DBE_LOG);
    db_post_events(ppd,&ppd->val,monitor_mask);
    if(ppd->odly != ppd->dly){
         db_post_events(ppd,&ppd->dly,monitor_mask);
         ppd->odly=ppd->dly;
    }
    if(ppd->owid != ppd->wide){
         db_post_events(ppd,&ppd->wide,monitor_mask);
         ppd->owid=ppd->wide;
    }

    return;
}
