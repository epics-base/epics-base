/* recSteppermotor.c */
/* share/src/rec $Id$ */

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

 * .21  10-15-90	mrk	extensible record and device support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<steppermotorRecord.h>
#include	<steppermotor.h>

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
long get_units();
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
long get_alarm_double();

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

/* because the driver does all the work just declare device support here*/
struct dset devSmCompumotor1830={4,NULL,NULL,NULL,NULL};
struct dset devSmOms6Axis={4,NULL,NULL,NULL,NULL};

#define VELOCITY 0
#define POSITION 1
#define POSITIVE_LIMIT 1
#define NEGATIVE_LIMIT 2

void alarm();
void monitor();
void smcb_callback();
void init_sm();
void convert_sm();
void positional_sm();
void velocity_sm();
void sm_get_position();

static not_init_record=FALSE;


static long init_record(psm)
    struct steppermotorRecord	*psm;
{

    /* get the initial value if dol is a constant*/
    if (psm->dol.type == CONSTANT ){
            psm->udf = FALSE;
            psm->val = psm->dol.value.value;
    }

    init_sm(psm);
    return(0);
}


static long process(paddr)
    struct dbAddr	*paddr;
{
	struct steppermotorRecord	*psm=(struct steppermotorRecord *)(paddr->precord);

	/* intialize the stepper motor record when the init bit is 0 */
	/* the init is set when the readback returns */
	not_init_record=TRUE;
	if (psm->init == 0){
		init_sm(psm);
		tsLocalTime(&psm->time);
		monitor(psm);
		return(0);
	}

	psm->pact = TRUE;
	if(psm->cmod == POSITION)
		positional_sm(psm);
	else
		velocity_sm(psm);


	tsLocalTime(&psm->time);
	/* check event list */
	monitor(psm);

	/* process the forward scan link record */
	if (psm->flnk.type==DB_LINK) dbScanPassive(psm->flnk.value.db_link.pdbAddr);

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

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    strncpy(units,psm->egu,sizeof(psm->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    *precision = psm->prec;
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    pgd->upper_disp_limit = psm->hopr;
    pgd->lower_disp_limit = psm->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    pcd->upper_ctrl_limit = psm->hopr;
    pcd->lower_ctrl_limit = psm->lopr;
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct steppermotorRecord	*psm=(struct steppermotorRecord *)paddr->precord;

    pad->upper_alarm_limit = psm->hihi;
    pad->upper_warning_limit = psm->high;
    pad->lower_warning_limit = psm->low;
    pad->lower_alarm_limit = psm->lolo;
    return(0);
}


static void alarm(psm)
    struct steppermotorRecord	*psm;
{
	float deviation;

	deviation = psm->val - psm->rbv;

        /* alarm condition hihi */
        if (psm->nsev<psm->hhsv){
                if (deviation > psm->hihi){
                        psm->nsta = HIHI_ALARM;
                        psm->nsev = psm->hhsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (psm->nsev<psm->llsv){
                if (deviation < psm->lolo){
                        psm->nsta = LOLO_ALARM;
                        psm->nsev = psm->llsv;
                        return;
                }
        }

        /* alarm condition high */
        if (psm->nsev<psm->hsv){
                if (deviation > psm->high){
                        psm->nsta = HIGH_ALARM;
                        psm->nsev =psm->hsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (psm->nsev<psm->lsv){
                if (deviation < psm->low){
                        psm->nsta = LOW_ALARM;
                        psm->nsev = psm->lsv;
                        return;
                }
        }
        return;

}

static void monitor(psm)
    struct steppermotorRecord	*psm;
{
	unsigned short	monitor_mask;
        float           delta;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=psm->stat;
        sevr=psm->sevr;
        nsta=psm->nsta;
        nsev=psm->nsev;
        /*set current stat and sevr*/
        psm->stat = nsta;
        psm->sevr = nsev;
        psm->nsta = 0;
        psm->nsev = 0;

        /* Flags which events to fire on the value field */
        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(psm,&psm->stat,DBE_VALUE);
                db_post_events(psm,&psm->sevr,DBE_VALUE);
        }
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
    short           stat,sevr,nsta,nsev;
   
/*
printf("smcb_callback\n");
printf("%d %d %d %d %d %d %d %d %d\n",
psm_data->cw_limit,
psm_data->ccw_limit,
psm_data->moving,
psm_data->direction,
psm_data->constant_velocity,
psm_data->velocity,
psm_data->encoder_position,
psm_data->motor_position,
psm_data->accel
);
*/
    if(not_init_record) {
	dbScanLock(psm);
	if(psm->pact) {
	    dbScanUnlock(psm);
	    return;
	}
	psm->pact = TRUE;
	tsLocalTime(&psm->time);
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
			db_post_events(psm,&psm->dir,DBE_VALUE);
	}

	/* constant velocity */
	if (psm->cvel != psm_data->constant_velocity){
		psm->cvel = psm_data->constant_velocity;
		if (psm->mlis.count)
			db_post_events(psm,&psm->cvel,DBE_VALUE);
	}
    }else{ /* POSITION*/
	/* constant velocity */
	if (psm->cvel != psm_data->constant_velocity){
		psm->cvel = psm_data->constant_velocity;
		if (psm->mlis.count)
			db_post_events(psm,&psm->cvel,DBE_VALUE);
	}

	/* direction */
	if (psm->dir != psm_data->direction){
		psm->dir = psm_data->direction;
		if (psm->mlis.count)
			db_post_events(psm,&psm->dir,DBE_VALUE);
	}

	/* encoder position */
	/* the encoder is multiplied by 4 on the assumption that all indexers */
	/* use the quardrature encoding technique - if we use an encoder      */
	/* that does not, then we need to make quadrature encoder a database  */
	/* field and use the 4 on that condition !!!!!                        */
	if (psm->epos){
		psm->epos = (psm_data->encoder_position * psm->dist * psm->mres)
		  / (psm->eres * 4);
		if (psm->mlis.count)
			db_post_events(psm,&psm->epos,DBE_VALUE);
	}

	/* motor position */
	if (psm->mpos != psm_data->motor_position){
		psm->mpos = psm_data->motor_position * psm->dist;
		if (psm->mlis.count)
			db_post_events(psm,&psm->mpos,DBE_VALUE);
	}

	/* limit switches */
	if (psm->mcw != psm_data->cw_limit){
		psm->mcw = psm_data->cw_limit;
		psm->cw = (psm->mcw)?0:1;
		if (psm->mlis.count)
			db_post_events(psm,&psm->cw,DBE_VALUE);
	}
	if (psm->mccw != psm_data->ccw_limit){
		psm->mccw = psm_data->ccw_limit;
		psm->ccw = (psm->mccw)?0:1;
		if (psm->mlis.count)
			db_post_events(psm,&psm->ccw,DBE_VALUE);
	}

	/* alarm conditions for limit switches */
	if ((psm->ccw == 0) || (psm->cw == 0)){	/* limit violation */
		if (psm->nsev<psm->hlsv) {
			psm->nsta = HW_LIMIT_ALARM;
			psm->nsev = psm->hlsv;
		}
	}


        /* get previous stat and sevr  and new stat and sevr*/
        stat=psm->stat;
        sevr=psm->sevr;
        nsta=psm->nsta;
        nsev=psm->nsev;
        /*set current stat and sevr*/
        psm->stat = nsta;
        psm->sevr = nsev;
        psm->nsta = 0;
        psm->nsev = 0;

        /* anyone waiting for an event on this record */
        if (psm->mlis.count!=0  && (stat!=nsta || sevr!=nsev) ){
		db_post_events(psm,&psm->val,DBE_VALUE|DBE_ALARM);
		db_post_events(psm,&psm->stat,DBE_VALUE);
		db_post_events(psm,&psm->sevr,DBE_VALUE);
   	 }

	/* get the read back value */
	sm_get_position(psm,psm_data->moving);

	/* needs to follow get position to prevent moves with old readback */
	/* moving */
	if (psm->movn != psm_data->moving){
		psm->movn = psm_data->moving;
		if (psm->mlis.count)
			db_post_events(psm,&psm->movn,DBE_VALUE|DBE_LOG);
	}

        /* stop motor on overshoot */
        if (psm->movn){
                if (psm->posm){ /* moving in the positive direction */
                        if (psm->rbv > (psm->val + psm->rdbd))
                                sm_driver(psm->dtyp,
                                        psm->out.value.vmeio.card,
                                        psm->out.value.vmeio.signal,
                                        SM_MOTION,0);
                }else{          /* moving in the negative direction */
                        if (psm->rbv < (psm->val + psm->rdbd) )
                                sm_driver(psm->dtyp,
                                        psm->out.value.vmeio.card,
                                        psm->out.value.vmeio.signal,
                                        SM_MOTION,0);
                }
        }
    }
    if(not_init_record) {
	psm->pact = FALSE;
	dbScanUnlock(psm);
    }
    return;
}

/*
 * INIT_SM
 */
static void init_sm(psm)
struct steppermotorRecord      *psm;
{
	int	acceleration,velocity;
	short	card,channel;
	short		status;

	/* the motor number is the card number */
	card = psm->out.value.vmeio.card;
	channel = psm->out.value.vmeio.signal;
	
	/* check supported hardware */
	if (psm->out.type != VME_IO){
		psm->init = 1;
		return;
	}

	/* acceleration is in terms of seconds to reach velocity */
	acceleration = (1/psm->accl) * psm->velo * psm->mres;

	/* velocity is in terms of revolutions per second */
	velocity = psm->velo * psm->mres;

	/* initialize the motor */
	/* set mode - first command checks card present */
	if (sm_driver(psm->dtyp,card,channel,SM_MODE,psm->mode,0) < 0){
		if(psm->nsev < VALID_ALARM) {
			psm->nsta = WRITE_ALARM;
			psm->nsev = VALID_ALARM;
		}
		psm->init = 1;
		return;
	}

	/* set encoder/motor ratio */
	sm_driver(psm->dtyp,card,channel,SM_ENCODER_RATIO,psm->mres,psm->eres);

	/* set the velocity */
	sm_driver(psm->dtyp,card,channel,SM_VELOCITY,velocity,acceleration);
        psm->lvel = psm->velo;
        psm->lacc = psm->accl;

	/* set the callback routine */
	sm_driver(psm->dtyp,card,channel,SM_CALLBACK,smcb_callback,psm);

	/* initialize the limit values */
	psm->mcw = psm->mccw = -1;

	/*  set initial position */
	if (psm->mode == POSITION){
		if (psm->ialg != 0){
			if (psm->ialg == POSITIVE_LIMIT){
				status = sm_driver(psm->dtyp,card,channel,SM_MOVE,0x0fffffff,0);
			}else if (psm->ialg == NEGATIVE_LIMIT){
				status = sm_driver(psm->dtyp,card,channel,SM_MOVE,-0x0fffffff,0);
			}
			psm->sthm = 1;
		/* force a read of the position and status */
		}else{
			status = sm_driver(psm->dtyp,card,channel,SM_READ,0,0);
		}
		if (status < 0){
			if (psm->nsev < VALID_ALARM) {
				psm->nsta = WRITE_ALARM;
				psm->nsev = VALID_ALARM;
			}
			return;
		}
	}else if (psm->mode == VELOCITY){
		/* get the velocity */
/*		get_velocity(motor,smcb_velocity_req);*/
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
	short	card,channel,done_move;
        int             acceleration,velocity;
	double          temp;

	/* only VME stepper motor cards supported */
	if (psm->out.type != VME_IO) return;

	/* the motor number is the card number */
	card = psm->out.value.vmeio.card;
	channel = psm->out.value.vmeio.signal;
	
	/* emergency stop */
	if (psm->stop){
		sm_driver(psm->dtyp,card,channel,SM_MOTION,0,0);
		psm->stop = 0;
		if (psm->mlis.count) db_post_events(psm,&psm->stop,DBE_VALUE);
		return;
	}

	/* no need to do anymore if the motor is in motion */
	if (psm->movn != 0)
		return;

        /* set the velocity and acceleration */
        if ((psm->velo != psm->lvel) || (psm->lacc != psm->accl)){
                acceleration = (1/psm->accl) * psm->velo * psm->mres;
                velocity = psm->velo * psm->mres;
                sm_driver(psm->dtyp,card,channel,SM_VELOCITY,velocity,acceleration);
                psm->lvel = psm->velo;
                psm->lacc = psm->accl;
        }

	/* set home when requested */
	if (psm->sthm != 0){
		psm->sthm = 0;		/* reset the set home field */
		psm->val = 0;		/* make desired value = home */
		psm->rval = 0;		/* make the raw value = 0 */
		psm->rbv = 0;
		psm->rrbv = 0;
		psm->ival = 0;		/* resets initial offset */
		sm_driver(psm->dtyp,card,channel,SM_SET_HOME,0,0);
		if(psm->mlis.count) {
			db_post_events(psm,&psm->rbv,DBE_VALUE);
			db_post_events(psm,&psm->val,DBE_VALUE);
			db_post_events(psm,&psm->sthm,DBE_VALUE);
		}
		return;
	}

	/* fetch the desired value if there is a database link */
        if (psm->dol.type == DB_LINK && psm->omsl == CLOSED_LOOP){
		long options=0;
		long nRequest=1;

		if(dbGetLink(&(psm->dol.value.db_link),psm,DBR_FLOAT,&(psm->val),&options,&nRequest)){
			if (psm->nsev < VALID_ALARM) {
				psm->nsta = LINK_ALARM;
				psm->nsev = VALID_ALARM;
			}
			return;
		} else psm->udf = FALSE;
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
		psm->rcnt = 0;
		psm->lval = psm->val;
                psm->dmov = 0;          /* start moving to desired location */
		if (psm->mlis.count){
			db_post_events(psm,&psm->rcnt,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->lval,DBE_VALUE|DBE_LOG);
			db_post_events(psm,&psm->dmov,DBE_VALUE|DBE_LOG);
		}
	}

	/* difference between desired position and readback pos */
	if ( (psm->rbv < (psm->val - psm->rdbd))
	  || (psm->rbv > (psm->val + psm->rdbd)) ){
                /* determine direction */
                if (psm->rcnt == 0)
                        psm->posm = (psm->rbv < psm->val);

		/* one attempt was made - record the error */
		if (psm->rcnt == 1){
			psm->miss = (psm->val - psm->rbv);
			if (psm->mlis.count)
				db_post_events(psm,&psm->miss,DBE_VALUE|DBE_LOG);
		}

		/* should we retry */
		if (psm->rcnt <= psm->rtry){
                        /* convert */
                        temp = psm->val / psm->dist;
                        psm->rval = temp;


			/* move motor */
			if (sm_driver(psm->dtyp,card,channel,SM_MOVE,psm->rval-psm->rrbv,0) < 0){
				if (psm->nsev < VALID_ALARM) {
					psm->stat = WRITE_ALARM;
					psm->sevr = VALID_ALARM;
				}
				return;
			}
			psm->movn = 1;
			psm->rcnt++;
			if (psm->mlis.count){
				db_post_events(psm,&psm->movn,DBE_VALUE|DBE_LOG);
				db_post_events(psm,&psm->rcnt,DBE_VALUE|DBE_LOG);
			}
                        done_move = 0;
                /* no more retries - put the record in alarm */
                }else{
                        done_move = 1;
                }
        }else{
                /* error doesn't exceed deadband - done moving */
                done_move = 1;
        }
        /* there was a move in progress and now it is complete */
        if (done_move && (psm->dmov == 0)){
                psm->dmov = 1;
                if (psm->mlis.count)
                        db_post_events(psm,&psm->dmov,DBE_VALUE|DBE_LOG);

                /* check for deviation from desired value */
                alarm(psm);

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
	float	chng_vel;
	int	acceleration,velocity;
	short	card,channel;

	/* only VME stepper motor cards supported */
	if (psm->out.type != VME_IO) return;

	/* fetch the desired value if there is a database link */
        if (psm->dol.type == DB_LINK && psm->omsl == CLOSED_LOOP){
		long options=0;
		long nRequest=1;

		if(dbGetLink(&(psm->dol.value.db_link),psm,DBR_FLOAT,&(psm->val),&options,&nRequest)) {
			if (psm->nsev < VALID_ALARM) {
				psm->nsta = READ_ALARM;
				psm->nsev = VALID_ALARM;
			}
			return;
		} else psm->udf=FALSE;
	}

	/* the motor number is the card number */
	card = psm->out.value.vmeio.card;
	channel = psm->out.value.vmeio.signal;
	
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
			acceleration = (1/psm->accl) * chng_vel * psm->mres;

			/* velocity */
			psm->velo = psm->val;
			velocity = ((psm->val >= 0) ? psm->rval : -psm->rval);

			/* motor commands */
			sm_driver(psm->dtyp,card,channel,SM_VELOCITY,velocity,acceleration);
	
			/*the last arg of next call is check for direction */
			if(sm_driver(psm->dtyp,card,channel,SM_MOTION,1,(psm->val < 0))){
				if (psm->nsev < VALID_ALARM) {
					psm->stat = WRITE_ALARM;
					psm->sevr = VALID_ALARM;
				}
				return;
			}
			psm->cvel = 0;
		}
	}else{
		/* trust it will stop - to read velocity here will not */
		/* work as the motor takes more time to stop than the  */
		/* request for velocity takes to return		       */
		sm_driver(psm->dtyp,card,channel,SM_MOTION,0,0);
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
static void sm_get_position(psm,moving)
struct steppermotorRecord	*psm;
short                           moving;
{
	short	reset;
	float		new_pos,delta;

    /* get readback position */
    if (psm->rdbl.type == DB_LINK){
	long options=0;
	long nRequest=1;

	/* when readback comes from another field of this record   */
	/* the fetch will fail if the record is uninitialized      */
        /* also - prdl (process readback location) should be set   */
        /* to NO if the readback is from the same record           */

	reset = psm->init;
	if (reset == 0)	psm->init = 1;
	if(dbGetLink(&(psm->rdbl.value.db_link),psm,DBR_FLOAT,&new_pos,&options,&nRequest)){
		if (psm->nsev < VALID_ALARM) {
			psm->nsta = READ_ALARM;
			psm->nsev = VALID_ALARM;
		}
		psm->init = reset;
		return;
	}
	psm->init = reset;
    /* default is the motor position returned from the driver */
    }else{
	new_pos = psm->mpos;
    }

    /* readback position at initialization */
    if ((psm->init == 0) && (moving == 0)){
	if (psm->sthm){
		sm_driver(psm->dtyp,
		  psm->out.value.vmeio.card,
		  psm->out.value.vmeio.signal,
		  SM_SET_HOME,0,0);
		psm->sthm = 0;
		return;
	}
        psm->rbv = psm->val = psm->ival + new_pos;
	psm->rval = psm->rrbv = psm->rbv / psm->dist;
	psm->init = 1;
	if (psm->mlis.count != 0){
		db_post_events(psm,&psm->val,DBE_VALUE|DBE_ALARM);
		db_post_events(psm,&psm->rbv,DBE_VALUE|DBE_ALARM);
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
		if ((delta > psm->mdel) || (delta < -psm->mdel)){
			psm->rbv = new_pos;
			db_post_events(psm,&psm->rbv,DBE_VALUE|DBE_ALARM);
			db_post_events(psm,&psm->rrbv,DBE_VALUE|DBE_ALARM);
		}
        }else{
                psm->rbv = new_pos;
	}
    }

}
