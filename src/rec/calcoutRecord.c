/* calcout.c - Record Support Routines for calc with output records */
/*
 *   Author : Ned Arnold
 *   Based on recCalc.c, by ...
 *      Original Author: Julie Sander and Bob Dalesio
 *      Current  Author: Marty Kraimer
 *      Date:            7-27-87
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
 * .01  08-29-96	nda    Created from calcoutRecord.c for EPICS R3.13 
 * .02  09-13-96	nda    Original release for EPICS R3.13beta3
 * 
 *
 */



#include	<vxWorks.h>
#include        <stdlib.h>
#include        <stdarg.h>
#include        <stdio.h>
#include        <string.h>

#include        <tickLib.h>
#include        <wdLib.h>
#include        <sysLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbScan.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include        <callback.h>
#include        <taskwd.h>

#define GEN_SIZE_OFFSET
#include	<calcoutRecord.h>
#undef  GEN_SIZE_OFFSET
#include        <menuIvoa.h>

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
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

struct rset calcoutRSET={
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


/* To provide feedback to the user as to the connection status of the 
 * links (.INxV and .OUTV), the following algorithm has been implemented ...
 *
 * A new PV_LINK [either in init() or special()] is searched for using
 * dbNameToAddr. If local, it is so indicated. If not, a checkLinkCb
 * callback is scheduled to check the connectivity later using 
 * dbCaIsLinkConnected(). Anytime there are unconnected CA_LINKs, another
 * callback is scheduled. Once all connections are established, the CA_LINKs
 * are checked whenever the record processes. 
 *
 */

#define NO_CA_LINKS     0
#define CA_LINKS_ALL_OK 1
#define CA_LINKS_NOT_OK 2

struct rpvtStruct {
        CALLBACK  doOutCb;
        WDOG_ID   wd_id_0;
        CALLBACK  checkLinkCb;
        WDOG_ID   wd_id_1;
        short     wd_id_1_LOCK;
        short     caLinkStat; /* NO_CA_LINKS,CA_LINKS_ALL_OK,CA_LINKS_NOT_OK */
};

static void alarm();
static void monitor();
static int fetch_values();
static void execOutput();
static void doOutputCallback();
static void checkLinks();
static void checkLinksCallback();

int    calcoutRecDebug;

#define ARG_MAX 12


static long init_record(pcalc,pass)
    struct calcoutRecord	*pcalc;
    int pass;
{
    struct link *plink;
    int i;
    double *pvalue;
    unsigned short *plinkValid;
    short error_number;
    char rpbuf[184];

    struct dbAddr       *pAddr = 0;
    struct rpvtStruct   *prpvt;

    if (pass==0) {
        pcalc->rpvt = (void *)calloc(1, sizeof(struct rpvtStruct));
        return(0);
    }
    
    prpvt = (struct rpvtStruct *)pcalc->rpvt;

    plink = &pcalc->inpa;
    pvalue = &pcalc->a;
    plinkValid = &pcalc->inav;
    for(i=0; i<(ARG_MAX+1); i++, plink++, pvalue++, plinkValid++) {
        if (plink->type == CONSTANT) {
            /* Don't InitConstantLink the .OUT link */
            if(i<ARG_MAX) { 
                recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
                db_post_events(pcalc,pvalue,DBE_VALUE);
            }
            *plinkValid = calcoutINAV_CON;
        }
        /* see if the PV resides on this ioc */
        else if(!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
            *plinkValid = calcoutINAV_LOC;
        }
        /* pv is not on this ioc. Callback later for connection stat */
        else {
            *plinkValid = calcoutINAV_EXT_NC;
             prpvt->caLinkStat = CA_LINKS_NOT_OK;
        }
        db_post_events(pcalc,plinkValid,DBE_VALUE);
    }

    pcalc->clcv=postfix(pcalc->calc,rpbuf,&error_number);
    if(pcalc->clcv){
		recGblRecordError(S_db_badField,(void *)pcalc,
			"calcout: init_record: Illegal CALC field");
		return(S_db_badField);
    }
    else {
        memcpy(pcalc->rpcl,rpbuf,sizeof(pcalc->rpcl));
    }
    db_post_events(pcalc,&pcalc->clcv,DBE_VALUE);

    pcalc->oclv=postfix(pcalc->ocal,rpbuf,&error_number);
    if(pcalc->oclv){
		recGblRecordError(S_db_badField,(void *)pcalc,
			"calcout: init_record: Illegal OCAL field");
		return(S_db_badField);
    }
    else {
        memcpy(pcalc->orpc,rpbuf,sizeof(pcalc->orpc));
    }
    db_post_events(pcalc,&pcalc->oclv,DBE_VALUE);

    prpvt = (struct rpvtStruct *)pcalc->rpvt;
    callbackSetCallback(doOutputCallback, &prpvt->doOutCb);
    callbackSetPriority(pcalc->prio, &prpvt->doOutCb);
    callbackSetUser(pcalc, &prpvt->doOutCb);
    callbackSetCallback(checkLinksCallback, &prpvt->checkLinkCb);
    callbackSetPriority(0, &prpvt->checkLinkCb);
    callbackSetUser(pcalc, &prpvt->checkLinkCb);
    prpvt->wd_id_0 = wdCreate();
    prpvt->wd_id_1 = wdCreate();
    prpvt->wd_id_1_LOCK = 0;

    if(prpvt->caLinkStat == CA_LINKS_NOT_OK) {
        wdStart(prpvt->wd_id_1, 60, (FUNCPTR)callbackRequest,
                (int)(&prpvt->checkLinkCb));
        prpvt->wd_id_1_LOCK = 1;
    }

    return(0);
}

static long process(pcalc)
    struct calcoutRecord     *pcalc;
{
    struct rpvtStruct   *prpvt = (struct rpvtStruct *)pcalc->rpvt;
    int      wdDelay;
    short    doOutput = 0;

  
    if(!pcalc->pact) { 
	pcalc->pact = TRUE;

        /* if some links are CA, check connections */
        if(prpvt->caLinkStat != NO_CA_LINKS) {
            checkLinks(pcalc);
        }

	if(fetch_values(pcalc)==0) {
	    if((pcalc->clcv) ||
              (pcalc->clcv) ||
                calcPerform(&pcalc->a,&pcalc->val,pcalc->rpcl)) {
		recGblSetSevr(pcalc,CALC_ALARM,INVALID_ALARM);
	    } else pcalc->udf = FALSE;
	}
	recGblGetTimeStamp(pcalc);
	/* check for alarms */
	alarm(pcalc);

        /* check for output link execution */
        switch(pcalc->oopt) {
          case calcoutOOPT_Every_Time:
                doOutput = 1;
            break;
          case calcoutOOPT_On_Change:
            if(fabs(pcalc->pval - pcalc->val) > pcalc->mdel)  {
                doOutput = 1;
            }
            break;
          case calcoutOOPT_Transition_To_Zero:
            if((pcalc->pval != 0) && (pcalc->val == 0)) {
                doOutput = 1;
            }
            break;         
          case calcoutOOPT_Transition_To_Non_zero:
            if((pcalc->pval == 0) && (pcalc->val != 0)) {
                doOutput = 1;
            }
            break;
          case calcoutOOPT_When_Zero:
            if(!pcalc->val) {
                doOutput = 1;
            }
            break;
          case calcoutOOPT_When_Non_zero:
            if(pcalc->val) {
                doOutput = 1;
            }
            break;
          default:
            break;
        }
        pcalc->pval = pcalc->val;

        if(doOutput) {
            if(pcalc->odly > 0.0) {
                pcalc->dlya = 1;
                db_post_events(pcalc,&pcalc->dlya,DBE_VALUE);
                wdDelay = pcalc->odly * sysClkRateGet();
                callbackSetPriority(pcalc->prio, &prpvt->doOutCb);
                wdStart(prpvt->wd_id_0, wdDelay, (FUNCPTR)callbackRequest,
                        (int)(&prpvt->doOutCb));
            }
            else {
                execOutput(pcalc);
            }
        }

	/* check event list */
	monitor(pcalc);

        /* if no delay requested, finish processing */ 
        if(!pcalc->dlya) {
            /* process the forward scan link record */
            recGblFwdLink(pcalc);
            pcalc->pact = FALSE;
        }
    }
    else {
        execOutput(pcalc);
        pcalc->dlya = 0;
        db_post_events(pcalc,&pcalc->dlya,DBE_VALUE);

	/* process the forward scan link record */
        recGblFwdLink(pcalc);

        pcalc->pact = FALSE;
    }

    return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    struct calcoutRecord *pcalc = (struct calcoutRecord *)(paddr->precord);
    struct rpvtStruct   *prpvt = (struct rpvtStruct *)pcalc->rpvt;
    struct dbAddr       *pAddr = 0;
    short error_number;
    char rpbuf[184];
    int                 fieldIndex = dbGetFieldIndex(paddr);
    int                 lnkIndex;
    struct link         *plink;
    double              *pvalue;
    unsigned short      *plinkValid;

    if(!after) return(0);
    switch(fieldIndex) {
      case(calcoutRecordCALC):
        pcalc->clcv=postfix(pcalc->calc,rpbuf,&error_number);
        if(pcalc->clcv){
              recGblRecordError(S_db_badField,(void *)pcalc,
                        "calcout: special(): Illegal CALC field");
        }
        else {
            memcpy(pcalc->rpcl,rpbuf,sizeof(pcalc->rpcl));
        }
        db_post_events(pcalc,&pcalc->clcv,DBE_VALUE);
        return(0);
        break;

      case(calcoutRecordOCAL):
        pcalc->oclv=postfix(pcalc->ocal,rpbuf,&error_number);
        if(pcalc->oclv){
                recGblRecordError(S_db_badField,(void *)pcalc,
                        "calcout: special(): Illegal OCAL field");
        }
        else {
            memcpy(pcalc->orpc,rpbuf,sizeof(pcalc->orpc));
        }
        db_post_events(pcalc,&pcalc->oclv,DBE_VALUE);

        return(0);
        break;

      case(calcoutRecordINPA):
      case(calcoutRecordINPB):
      case(calcoutRecordINPC):
      case(calcoutRecordINPD):
      case(calcoutRecordINPE):
      case(calcoutRecordINPF):
      case(calcoutRecordINPG):
      case(calcoutRecordINPH):
      case(calcoutRecordINPI):
      case(calcoutRecordINPJ):
      case(calcoutRecordINPK):
      case(calcoutRecordINPL):
      case(calcoutRecordOUT):
        lnkIndex = fieldIndex - calcoutRecordINPA;
        plink   = &pcalc->inpa + lnkIndex;
        pvalue  = &pcalc->a    + lnkIndex;
        plinkValid = &pcalc->inav + lnkIndex;

        if (plink->type == CONSTANT) {
            if(fieldIndex != calcoutRecordOUT) {
                recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
                db_post_events(pcalc,pvalue,DBE_VALUE);
            }
            *plinkValid = calcoutINAV_CON;
        }
        /* see if the PV resides on this ioc */
        else if(!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
            *plinkValid = calcoutINAV_LOC;
        }
        /* pv is not on this ioc. Callback later for connection stat */
        else {
            *plinkValid = calcoutINAV_EXT_NC;
            /* DO_CALLBACK, if not already scheduled */
            if(!prpvt->wd_id_1_LOCK) {
                wdStart(prpvt->wd_id_1, 30, (FUNCPTR)callbackRequest,
                    (int)(&prpvt->checkLinkCb));
                prpvt->wd_id_1_LOCK = 1;
                prpvt->caLinkStat = CA_LINKS_NOT_OK;
            }
        }
        db_post_events(pcalc,plinkValid,DBE_VALUE);

        
        return(0);
        break;

      default:
	recGblDbaddrError(S_db_badChoice,paddr,"calc: special");
	return(S_db_badChoice);
    }
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct calcoutRecord	*pcalc=(struct calcoutRecord *)paddr->precord;

    strncpy(units,pcalc->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct calcoutRecord	*pcalc=(struct calcoutRecord *)paddr->precord;

    *precision = pcalc->prec;
    if(paddr->pfield == (void *)&pcalc->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct calcoutRecord	*pcalc=(struct calcoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val
    || paddr->pfield==(void *)&pcalc->hihi
    || paddr->pfield==(void *)&pcalc->high
    || paddr->pfield==(void *)&pcalc->low
    || paddr->pfield==(void *)&pcalc->lolo){
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
       return(0);
    } 

    if(paddr->pfield>=(void *)&pcalc->a && paddr->pfield<=(void *)&pcalc->l){
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&pcalc->la && paddr->pfield<=(void *)&pcalc->ll){
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
        return(0);
    }
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct calcoutRecord	*pcalc=(struct calcoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val
    || paddr->pfield==(void *)&pcalc->hihi
    || paddr->pfield==(void *)&pcalc->high
    || paddr->pfield==(void *)&pcalc->low
    || paddr->pfield==(void *)&pcalc->lolo){
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
       return(0);
    } 

    if(paddr->pfield>=(void *)&pcalc->a && paddr->pfield<=(void *)&pcalc->l){
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&pcalc->la && paddr->pfield<=(void *)&pcalc->ll){
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
        return(0);
    }
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct calcoutRecord	*pcalc=(struct calcoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val){
         pad->upper_alarm_limit = pcalc->hihi;
         pad->upper_warning_limit = pcalc->high;
         pad->lower_warning_limit = pcalc->low;
         pad->lower_alarm_limit = pcalc->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}


static void alarm(pcalc)
    struct calcoutRecord	*pcalc;
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(pcalc->udf == TRUE ){
 		recGblSetSevr(pcalc,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = pcalc->hihi; 
        lolo = pcalc->lolo; 
        high = pcalc->high;  
        low = pcalc->low;
	hhsv = pcalc->hhsv; 
        llsv = pcalc->llsv; 
        hsv = pcalc->hsv; 
        lsv = pcalc->lsv;
	val = pcalc->val; 
        hyst = pcalc->hyst; 
        lalm = pcalc->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(pcalc,HIHI_ALARM,pcalc->hhsv)) 
                    pcalc->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(pcalc,LOLO_ALARM,pcalc->llsv)) 
                    pcalc->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(pcalc,HIGH_ALARM,pcalc->hsv)) 
                    pcalc->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(pcalc,LOW_ALARM,pcalc->lsv)) 
                    pcalc->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	pcalc->lalm = val;
	return;
}

static void doOutputCallback(pcallback)
    struct callback *pcallback;
{

    dbCommon    *pcalc;
    struct rset *prset;

    callbackGetUser(pcalc, pcallback);
    prset = (struct rset *)pcalc->rset;
    dbScanLock((struct dbCommon *)pcalc);
    (*prset->process)(pcalc);
    dbScanUnlock((struct dbCommon *)pcalc);
}

    

static void execOutput(pcalc)
    struct calcoutRecord *pcalc;
{
        long            status;

        /* Determine output data */
        switch(pcalc->dopt) {
          case(calcoutDOPT_Use_VAL):
              pcalc->oval = pcalc->val;
            break; 
          case(calcoutDOPT_Use_OVAL):
              if(!pcalc->oclv) {
                  if(calcPerform(&pcalc->a,&pcalc->oval,pcalc->orpc)) {
                        recGblSetSevr(pcalc,CALC_ALARM,INVALID_ALARM);
                  }
              }
              else 
                  recGblSetSevr(pcalc,CALC_ALARM,INVALID_ALARM);
          break;
        }

        /* Check to see what to do if INVALID */
        if (pcalc->sevr < INVALID_ALARM ) {
            /* Output the value */
            status=dbPutLink(&(pcalc->out), DBR_DOUBLE,&(pcalc->oval),1);
            /* post event if output event != 0 */
            if(pcalc->oevt > 0) {
                post_event((int)pcalc->oevt);
            }
        }
        else {
            switch (pcalc->ivoa) {
                case (menuIvoaContinue_normally) :
                    /* write the new value */
                    status=dbPutLink(&(pcalc->out), DBR_DOUBLE,
                                     &(pcalc->oval),1);
                    /* post event if output event != 0 */
                    if(pcalc->oevt > 0) {
                        post_event((int)pcalc->oevt);
                    }
                  break;
                case (menuIvoaDon_t_drive_outputs) :
                  break;
                case (menuIvoaSet_output_to_IVOV) :
                    pcalc->oval=pcalc->ivov;
                    status=dbPutLink(&(pcalc->out), DBR_DOUBLE,
                                     &(pcalc->oval),1);
                    /* post event if output event != 0 */
                    if(pcalc->oevt > 0) {
                        post_event((int)pcalc->oevt);
                    }
                  break;
                default :
                    status=-1;
                    recGblRecordError(S_db_badField,(void *)pcalc,
                            "calcout:process Illegal IVOA field");
            }
        } 
}

static void monitor(pcalc)
    struct calcoutRecord	*pcalc;
{
	unsigned short	monitor_mask;
	double		delta;
	double		*pnew;
	double		*pprev;
	int		i;

        monitor_mask = recGblResetAlarms(pcalc);
        /* check for value change */
        delta = pcalc->mlst - pcalc->val;
        if(delta<0.0) delta = -delta;
        if (delta > pcalc->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pcalc->mlst = pcalc->val;
        }
        /* check for archive change */
        delta = pcalc->alst - pcalc->val;
        if(delta<0.0) delta = -delta;
        if (delta > pcalc->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pcalc->alst = pcalc->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pcalc,&pcalc->val,monitor_mask);
        }
        /* check all input fields for changes*/
        for(i=0, pnew=&pcalc->a, pprev=&pcalc->la; i<ARG_MAX; 
            i++, pnew++, pprev++) {
             if((*pnew != *pprev) || (monitor_mask&DBE_ALARM)) {
                  db_post_events(pcalc,pnew,monitor_mask|DBE_VALUE|DBE_LOG);
                  *pprev = *pnew;
	     }
	}
        /* Check OVAL field */
        if(pcalc->povl != pcalc->oval) {
            db_post_events(pcalc,&pcalc->oval, monitor_mask|DBE_VALUE|DBE_LOG);
            pcalc->povl = pcalc->oval;
        }
        return;
}

static int fetch_values(pcalc)
    struct calcoutRecord *pcalc;
{
	struct link	*plink;	/* structure of the link field  */
	double		*pvalue;
	long		status = 0;
	int		i;

	for(i=0, plink=&pcalc->inpa, pvalue=&pcalc->a; i<ARG_MAX; 
            i++, plink++, pvalue++) {

            status = dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);

	    if (!RTN_SUCCESS(status)) return(status);
	}
	return(0);
}

static void checkLinksCallback(pcallback)
    struct callback *pcallback;
{

    struct calcoutRecord *pcalc;
    struct rpvtStruct   *prpvt;

    callbackGetUser(pcalc, pcallback);
    prpvt = (struct rpvtStruct *)pcalc->rpvt;
    
    dbScanLock((struct dbCommon *)pcalc);
    prpvt->wd_id_1_LOCK = 0;
    checkLinks(pcalc);
    dbScanUnlock((struct dbCommon *)pcalc);

}



static void checkLinks(pcalc)
    struct calcoutRecord *pcalc;
{

    struct link *plink;
    struct rpvtStruct   *prpvt = (struct rpvtStruct *)pcalc->rpvt;
    int i;
    int stat;
    int caLink   = 0;
    int caLinkNc = 0;
    unsigned short *plinkValid;

    if(calcoutRecDebug) printf("checkLinks() for %p\n", pcalc);

    plink   = &pcalc->inpa;
    plinkValid = &pcalc->inav;

    for(i=0; i<ARG_MAX+1; i++, plink++, plinkValid++) {
        if (plink->type == CA_LINK) {
            caLink = 1;
            stat = dbCaIsLinkConnected(plink);
            if(!stat && (*plinkValid == calcoutINAV_EXT_NC)) {
                caLinkNc = 1;
            }
            else if(!stat && (*plinkValid == calcoutINAV_EXT)) {
                *plinkValid = calcoutINAV_EXT_NC;
                db_post_events(pcalc,plinkValid,DBE_VALUE);
                caLinkNc = 1;
            } 
            else if(stat && (*plinkValid == calcoutINAV_EXT_NC)) {
                *plinkValid = calcoutINAV_EXT;
                db_post_events(pcalc,plinkValid,DBE_VALUE);
            } 
        }
        
    }
    if(caLinkNc)
        prpvt->caLinkStat = CA_LINKS_NOT_OK;
    else if(caLink)
        prpvt->caLinkStat = CA_LINKS_ALL_OK;
    else
        prpvt->caLinkStat = NO_CA_LINKS;

    if(!prpvt->wd_id_1_LOCK && caLinkNc) {
        /* Schedule another CALLBACK */
        prpvt->wd_id_1_LOCK = 1;
        wdStart(prpvt->wd_id_1, 30, (FUNCPTR)callbackRequest,
               (int)(&prpvt->checkLinkCb));
    }
}

