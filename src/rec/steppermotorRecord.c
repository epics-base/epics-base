/* recSteppermotor.c */
/* base/src/rec  $Id$ */

/* recSteppermotor.c - Record Support Routines for Steppermotor records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            12-11-89
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
 * .01  02-07-90        lrd     fix initial fetch from within the motor record
 * .02  02-07-90        lrd     add a motor command for reading current status
 * .03  04-11-90        lrd     fixed acceleration for velocity mode motor
 * .04  04-13-90        lrd     make second argument for move = 0
 * .05  04-19-90        lrd     keep first error
 *                              add retry deadband
 *                              make the retry count a database field
 * .06  04-20-90        lrd     make readback occur before setting MOVN to 0
 * .07  07-02-90        lrd     make conversion compute in floating point
 * .08  10-01-90        lrd     modify readbacks to be throttled by delta
 * .09  10-23-90        lrd     update rbv even when there are no monitors
 * .10  10-25-90        lrd     change initialization to set all variables to IVAL
 * .11  10-26-90        lrd     add DMOV to indicate all retries exhausted or
 *                              motor is at position within deadband
 * .12  10-31-90        lrd     add time stamps
 * .13  11-28-90        lrd     make initialization work when readbacks are
 *                              from LVDTs, Motor position and encoders.
 *                              Fixed sm_get_position to be aware of the motion
 *                              status before it was set in the record see .06
 * .14  11-29-90        lrd     conditionally process soft channels
 * .15  12-14-90        lrd     fixed limit switch monitor notification
 * .16  12-17-90        lrd     stop motor on overshoot
 * .17  12-17-90        lrd     fix limits on initialization
 * .18  03-15-91        lrd     change acceleration and velocity for positional
 *                              motors
 * .19  03-21-91        lrd     add forward link processing
 * .20  06-04-91        lrd     apply drive high and low software clamps before
 *                              checking if the setpoint is different
 *                              move the conversion to steps in line
 *                              apply deadband to overshoot checking

 * .21  06-25-91        mk      fix direction indication
 * .22  06-25-91        mk/lrd  fix encoder position divide by zero
 * .23  06-25-91        lrd     add DBE_LOG to monitors other than value
 * .24  07-02-91        rac     avoid gcc warnings
 * .25  08-28-91        lrd     add open circuit detect on limit switches
 * .26  09-16-91        lrd     fix IALG not 0 and SCAN not Passive - The motor
 *                              would continually attempt to initialize
 * .27  12-09-91        lrd     new ININVALID severity for errors that invalidate
 *                              the results
 * .28  12-17-91        lrd     changed the MDEL and ADEL deadbands so
 *                              the range for exceeding the deadband includes
 *                              the value specified. (i.e. 0 is always violated)

 * .21  10-15-90	mrk	extensible record and device support
 * .22  10-24-91	jba	bug fix to alarms
 * .23  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .24  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .25  02-28-92	jba	ANSI C changes
 * .26  03-18-92	mrk	move retry to callback
				Make STOP stop even if retry>0
				Make motor move whenever VAL field accessed
 * .27  04-08-92	mrk	break out device support
 * .28  04-18-92        jba     removed process from dev init_record parms
 * .29  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .30  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .31  07-21-92        jba     changed alarm limits for non val related fields
 * .32  10-10-92        jba     replaced code for get of VAL from DOL with recGblGetLinkValue
 * .33  01-05-93        jbk     force recalc of velo and accel each time rec processed
 * .34  07-20-93        jbk     fixed accel of zero causing divide by zero
 * .35  08-06-93	mrk	vel mode: Call recGblFwdLink only when motor
 *				Stops
 * .36  09-15-93	mrk	call monitor when starting
 * .37  03-29-94	mcn	converted to fast links
 * .38  09-27-95	lrd	fix init to limit in overshoot check and retry
 *				post monitors for mcw and mccw
 * .39  04-09-96	ric	Pos/Neg limit algos changed to move max int
 *				steps.  Dev/Drv sup will interpret meaning
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<dbScan.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#define GEN_SIZE_OFFSET
#include	<steppermotorRecord.h>
#undef  GEN_SIZE_OFFSET
#include	<steppermotor.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long  special();
static long get_value();
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

struct rset steppermotorRSET={
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

struct smdset {
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       sm_command;
};


#define VELOCITY 0
#define POSITION 1
#define POSITIVE_LIMIT 1
#define NEGATIVE_LIMIT 2
#define POSITIVE_HOME  3
#define NEGATIVE_HOME  4

static void alarm();
static void monitor();
static void smcb_callback();
static void init_sm();
static void positional_sm();
static void velocity_sm();
static void sm_get_position();


static long init_record(psm,pass)
    struct steppermotorRecord	*psm;
    int pass;
{
    struct smdset *pdset;
    long status;

    if (pass==0) return(0);

    if(!(pdset = (struct smdset *)(psm->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)psm,"sm: init_record");
        return(S_dev_noDSET);
    }
    /* must have sm_command function defined */
    if( (pdset->number < 5) || (pdset->sm_command == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)psm,"sm: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
        if((status=(*pdset->init_record)(psm))) return(status);
    }

    /* get the initial value if dol is a constant*/
    if (psm->dol.type == CONSTANT ){
	recGblInitConstantLink(&psm->dol,DBF_FLOAT,&psm->val);
        psm->udf = FALSE;
    }

    init_sm(psm);
    return(0);
}


static long process(psm)
	struct steppermotorRecord	*psm;
{
	struct smdset   *pdset = (struct smdset *)(psm->dset);

        if( (pdset==NULL) || (pdset->sm_command==NULL) ) {
                psm->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)psm,"sm_command");
                return(S_dev_missingSup);
        }
	/* intialize the stepper motor record when the init bit is 0 */
	/* the init is set when the readback returns */
	if (psm->init <= 0){
		if(psm->init==0) init_sm(psm);
		recGblGetTimeStamp(psm);
		monitor(psm);
		return(0);
	}

	psm->pact = TRUE;
	if(psm->cmod == POSITION) {
		positional_sm(psm);
		if(!psm->dmov) {
			recGblGetTimeStamp(psm);
			monitor(psm);
			psm->pact=FALSE;
			return(0);
		}
	}
	else {
		velocity_sm(psm);
	}
	recGblGetTimeStamp(psm);
	/* check event list */
	monitor(psm);
	/* process the forward scan link record */
	recGblFwdLink(psm);
	psm->pact=FALSE;
	return(0);
}

static long get_value(psm,pvdes)
    struct steppermotorRecord		*psm;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &psm->val;
    return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    struct steppermotorRecord *psm =
				(struct steppermotorRecord *)(paddr->precord);
    int                 special_type = paddr->special;

    if(!after) return(0);
    switch(special_type) {
    case(SPC_MOD):
	/* set lval different than val so that motor will move*/
	if(psm->lval==psm->val) psm->lval = psm->val+1.0;
        return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"stepperMotor: special");
        return(S_db_badChoice);
    }
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    strncpy(units,psm->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    *precision = psm->prec;
    if(paddr->pfield==(void *)&psm->val
    || paddr->pfield==(void *)&psm->lval) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psm->val
    || paddr->pfield==(void *)&psm->mpos
    || paddr->pfield==(void *)&psm->rbv
    || paddr->pfield==(void *)&psm->epos
    || paddr->pfield==(void *)&psm->hihi
    || paddr->pfield==(void *)&psm->high
    || paddr->pfield==(void *)&psm->low
    || paddr->pfield==(void *)&psm->lolo
    || paddr->pfield==(void *)&psm->lval){
        pgd->upper_disp_limit = psm->hopr;
        pgd->lower_disp_limit = psm->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psm->val
    || paddr->pfield==(void *)&psm->mpos
    || paddr->pfield==(void *)&psm->rbv
    || paddr->pfield==(void *)&psm->epos
    || paddr->pfield==(void *)&psm->hihi
    || paddr->pfield==(void *)&psm->high
    || paddr->pfield==(void *)&psm->low
    || paddr->pfield==(void *)&psm->lolo
    || paddr->pfield==(void *)&psm->lval){
        pcd->upper_ctrl_limit = psm->drvh;
        pcd->lower_ctrl_limit = psm->drvl;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psm->val
    || paddr->pfield==(void *)&psm->mpos
    || paddr->pfield==(void *)&psm->rbv
    || paddr->pfield==(void *)&psm->epos
    || paddr->pfield==(void *)&psm->lval){
         pad->upper_alarm_limit = psm->hihi;
         pad->upper_warning_limit = psm->high;
         pad->lower_warning_limit = psm->low;
         pad->lower_alarm_limit = psm->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}


static void alarm(psm)
    struct steppermotorRecord	*psm;
{
	float deviation;

	deviation = psm->val - psm->rbv;

        /* alarm condition hihi */
        if (deviation > psm->hihi && recGblSetSevr(psm,HIHI_ALARM,psm->hhsv)){
                return;
        }

        /* alarm condition lolo */
        if (deviation < psm->lolo && recGblSetSevr(psm,LOLO_ALARM,psm->llsv)){
                return;
        }

        /* alarm condition high */
        if (deviation > psm->high && recGblSetSevr(psm,HIGH_ALARM,psm->hsv)){
                return;
        }

        /* alarm condition low */
        if (deviation < psm->low && recGblSetSevr(psm,LOW_ALARM,psm->lsv)){
                return;
        }
        return;
}

static void monitor(psm)
    struct steppermotorRecord	*psm;
{
	unsigned short	monitor_mask;
        float           delta;

        /* get previous stat and sevr  and new stat and sevr*/
	monitor_mask = recGblResetAlarms(psm);
        /* check for value change */
        delta = psm->mlst - psm->val;
        if(delta<0.0) delta = -delta;
        if (delta > psm->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                psm->mlst = psm->val;
        }
        /* check for archive change */
        delta = psm->alst - psm->val;
        if(delta<0.0) delta = -delta;
        if (delta > psm->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                psm->alst = psm->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(psm,&psm->val,monitor_mask);
        }
        return;
}

/*
 * SMCB_CALLBACK
 *
 * callback routine when a velocity is read
 */
static void smcb_callback(psm_data,psm)
struct motor_data	*psm_data;
struct steppermotorRecord	*psm;
{
    struct smdset   *pdset = (struct smdset *)(psm->dset);
    unsigned short   stat,sevr,nsta,nsev;
    int		intAccept=interruptAccept;
    short	post_events;
    double          temp;
    short	done_move=0;

    if(intAccept) {
	dbScanLock((struct dbCommon *)psm);
	if(psm->pact) {
	    dbScanUnlock((struct dbCommon *)psm);
	    return;
	}
	psm->pact = TRUE;
	recGblGetTimeStamp(psm);
    } else psm->mlis.count=0;
    if (psm->cmod == VELOCITY){
	/* check velocity */
	if (psm->rrbv != psm_data->velocity){
		psm->rrbv = psm_data->velocity;
		psm->rbv = (float)psm_data->velocity / (float)psm->mres;
		if (psm->mlis.count){
			db_post_events(psm,&psm->rbv,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->rrbv,DBE_VALUE|DBE_LOG);
		}
	}

	/* direction */
	if (psm->dir != psm_data->direction){
		psm->dir = psm_data->direction;
		if (psm->mlis.count)
			db_post_events(psm,&psm->dir,DBE_VALUE|DBE_LOG);
	}

	/* constant velocity */
	if (psm->cvel != psm_data->constant_velocity){
		psm->cvel = psm_data->constant_velocity;
		if (psm->mlis.count)
			db_post_events(psm,&psm->cvel,DBE_VALUE|DBE_LOG);
	}
    }else{ /* POSITION*/
	/* constant velocity */
	if (psm->cvel != psm_data->constant_velocity){
		psm->cvel = psm_data->constant_velocity;
		if (psm->mlis.count)
			db_post_events(psm,&psm->cvel,DBE_VALUE|DBE_LOG);
	}

	/* direction */
	if (psm->dir != psm_data->direction){
		psm->dir = psm_data->direction;
		if (psm->mlis.count)
			db_post_events(psm,&psm->dir,DBE_VALUE|DBE_LOG);
	}

	/* encoder position */
	/* the encoder is multiplied by 4 on the assumption that all indexers */
	/* use the quardrature encoding technique - if we use an encoder      */
	/* that does not, then we need to make quadrature encoder a database  */
	/* field and use the 4 on that condition !!!!!                        */
	if (psm->eres){
		psm->epos = (psm_data->encoder_position * psm->dist * psm->mres)
		  / (psm->eres * 4);
		if (psm->mlis.count)
			db_post_events(psm,&psm->epos,DBE_VALUE|DBE_LOG);
	}

	/* motor position */
	if (psm->mpos != psm_data->motor_position){
		psm->mpos = psm_data->motor_position * psm->dist;
		if (psm->mlis.count)
			db_post_events(psm,&psm->mpos,DBE_VALUE|DBE_LOG);
	}

	/* limit switches */
	if (psm->mcw != psm_data->cw_limit){
		psm->mcw = psm_data->cw_limit;
		psm->cw = (psm->mcw)?0:1;   /* change sense for VMS OPI */
		if (psm->mlis.count){
			db_post_events(psm,&psm->cw,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->mcw,DBE_VALUE|DBE_LOG);
		}
	}
	if (psm->mccw != psm_data->ccw_limit){
		psm->mccw = psm_data->ccw_limit;
		psm->ccw = (psm->mccw)?0:1; /* change sense for VMS OPI */
		if (psm->mlis.count){
			db_post_events(psm,&psm->ccw,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->mccw,DBE_VALUE|DBE_LOG);
		}
	}

	/* alarm conditions for limit switches */
        /* get previous stat and sevr  and new stat and sevr*/
        recGblResetAlarms(psm);
	post_events = FALSE;
	if (psm->mccw && psm->mcw){                    /* limits disconnected */
		recGblSetSevr(psm,WRITE_ALARM,INVALID_ALARM);
		post_events = TRUE;
	}else if ((psm->ccw == 0) || (psm->cw == 0)){  /* limit violation */
		recGblSetSevr(psm,HW_LIMIT_ALARM,psm->hlsv);
		post_events = TRUE;
	}

	/* moving */
	if (psm->movn != psm_data->moving){
		psm->movn = psm_data->moving;
		if (psm->mlis.count)
			db_post_events(psm,&psm->movn,DBE_VALUE|DBE_LOG);
	}
	/* get the read back value */
	sm_get_position(psm);

	/* needs to follow get position to prevent moves with old readback */

        /* anyone waiting for an event on this record */
        if (psm->mlis.count!=0  && post_events) {
		db_post_events(psm,&psm->val,DBE_VALUE|DBE_ALARM|DBE_LOG);
		db_post_events(psm,&psm->rbv,DBE_VALUE|DBE_ALARM|DBE_LOG);
		db_post_events(psm,&psm->stat,DBE_VALUE|DBE_LOG);
		db_post_events(psm,&psm->sevr,DBE_VALUE|DBE_LOG);
   	 }
        /* stop motor on overshoot */
        if ((psm->movn) && (psm->init == 1)){
                if (psm->posm){ /* moving in the positive direction */
                        if (psm->rbv > (psm->val + psm->rdbd))
                                (*pdset->sm_command)(psm,SM_MOTION,0,0);
                }else{          /* moving in the negative direction */
                        if (psm->rbv < (psm->val + psm->rdbd) )
                                (*pdset->sm_command)(psm,SM_MOTION,0,0);
                }
        }
	if((!psm->movn) && (psm->init == 1)) {
	    /* difference between desired position and readback pos */
	    if ( (psm->rbv < (psm->val - psm->rdbd))
	      || (psm->rbv > (psm->val + psm->rdbd)) ){
                    /* determine direction */
                    psm->posm = (psm->rbv < psm->val);
    
		    /* one attempt was made - record the error */
		    if (psm->rcnt == 1){
			    psm->miss = (psm->val - psm->rbv);
			    if (psm->mlis.count)
				    db_post_events(psm,&psm->miss,DBE_VALUE|DBE_LOG);
		    }

		    /* should we retry */
		    if (psm->rcnt < psm->rtry){

                            /* convert */
                            temp = psm->val / psm->dist;
                            psm->rval = temp;
    

			    /* move motor */
			    if ((*pdset->sm_command)(psm,SM_MOVE,psm->rval-psm->rrbv,0) < 0){
				    recGblSetSevr(psm,WRITE_ALARM,INVALID_ALARM);
			    } else {
			        psm->rcnt++;
			        if (psm->mlis.count){
				    db_post_events(psm,&psm->rcnt,DBE_VALUE|DBE_LOG);
			        }
                                done_move = 0;
			    }
                    /* no more retries - put the record in alarm */
                    }else{
                            done_move = 1;
                    }
            }else{
                    /* error doesn't exceed deadband - done moving */
                    done_move = 1;
            }
	}
        /* there was a move in progress and now it is complete */
        if (done_move && (psm->dmov == 0)){
                psm->dmov = 1;
                if (psm->mlis.count)
                        db_post_events(psm,&psm->dmov,DBE_VALUE|DBE_LOG);

                /* check for deviation from desired value */
		recGblGetTimeStamp(psm);
                alarm(psm);
		monitor(psm);
		/* process the forward scan link record */
		recGblFwdLink(psm);
	}
    }
    if(intAccept) {
	psm->pact = FALSE;
	dbScanUnlock((struct dbCommon *)psm);
    }
    return;
}

/*
 * INIT_SM
 */
static void init_sm(psm)
struct steppermotorRecord      *psm;
{
	struct smdset   *pdset = (struct smdset *)(psm->dset);
	int	acceleration,velocity;
	short		status=0;

	/* acceleration is in terms of seconds to reach velocity */
	if(psm->accl!=0)
		acceleration = (1/psm->accl) * psm->velo * psm->mres;
	else
		acceleration = 0;

	/* velocity is in terms of revolutions per second */
	velocity = psm->velo * psm->mres;

	/* initialize the motor */
	/* set mode - first command checks card present */
	if ((*pdset->sm_command)(psm,SM_MODE,psm->mode,0) < 0){
		recGblSetSevr(psm,WRITE_ALARM,INVALID_ALARM);
		psm->init = 1;
		return;
	}

	/* set encoder/motor ratio */
	(*pdset->sm_command)(psm,SM_ENCODER_RATIO,psm->mres,psm->eres);

	/* set the velocity */
	(*pdset->sm_command)(psm,SM_VELOCITY,velocity,acceleration);
        psm->lvel = psm->velo;
        psm->lacc = psm->accl;

	/* set the callback routine */
	(*pdset->sm_command)(psm,SM_CALLBACK,smcb_callback,psm);

	/* initialize the limit values */
	psm->mcw = psm->mccw = -1;

	/*  set initial position */
	if (psm->mode == POSITION){
		if (psm->ialg != 0){
			switch (psm->ialg){
			case (POSITIVE_LIMIT):
				status = (*pdset->sm_command)(psm,SM_FIND_LIMIT,1,0);
				break;
			case (NEGATIVE_LIMIT):
				status = (*pdset->sm_command)(psm,SM_FIND_LIMIT,-1,0);
				break;
			case (POSITIVE_HOME):
				status = (*pdset->sm_command)(psm,SM_FIND_HOME,1,0);
				break;
			case (NEGATIVE_HOME):
				status = (*pdset->sm_command)(psm,SM_FIND_HOME,-1,0);
				break;
			}
			psm->sthm = 1;
		/* force a read of the position and status */
		}else{
			status = (*pdset->sm_command)(psm,SM_READ,0,0);
		}
		if (status < 0){
			recGblSetSevr(psm,WRITE_ALARM,INVALID_ALARM);
			return;
		}
		psm->init = -1;
	}else if (psm->mode == VELOCITY){
		psm->velo = 0;
		psm->init = 1;
	}
	psm->cmod = psm->mode;
	return;
}

/*
 * POSITIONAL_SM
 *
 * control a stepper motor through position
 */
static void positional_sm(psm)
struct steppermotorRecord	*psm;
{
	struct smdset   *pdset = (struct smdset *)(psm->dset);
        int             acceleration,velocity;
	long 		status;

	
	/* emergency stop */
	if (psm->stop){
		(*pdset->sm_command)(psm,SM_MOTION,0,0);
		psm->stop = 0;
		psm->rcnt=psm->rtry+1;
		if (psm->mlis.count) db_post_events(psm,&psm->stop,DBE_VALUE|DBE_LOG);
		return;
	}

	/* no need to do anymore if the motor is in motion */
	if (psm->movn != 0)
		return;

        /* set the velocity and acceleration */
	/* jbk change for allowing mres to change */
/*      if ((psm->velo != psm->lvel) || (psm->lacc != psm->accl)){ */
				if(psm->accl!=0)
                	acceleration = (1/psm->accl) * psm->velo * psm->mres;
				else
					acceleration = 0;

                velocity = psm->velo * psm->mres;
                (*pdset->sm_command)(psm,SM_VELOCITY,velocity,acceleration);
                psm->lvel = psm->velo;
                psm->lacc = psm->accl;
/*      } */

	/* set home when requested */
	if (psm->sthm != 0){
		psm->sthm = 0;		/* reset the set home field */
		psm->val = 0;		/* make desired value = home */
		psm->rval = 0;		/* make the raw value = 0 */
		psm->rbv = 0;
		psm->rrbv = 0;
		psm->ival = 0;		/* resets initial offset */
		(*pdset->sm_command)(psm,SM_SET_HOME,0,0);
		if(psm->mlis.count) {
			db_post_events(psm,&psm->rbv,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->val,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->sthm,DBE_VALUE|DBE_LOG);
		}
		return;
	}

	/* fetch the desired value if there is a database link */
        if (psm->omsl == CLOSED_LOOP) {
		status=dbGetLink(&(psm->dol),DBR_FLOAT, &(psm->val),0,0);
		if (!RTN_SUCCESS(status)) return;
		psm->udf = FALSE;
	}

        /* check drive limits */
        if (psm->dist > 0){
                if (psm->val > psm->drvh) psm->val = psm->drvh;
                else if (psm->val < psm->drvl) psm->val = psm->drvl;
        }else{
                if (-psm->val > psm->drvh) psm->val = -psm->drvh;
                else if (-psm->val < psm->drvl) psm->val = -psm->drvl;
        }


	/* Change of desired position */
	if (psm->lval != psm->val){
		double temp;

		sm_get_position(psm);
		psm->rcnt = 0;
		psm->lval = psm->val;
                psm->dmov = 0;          /* start moving to desired location */
		psm->posm = (psm->rbv < psm->val);
		temp = psm->val/psm->dist;
		psm->rval = temp;
		if (psm->mlis.count){
			db_post_events(psm,&psm->rcnt,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->lval,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->dmov,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->posm,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->rval,DBE_VALUE|DBE_LOG);
		}
	        /* move motor */
		if ((*pdset->sm_command)(psm,SM_MOVE,psm->rval-psm->rrbv,0) < 0){
			recGblSetSevr(psm,WRITE_ALARM,INVALID_ALARM);
		}
	}

	return;
}

/*
 * VELOCITY_SM
 *
 * control a velocity stepper motor
 */
static void velocity_sm(psm)
struct steppermotorRecord	*psm;
{
	struct smdset   *pdset = (struct smdset *)(psm->dset);
	float	chng_vel;
	int	acceleration,velocity;
	long 	status;

	/* fetch the desired value if there is a database link */
        if (psm->omsl == CLOSED_LOOP){
		status=dbGetLink(&(psm->dol),DBR_FLOAT, &(psm->val),0,0);
		if (!RTN_SUCCESS(status)) return;
		psm->udf = FALSE;
	}

	/* Motor not at desired velocity */
	if ((psm->mlst == psm->val) && (psm->val != psm->rbv) && (psm->cvel)) {
		alarm(psm);
		return;
	}

	/* convert the egu velocity to hardware understandable velocity */
	psm->rval = psm->val * psm->mres;

	/* send the new velocity */
	if (psm->rval != 0){

		/* only if velocity has changed */
		if (psm->velo != psm->val){
			/* acceleration */
			chng_vel = psm->velo - psm->val;
			if (chng_vel < 0) chng_vel = -chng_vel;
			if(psm->accl!=0)
				acceleration = (1/psm->accl) * chng_vel * psm->mres;
			else
				acceleration = 0;

			/* velocity */
			psm->velo = psm->val;
			velocity = ((psm->val >= 0) ? psm->rval : -psm->rval);

			/* motor commands */
			(*pdset->sm_command)(psm,SM_VELOCITY,velocity,acceleration);
	
			/*the last arg of next call is check for direction */
			if((*pdset->sm_command)(psm,SM_MOTION,1,(psm->val < 0))){
				recGblSetSevr(psm,WRITE_ALARM,INVALID_ALARM);
				return;
			}
			psm->cvel = 0;
		}
	}else{
		/* trust it will stop - to read velocity here will not */
		/* work as the motor takes more time to stop than the  */
		/* request for velocity takes to return		       */
		(*pdset->sm_command)(psm,SM_MOTION,0,0);
		psm->rbv = 0;
		psm->rrbv = 0;
		psm->cvel = 0;
		psm->velo = psm->val;

		/* post events */
		if(psm->mlis.count) {
			db_post_events(psm,&psm->rbv,DBE_VALUE);
			db_post_events(psm,&psm->velo,DBE_VALUE);
		}
	}
}

/*
 * SM_GET_POSITION
 *
 * get the stepper motor readback position
 */
static void sm_get_position(psm)
struct steppermotorRecord	*psm;
{
    struct smdset	*pdset = (struct smdset *)(psm->dset);
    short		reset;
    float		new_pos,delta;

    /* get readback position */
    if (psm->rdbl.type == DB_LINK){
	/* when readback comes from another field of this record   */
	/* the fetch will fail if the record is uninitialized      */
        /* also - prdl (process readback location) should be set   */
        /* to NO if the readback is from the same record           */

	reset = psm->init;
	if (reset == 0)	psm->init = 1;
	if(dbGetLink(&psm->rdbl,DBR_FLOAT, &new_pos,0,0)) {
		recGblSetSevr(psm,READ_ALARM,INVALID_ALARM);
		psm->init = reset;
		return;
	}
	psm->init = reset;
    /* default is the motor position returned from the driver */
    }
    else {
	new_pos = psm->mpos;
    }

    /* readback position at initialization */
    if ((psm->init <= 0) && (psm->movn == 0)){
	if (psm->sthm){
		(*pdset->sm_command)(psm,SM_SET_HOME,0,0);
		psm->sthm = 0;
		return;
	}
        psm->rbv = psm->val = psm->ival + new_pos;
	psm->rval = psm->rrbv = psm->rbv / psm->dist;
	psm->init = 1;
	if (psm->mlis.count != 0){
		db_post_events(psm,&psm->val,DBE_VALUE|DBE_ALARM|DBE_LOG);
		db_post_events(psm,&psm->rbv,DBE_VALUE|DBE_ALARM|DBE_LOG);
	}
    /* readback normally */
    }else{
	if (psm->ival != 0){
  		/* adjust if initial position is not 0 */
    		new_pos += psm->ival;
	}
	/* get the raw readback value */
	psm->rrbv = new_pos / psm->dist;

	/* post events */
	if (psm->mlis.count != 0){
		delta = new_pos - psm->rbv;
		if (!psm->movn ||((delta > psm->mdel) || (delta < -psm->mdel))){
			psm->rbv = new_pos;
			db_post_events(psm,&psm->rbv,DBE_VALUE|DBE_ALARM);
			db_post_events(psm,&psm->rrbv,DBE_VALUE|DBE_ALARM);
		}
        }else{
                psm->rbv = new_pos;
	}
    }

}

