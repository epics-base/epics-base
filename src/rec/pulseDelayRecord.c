/* recPulseDelay.c */
/* base/src/rec  $Id$ */

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
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .04  02-28-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not status
 * .06  04-18-92        jba     removed process from dev init_record parms
 * .07  06-02-92        jba     changed graphic/control limits for dly,odly,wide,owid
 * .08  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .09  09-16-92        jba     replaced code with calls to recGblGetLinkvalue
 * .10  10-20-92        jbk     added get_enum funcs, allowed defaults in fields
 */ 

#include     <vxWorks.h>
#include     <types.h>
#include     <stdioLib.h>
#include     <lstLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include        <alarm.h>
#include     <dbAccess.h>
#include     <dbEvent.h>
#include     <dbFldTypes.h>
#include     <devSup.h>
#include     <errMdef.h>
#include     <recSup.h>
#define GEN_SIZE_OFFSET
#include     <pulseDelayRecord.h>
#undef  GEN_SIZE_OFFSET

/* defines used in special routine */

#define DLY_FIELD	0x0001
#define WIDE_FIELD	0x0002
#define STV_FIELD	0x0004
#define GATE_FIELD	0x0008
#define HTS_FIELD	0x0010
 
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
static long get_enum_str();
static long get_enum_strs();
static long put_enum_str();
static long get_graphic_double();
static long get_control_double();
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
     DEVSUPFUN     write_pd;/*(-1,0)=>(failure,success*/
     DEVSUPFUN     get_enum;
};
static void monitor();

static long init_record(ppd,pass)
    struct pulseDelayRecord	*ppd;
    int pass;
{
    struct pddset *pdset;
    long status=0;

    if (pass==0) return(0);

    /* must have device support */
    if(!(pdset = (struct pddset *)(ppd->dset))) {
         recGblRecordError(S_dev_noDSET,(void *)ppd,"pd: init_record");
         return(S_dev_noDSET);
    }

    /* must have write_pd functions defined */
    if( (pdset->number < 5) || (pdset->write_pd == NULL) ) {
         recGblRecordError(S_dev_missingSup,(void *)ppd,"pd: write_pd");
         return(S_dev_missingSup);
    }

    /* get the soft trigger value if stl is a constant*/
    if(ppd->stl.type==CONSTANT)
    {
	recGblInitConstantLink(&ppd->stl,DBF_USHORT,&ppd->stv);
    }
 
    /* get the soft gate value if glnk is a constant*/
    if(ppd->glnk.type==CONSTANT)
    {
	recGblInitConstantLink(&ppd->glnk,DBF_USHORT,&ppd->gate);
    }
 
    /* call device support init_record */
    if( pdset->init_record ) {
         if((status=(*pdset->init_record)(ppd))) return(status);
    }

    ppd->pfld=0; /* clear the pfld */

	if(ppd->val)
		ppd->pfld|=0x0f00;
	else
		ppd->pfld&=0x00ff;

    return(0);
}

static long process(ppd)
    struct pulseDelayRecord     *ppd;
{
    struct pddset     *pdset = (struct pddset *)(ppd->dset);
    long           status=0;
    unsigned char    pact=ppd->pact;

    /* must have  write_pd functions defined */
    if( (pdset==NULL) || (pdset->write_pd==NULL) ) {
         ppd->pact=TRUE;
         recGblRecordError(S_dev_missingSup,(void *)ppd,"write_pd");
         return(S_dev_missingSup);
    }

    if(!ppd->pact)
    {
    	/* get soft trigger value */
        status=dbGetLink(&(ppd->stl),DBR_SHORT,&ppd->stv,0,0);
 
    	/* get soft gate value */
        status=dbGetLink(&(ppd->glnk),DBR_SHORT,&ppd->gate,0,0);

    }

    if (status==0) status=(*pdset->write_pd)(ppd); /* write the new value */

    /* reset field causing processing parameter */
    /* used to be set to zero, use a bit to detect val field change */
    ppd->pfld&=0x0f00;
 
    /* check if device support set pact */
    if ( !pact && ppd->pact ) return(0);
    ppd->pact = TRUE;
    ppd->udf=FALSE;
    recGblGetTimeStamp(ppd);
    monitor(ppd);
    recGblFwdLink(ppd);
    ppd->pact=FALSE;
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long          *precision;
{
    struct pulseDelayRecord    *ppd=(struct pulseDelayRecord *)paddr->precord;

    *precision = ppd->prec;
    if(paddr->pfield == (void *)&ppd->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

/*---------------------------------------------------------------------
        This routine is used to set the pfld element to true if
        the field causing the processing is dly, hts, or stv.
-----------------------------------------------------------------------*/

static long special(paddr,after)
        struct dbAddr *paddr;
        int after;
{
        struct pulseDelayRecord *pd=(struct pulseDelayRecord *)paddr->precord;
 
        /* pre-processing mode */
        if(!after) return(0);
 
        if(paddr->pfield==&(pd->dly))
	    pd->pfld|=DLY_FIELD;
        else if(paddr->pfield==&(pd->wide))
	    pd->pfld|=WIDE_FIELD;
        else if(paddr->pfield==&(pd->stv))
	    pd->pfld|=STV_FIELD;
        else if(paddr->pfield==&(pd->hts))
	    pd->pfld|=HTS_FIELD;
        else if(paddr->pfield==&(pd->gate))
	    pd->pfld|=GATE_FIELD;
 
        return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct pulseDelayRecord     *ppd=(struct pulseDelayRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppd->val
    || paddr->pfield==(void *)&ppd->dly
    || paddr->pfield==(void *)&ppd->wide
    || paddr->pfield==(void *)&ppd->odly
    || paddr->pfield==(void *)&ppd->owid){
         pgd->upper_disp_limit = ppd->hopr;
         pgd->lower_disp_limit = ppd->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_enum_str(struct dbAddr *paddr,char *p)
{
    struct pulseDelayRecord *ppd=(struct pulseDelayRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppd->hts)
    {
	sprintf(p,"%d",ppd->hts);
    }
    else
    {
	strcpy(p,"No string");
    }

    return(0);
}

static long get_enum_strs(struct dbAddr *paddr,struct dbr_enumStrs *p)
{
    struct pulseDelayRecord *ppd=(struct pulseDelayRecord *)paddr->precord;
    struct pddset *pdset;
	     
    pdset=(struct pddset *)(ppd->dset);

    if(paddr->pfield==(void *)&ppd->hts)
    {
   	if( (pdset->number >= 6) && (pdset->get_enum != NULL) )
    	{
     		(*pdset->get_enum)(paddr,p);
	}
	else
	{
		strcpy(p->strs[0],"none available");
		p->no_str=1;
	}
    }
    else
    {
	strcpy(p->strs[0],"No string");
	p->no_str=1;
    }

    return(0);
}

static long put_enum_str(struct dbAddr *paddr,char *p)
{
    struct pulseDelayRecord *ppd=(struct pulseDelayRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppd->hts)
    {
	if(sscanf(p,"%hu",&ppd->hts)<=0)
		return(S_db_badChoice);
    }
    else
    {
	return(S_db_badChoice);
    }

    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct pulseDelayRecord     *ppd=(struct pulseDelayRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppd->val
    || paddr->pfield==(void *)&ppd->dly
    || paddr->pfield==(void *)&ppd->wide){
         pcd->upper_ctrl_limit = ppd->hopr;
         pcd->lower_ctrl_limit = ppd->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(ppd)
    struct pulseDelayRecord             *ppd;
{
    unsigned short  monitor_mask;

    monitor_mask = recGblResetAlarms(ppd);
    monitor_mask |= (DBE_VALUE | DBE_LOG);

	/* temp change, keep old val value in pfld so ascii files do not
	need to be changed, keep in 0x0f00 position */

    if( ( (ppd->pfld & 0x0f00) && !(ppd->val) )
		|| ( !(ppd->pfld & 0x0f00) && (ppd->val) ) )
    {
    	db_post_events(ppd,&ppd->val,monitor_mask);
		if(ppd->val)
        	ppd->pfld|=0x0f00;
		else
        	ppd->pfld&=0x00ff;
    }

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
