/* recPulseTrain.c */
/* share/src/rec $Id$ */

/* recPulser.c - Record Support Routines for PulseTrain records */
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
 * .01  mm-dd-yy        iii     Comment
 */ 

#include     <vxWorks.h>
#include     <types.h>
#include     <stdioLib.h>
#include     <lstLib.h>

#include        <alarm.h>
#include     <dbAccess.h>
#include     <dbDefs.h>
#include     <dbFldTypes.h>
#include     <devSup.h>
#include     <errMdef.h>
#include     <link.h>
#include     <recSup.h>
#include     <pulseTrainRecord.h>

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
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset pulseTrainRSET={
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


struct ptdset { /* pulseTrain input dset */
     long          number;
     DEVSUPFUN     dev_report;
     DEVSUPFUN     init;
     DEVSUPFUN     init_record; /*returns: (-1,0)=>(failure,success)*/
     DEVSUPFUN     get_ioint_info;
     DEVSUPFUN     write_pt;/*(-1,0,1)=>(failure,success,don't Continue*/
};
void monitor();

static long init_record(ppt)
    struct pulseTrainRecord     *ppt;
{
    struct ptdset *pdset;
    long status=0;

    /* must have device support */
    if(!(pdset = (struct ptdset *)(ppt->dset))) {
         recGblRecordError(S_dev_noDSET,ppt,"pt: init_record");
         return(S_dev_noDSET);
    }
    /* get the gate value if gsrc is a constant*/
    if (ppt->gsrc.type == CONSTANT ){
         ppt->gate = ppt->gsrc.value.value;
    }

    /* must have write_pt functions defined */
    if( (pdset->number < 5) || (pdset->write_pt == NULL) ) {
         recGblRecordError(S_dev_missingSup,ppt,"pt: write_pt");
         return(S_dev_missingSup);
    }
    /* call device support init_record */
    if( pdset->init_record ) {
         if((status=(*pdset->init_record)(ppt,process))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr     *paddr;
{
    struct pulseTrainRecord     *ppt=(struct pulseTrainRecord *)(paddr->precord);
    struct ptdset     *pdset = (struct ptdset *)(ppt->dset);
    long           status=0;
    long             options,nRequest;

    /* must have  write_pt functions defined */
    if( (pdset==NULL) || (pdset->write_pt==NULL) ) {
         ppt->pact=TRUE;
         recGblRecordError(S_dev_missingSup,ppt,"write_pt");
         return(S_dev_missingSup);
    }

    /* get gate value when gsrc is a DB_LINK  */
    if (!ppt->pact) {
         if (ppt->gsrc.type == DB_LINK){
              options=0;
              nRequest=1;
              ppt->pact = TRUE;
              status=dbGetLink(&ppt->gsrc.value.db_link,ppt,DBR_SHORT,
                   &ppt->gate,&options,&nRequest);
              ppt->pact = FALSE;
              if(status!=0) {
                  if (ppt->nsev<VALID_ALARM) {
                      ppt->nsta = LINK_ALARM;
                      ppt->nsev = VALID_ALARM;
                  }
              }
         }
     }

     if (status==0) status=(*pdset->write_pt)(ppt); /* write the new value */

     ppt->pact = TRUE;

     /* status is one if an asynchronous record is being processed*/
     if (status==1) return(0);

     ppt->udf=FALSE;
     tsLocalTime(&ppt->time);

     /* check event list */
     monitor(ppt);

     /* process the forward scan link record */
     if (ppt->flnk.type==DB_LINK) dbScanPassive(ppt->flnk.value.db_link.pdbAddr);

     ppt->pact=FALSE;
     return(status);
}

static long get_value(ppt,pvdes)
    struct pulseTrainRecord             *ppt;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_SHORT;
    pvdes->no_elements=1;
    (short *)pvdes->pvalue = &ppt->val;
    return(0);
}


static void monitor(ppt)
    struct pulseTrainRecord             *ppt;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    stat=ppt->stat;
    sevr=ppt->sevr;
    nsta=ppt->nsta;
    nsev=ppt->nsev;
    /*set current stat and sevr*/
    ppt->stat = nsta;
    ppt->sevr = nsev;
    ppt->nsta = 0;
    ppt->nsev = 0;

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
         /* post events for alarm condition change*/
         monitor_mask = DBE_ALARM;
         /* post stat and nsev fields */
         db_post_events(ppt,&ppt->stat,DBE_VALUE);
         db_post_events(ppt,&ppt->sevr,DBE_VALUE);
    }

    monitor_mask |= (DBE_VALUE | DBE_LOG);
    db_post_events(ppt,&ppt->val,monitor_mask);
    db_post_events(ppt,&ppt->per,monitor_mask);
    db_post_events(ppt,&ppt->dcy,monitor_mask);

    return;
}
