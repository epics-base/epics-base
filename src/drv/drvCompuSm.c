
/* drvCompuSm.c */
/* share/src/drv @(#)drvCompuSm.c	1.9     8/27/92 */
/*
 * subroutines and tasks that are used to interface to the Compumotor 1830
 * stepper motor drivers
 *
 * 	Author:	Bob Dalesio
 * 	Date:	01-03-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	02-07-90	lrd	add command to read the motor status
 * .02	04-11-90	lrd	made velocity mode motor go active
 * .03	04-12-90	lrd	only allow one connection to a motor
 * .04	04-13-90	lrd	add externally initiated motor motion monitoring
 * .05	09-05-91	joh	updated for v5 vxWorks
 * .06	10-09-91	lrd	monitor for external motion once every 2 seconds
 *				not at 30 Hz (.04 was not implemented correctly)
 * .06	11-31-91        bg	added compu_sm_io_report. Added sysBusToLocalAdrs()
 *                              for addressing.
 * .07	03-02-92        bg	added level and ability to print raw values to 
 *                              compu_sm_io_report for level > 0.
 * .08	05-04-92        bg	added compu_sm_reset and rebootHookAdd so ioc can be
 *                              rebooted with control X. 
 * .09	06-25-92        bg	Combined drvCompuSm.c and compu_sm_driver.c
 * .10	06-26-92        bg	Added level to compu_sm_io_report in drvCompuSm 
 *                              structure   
 * .11	06-29-92	joh	took file ptr arg out of io report
 * .12	08-06-92	joh	merged compu sm include file
 * .13	08-27-92	joh	silenced ANSI C function proto warning	
 * .14	08-27-92	joh	fixed no epics init
 */
#include <vxWorks.h>
#include <vme.h>
#include <sysLib.h>
#include <semLib.h>		/* library for semaphore support */
#include <wdLib.h>
#include <rngLib.h>		/* library for ring buffer support */

/* drvCompuSm.c -  Driver Support Routines for CompuSm */

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	 <steppermotor.h>
#include	 <task_params.h>


long compu_sm_io_report();
long compu_driver_init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvCompuSm={
	2,
	compu_sm_io_report,
	compu_driver_init};

/* compumotor vme interface information */
#define	MAX_COMPU_MOTORS	8		/********/

#define RESP_SZ		16		/* card returns 16 chars - cmd & resp */
#define RESPBUF_SZ	(RESP_SZ+1)	/* intr routine also passes motor no. */

/*	Control Byte bit definitions for the Compumotor 1830	*/
					/* bits 0 and 1 are not used */
#define	CBEND	0x04			/* end of command string */
#define CBLMR	0x08			/* last message byte read */
#define CBCR	0x10			/* command ready in cm_idb */
#define CBMA	0x20			/* message accepted from odb */
#define CBEI	0x40			/* enable interrupts */
#define CBBR	0x80			/* board reset */
#define	SND_MORE	CBCR | CBEI		/* more chars to send */
#define SND_LAST	CBEND | SND_MORE	/* last char being sent */
#define	RD_MORE		CBMA | CBEI		/* more chars to read */
#define RD_LAST		CBLMR | RD_MORE		/* last byte we need to read */

/*	Status Byte bit definitions	*/
#define SBIRDY	0x10			/* idb is ready */

/*	Structure used for communication with a Compumotor 1830
**	Motor Controller. The data buffer is repeated 64 times
**	on even word addresses (0xXXXXXX1,0xXXXXX5...) and the
**	control location is repeated 64 times on odd word 
**	addresses (0xXXXXX3, 0xXXXXX7...). The registers must
**	be read and written as bytes.
*/
struct compumotor {
	char	cm_d1;		/* not accessable */
	char	cm_idb;		/* input data buffer */
	char	cm_d2;		/* not accessable */
	char	cm_cb;		/* control byte */
	char	cm_d3[0x100-4];	/* fill to next standard address */
};


/* This file includes defines for all compumotor 1830 controller 
** commands. */

#define SM_NULL		0x0	/* Null command */
#define	SM_INT		0x1	/* Interrupt X */
#define SM_WRT_STAT	0x8	/* Write X to the user defined status bits */
#define	SM_SET_STAT	0x9	/* Set user defined status bit number X */ 
#define	SM_CLR_STAT	0xa	/* Clear user defined status bit number X */
#define	SM_SET_PROG	0xb	/* Set programmable output bit X */
#define	SM_CLR_PROG	0xc	/* Clear programmable output bit X */
#define	SM_WRT_PROG	0xd	/* Write X to the programmable output bits */
#define	SM_DEF_X_TO_Y	0xf	/* Define bit X to indicate state Y */
#define	SM_TOG_JOG	0x10	/* Disable/enable the JOG inputs */
#define	SM_DEF_JOG	0x11	/* Define JOG input functions */
#define SM_TOG_REM_PWR	0x12	/* Turn off/on remote power shutdown */
#define	SM_TOG_REM_SHUT	0x13	/* Disable/enable the "remote shutdown" bit */
#define	SM_SET_CW_MOTN	0x14	/* Set CW motion equal to +- */
#define	SM_TOG_POS_MTN	0x18	/* Turn off/on post-move position maintenance */
#define SM_TOG_STOP_STL	0x19	/* Turn off/on termination on stall detect */
#define SM_REP_X_Y	0x20	/* Repeat the following X commands Y times */
#define	SM_REP_TIL_CONT	0x21	/* Repeat the following X commands
				   until a CONTINUE is received */
#define SM_WAIT_CONT	0x28	/* Wait for a CONTINUE */
#define SM_WAIT_MILLI	0x29	/* Wait X milliseconds */
#define SM_WAIT_SECOND	0x2a	/* Wait X seconds */
#define SM_WAIT_MINUTE	0x2b	/* Wait X minutes */
#define SM_WAIT_TRIGGER	0x2c	/* Wait for trigger X to go active */
#define SM_DEF_A_OP_POS	0x2e	/* Define the abs open-loop position as X */
#define SM_DEF_A_CL_POS	0x2f	/* Define absolute closed-loop position */
#define	SM_DEF_ABS_ZERO	0x30	/* Define the present position as the 
				   absolute zero position */
#define	SM_DEF_VEL_ACC	0x31	/* Define default velocity and acceleration */
#define	SM_MOV_DEFAULT	0x32	/* Perform the default move (trapezoidal
				   continuous) */
#define SM_MOV_REL_POS	0x33	/* Go to relative position X at default
				   velocity and acceleration */
#define SM_MOV_ABS_POS	0x34	/* Go to absolute position X at default
				   velocity and acceleration */
#define	SM_MOV_REL_ENC	0x35	/* Go to relative encoder position X */
#define SM_MOV_ABS_ENC	0x36	/* Go to absolute encoder position X */
#define SM_DEF_OP_HOME	0x38	/* Define HOME location (open loop */
#define SM_DEF_CL_HOME	0x38	/* Define HOME location (closed loop) */
#define	SM_MOV_HOME_POS	0x39	/* Go HOME at the default velocity and
				   acceleration */
#define SM_MOV_HOME_ENC	0x3a	/* Go to encoder HOME at the default
				   velocity and acceleration */
#define	SM_DO_MOV_X	0x40	/* Perform move number X */
#define	SM_DO_SEQ_X	0x41	/* Perform sequence buffer X */
#define	SM_DO_VEL_STR	0x42	/* Perform the velocity streaming buffer */
#define	SM_CONT		0x48	/* CONTINUE (perform next command) */
#define	SM_OP_LOOP_MODE	0x50	/* Enter open loop indexer mode */
#define	SM_VEL_DIS_MODE	0x51	/* Enter velocity-distance streaming mode */
#define SM_VEL_TIM_MODE	0x52	/* Enter velocity-time streaming mode */
#define	SM_STOP		0x70	/* STOP motion */
#define SM_DSC_SEQ	0x71	/* Discontinue the sequence buffer */
#define	SM_SSP_SEQ	0x72	/* Suspend the sequence buffer;
				   wait for a CONTINUE to resume */
#define	SM_DSC_SNGL	0x73	/* Discontinue any singular command 
				   currently being performed */
#define	SM_STOP_ON_TRG	0x74	/* STOP motion when trigger X goes active */
#define	SM_DSC_SEQ_TRG	0x75	/* Discontinue the sequence buffer when
				   trigger X goes active */
#define SM_SSP_SEQ_TRG	0x76	/* Suspend sequence buffer when trigger
				   X goes active */
#define	SM_DSC_SNGL_TRG	0x77	/* Discontinue any singular command when
				   trigger X goes active */
#define	SM_KILL		0x78	/* Kill motion */
#define	SM_KILL_SEQ	0x79	/* Kill the sequence buffer */
#define	SM_KILL_SEQ_SNGL 0x7a	/* Kill current sequence singular command;
				   wait for CONTINUE */	
#define	SM_KILL_VEL_STR	0x7b	/* Kill the velocity streaming buffer */
#define	SM_KILL_ON_TRG	0x7c	/* Kill motion when trigger X goes active */
#define	SM_KILL_SEQ_TRG	0x7d	/* Kill the sequence buffer when trigger
				   X goes active */
#define	SM_KL_SQSNG_TRG	0x7e	/* Kill current sequence singular command
				   when trigger X goes active; wait for
				   a continue */
#define	SM_KL_VLSTR_TRG	0x7f	/* Kill the velocity streaming buffer
				   when trigger X goes active */
#define	SM_GET_B_REL_POS 0x80	/* Request position relative to the
				   beginning of the current move */
#define	SM_GET_E_REL_POS 0x81	/* Request position relative to the
				   end of the current move */
#define	SM_GET_H_REL_POS 0x82	/* Request position relative to the home
				   limit switch */
#define	SM_GET_Z_REL_POS 0x83	/* Request position relative to the 
				   absolute zero position */
#define	SM_GET_CUR_DIR	0x84	/* Request current direction */
#define	SM_GET_VEL	0x85	/* Request current velocity */
#define	SM_GET_ACC	0x86	/* Request current acceleration */
#define	SM_GET_MOV_STAT 0x88	/* Request current move status */
#define SM_GET_LIM_STAT	0x89	/* Request state of the limit switches */
#define	SM_GET_HOME_STAT 0x8a	/* Request state of the HOME switch */
#define SM_GET_TRV_DIR	0x8b	/* Request direction of travel */
#define	SM_GET_MOT_MOV	0x8c	/* Request whether motor is moving or not */
#define	SM_GET_MOT_CONST 0x8d	/* Request whether motor is at constant,
			  	   nonzero velocity or not */
#define	SM_GET_MOT_ACC	0x8e	/* Request whether motor is or is not
				   accelerating */
#define	SM_GET_MOT_DEC	0x8f	/* Request whether motor is or is not
				   decelerating */
#define	SM_GET_MODE	0x90	/* Request present mode */
#define	SM_GET_MV_PARM	0x91	/* Request move parameters for move number X */
#define	SM_GET_SEQ_CMMD	0x92	/* request commands stored in the
				   sequence buffer */
#define	SM_GET_MVDEF_STAT 0x93	/* Request state of the move definitions */
#define	SM_GET_TRG_STAT	0x94	/* Request state of trigger inputs */
#define	SM_GET_JOG_STAT	0x95	/* Request state of JOG inputs */
#define	SM_GET_Z_STAT	0x96	/* Request state of the Channel Z home input */
#define	SM_GET_OUT_STAT	0x97	/* Request the state of the programmable
				   output bits */
#define	SM_GET_REL_ENC	0x98	/* Request relative encoder count */
#define	SM_GET_REL_ERR	0x99	/* Request relative error from desired
				   closed loop position */
#define	SM_GET_ABS_ENC	0x9a	/* Request absolute encoder count */
#define	SM_GET_SLIP_STAT 0x9b	/* Request slip detect status */
#define	SM_GET_RATIO	0x9c	/* Request motor pulse to encoder pulse ratio */
#define	SM_GET_RESOLTN	0x9d	/* Request motor resolution */
#define	SM_GET_BACK_SIG	0x9e	/* Request backlash sigma (motor steps)*/
#define	SM_GET_ALG	0x9f	/* Request position maintenance alg.
				   const, and max velocity */
#define	SM_INT_NXT_MOV	0xa0	/* Interrupt at start of next move */
#define	SM_INT_ALL_MOV	0xa1	/* Interrupt at the start of every move */
#define	SM_INT_NXT_NZ	0xa2	/* Interrupt at constant nonzero velocity
				   of next move */
#define SM_INT_ALL_NZ	0xa3	/* Interrupt at constant nonzero velocity
				   of every move */
#define	SM_INT_NXT_END	0xa4	/* Interrupt at next end of motion */
#define	SM_INT_ALL_END	0xa5	/* Interrupt at every end of motion */
#define	SM_INT_NXT_STL	0xa6	/* Interrupt on next stall detect */
#define	SM_INT_ALL_STL	0xa7	/* Interrupt on every stall detect */
#define	SM_INT_NXT_PLIM	0xa8	/* Interrupt the next time the motor 
				   hits the positive limit */
#define	SM_INT_ALL_PLIM	0xa9	/* Interrupt on every positive limit */
#define	SM_INT_NXT_NLIM	0xaa	/* Interrupt the next time the motor
				   hits the negative limit */
#define	SM_INT_ALL_NLIM	0xab	/* Interrupt on every negative limit */
#define	SM_INT_TRG	0xac	/* Interrupt on trigger X active */
#define	SM_INT_INHBT	0xaf	/* Inhibit all interrupts */
#define	SM_DEF_RATIO	0xb0	/* Define motor pulse to encoder pulse ratio */
#define SM_DEF_RESOLTN	0xb1	/* Define motor resolution */
#define	SM_DEF_BACK_SIG	0xb2	/* Define backlash sigma (motor steps)*/
#define	SM_DEF_ALG	0xb3	/* Define position maintenance algorithm
				   const, and max velocity */
#define	SM_DEF_TEETH	0xb4	/* Define  the number of rotor teeth */
#define	SM_DEF_DEADBAND	0xb5	/* Def the deadband region in encoder pulses */
#define	SM_DEF_REL_TRP	0xc8	/* Define move X as a relative, 
				   trapezoidal move */
#define	SM_DEF_ABS_TRP	0xcb	/* Define move X as an absolute
				   trapezoidal move */
#define	SM_DEF_CONT	0xce	/* Define move X as a continuous move */
#define SM_DEF_REL_CL	0xd4	/* Define move X, define it as relative,
				   closed-loop move */
#define	SM_DEF_ABS_CL	0xd5	/* Def move X as an abs, closed- loop move */
#define	SM_DEF_STSTP_VEL 0xd6	/* Define the start/stop velocity */
#define	SM_DEL_MOV_X	0xd7	/* Delete move X */
#define	SM_END_SEQ_DEF	0xd8	/* End definition of sequence buffer */
#define	SM_BEG_SEQ_DEF	0xd9	/* Begin definition of sequence buffer */
#define	SM_DEL_SEQ_X	0xda	/* Delete sequence buffer X */
#define	SM_LD_VD_DATA	0xe0	/* Place data into the velocity-distance buffer */
#define	SM_LD_VT_DATA	0xe1	/* Place data into the velocity-time buffer */
#define	SM_GET_FREE_BYT	0xe2	/* Request number of free bytes in vel-
				   streaming/sequence buffer */
#define	SM_DEF_VS_CMMD	0xee	/* Define command to be executed during
				   the velocity streaming buffer */
#define	SM_GET_NUM_REV	0xfd	/* Request software part number and revision */
#define	SM_TEST_SWITCH	0xff	/* Perform the test switch function */


#define VELOCITY_MODE	0
#define	MAX_COMMANDS	256
#define COMPU_INT_LEVEL   5

/* array of pointers to stepper motor driver cards present in system */
struct compumotor	*pcompu_motors[MAX_COMPU_MOTORS];

LOCAL SEMAPHORE compu_wakeup;	/* compumotor data request task semaphore */

/* response variables */
LOCAL SEMAPHORE smRespSem;		/* task semaphore */
LOCAL RING_ID smRespQ;			/* ring buffer */
int smRespId;				/* task id */
#define RESP_Q_SZ (RESPBUF_SZ * 50)	/* response ring buffer size */

/* interrupt buffers */
unsigned char	sm_responses[MAX_COMPU_MOTORS][RESPBUF_SZ];
unsigned short	counts[MAX_COMPU_MOTORS];

/* VME memory Short Address Space is set up in gta_init */
static int *compu_addr;

/* motor information */
struct compu_motor{
short	active;		/* flag to tell the oms_task if the motor is moving */
int	callback;	/* routine in database library to call with status  */
int	callback_arg;	/* argument to callback routine  */
short	update_count;
short	mode;
short	motor_resolution;
};
struct compu_motor	compu_motor_array[MAX_COMPU_MOTORS];

/* Forward reference. */
VOID compu_sm_reset();
VOID compu_sm_stat();

/* motor status - returned to the database library routines */
struct motor_data	compu_motor_data_array[MAX_COMPU_MOTORS];

/* moving status bit descriptions */
#define	CW_LIMIT	0x01	/* clockwise???? limit       */
#define	CCW_LIMIT	0x02	/* counter-clockwise???? limit       */
#define DIRECTION	0x08	/* direction bit        */
#define MOVING		0x10	/* moving status bit    */
#define	CONSTANT_VEL	0x20	/* constant velocity    */

/* directions in driver card-ese */
#define	CLKW	0	/* clockwise direction  */
#define	CCLKW	1	/* counter clockwise direction */

/*
 * Code Portions:
 *
 * smCmdTask	Task which writes commands to the hardware
 * smRespTask	Task which places reponses from the hardware into resp buffers
 * sm_intr	Interrupt Handler - collects response data from the hardware
 * sm_drv_init	Initializes all motors, semaphores, ring buffers and interrupts
 * sm_driver	Subroutine for outside world to issue commands to motors
 * motor_select	Subroutine to setting callback arg and verifying no other user
 * motor_deselect Subroutine to free the motor for other users
 *
 * Interaction Chart:
 *            --------------                          -------------------
 *          /               \                        /                   \ 
 *         |    smRespTask   |                      |      smCmdTask      |
 *          \               /                        \                   /
 *           ---------------                          ------------------- 
 *              ^        ^                                      |
 *        TAKES |        | GETS                                 |
 *              |        |                                      |
 *   --------------  ---------------                            |
 *   Resp Semaphore   Response Queue                            |
 *   --------------  ---------------                            |
 *              ^        ^                                      |
 *        GIVES |        | PUTS                                 |
 *              |        |                                      |
 *           ---------------                                    |
 *          /                \                                  |
 *         |   sm_intr        |                                 |
 *          \                /                                  |
 *           ---------------                                    |
 *                   ^ reads responses          writes commands |
 *                   |  from hardware               to hardware V 
 */

/*
 * COMPU_RESP_TASK
 *
 * converts readback from the compumotor 1830 cards into a structure that
 * is returned to the database library layer every .1 second while a motor
 * is moving
 */
compu_resp_task()
{
    unsigned char		resp[RESPBUF_SZ];
    register short		i;
    register struct motor_data	*pmotor_data;

    FOREVER {
        /* wait for somebody to wake us up */
#	ifdef V5_vxWorks
       	 	semTake (&smRespSem, WAIT_FOREVER);
#	else
       	 	semTake (&smRespSem);
#	endif

 	/* the response buffer contains: 	 	 	*/
	/* 0 -	motor number				 	*/
	/* 1 -	the command which solicited this response	*/
	/* 2 -	the first byte of the response		 	*/

	/* process requests in the command ring buffer */
        while (rngBufGet(smRespQ,(char *)resp,RESPBUF_SZ) == RESPBUF_SZ){
		pmotor_data = &compu_motor_data_array[resp[0]];

		/* convert argument */
		switch(resp[1]){

		case (SM_GET_VEL):
		{
			register long	*pvelocity = (long *)(&resp[3]);
			pmotor_data->velocity = *pvelocity; 

			break;
		}
		case (SM_GET_MOV_STAT):
		{
			register struct compu_motor	*pcompu_motor;
			register int 			(*psmcb_routine)();

			pcompu_motor = &compu_motor_array[resp[0]];
			pmotor_data->moving = (resp[2] & MOVING)?1:0;
			pmotor_data->constant_velocity = (resp[2] & CONSTANT_VEL)?1:0;
			pmotor_data->cw_limit = (resp[2] & CW_LIMIT)?1:0;
			pmotor_data->ccw_limit = (resp[2] & CCW_LIMIT)?1:0;
			pmotor_data->direction = (resp[2] & DIRECTION)?1:0;

			/* post every .1 second or not moving */
			if ((pcompu_motor->update_count-- <= 0) 
			  || (pmotor_data->moving == 0)){
				if (pcompu_motor->callback != 0){
					(int)psmcb_routine = pcompu_motor->callback;
					(*psmcb_routine)(pmotor_data,pcompu_motor->callback_arg);
				}
				if (pmotor_data->moving){
					/* motors are reported at 10 Hz */
					pcompu_motor->update_count = 3;
				}else{
                                        pcompu_motor->active = FALSE;
					pcompu_motor->update_count = 0;
				}
			}
			break;
		}
		case (SM_GET_ABS_ENC):
		{
			register long		*pencoder = (long *)(&resp[2]);
			pmotor_data->encoder_position = *pencoder;
			break;
		}
		case (SM_GET_Z_REL_POS):
		{
			register long	*pmotor = (long *)(&resp[4]);
			pmotor_data->motor_position = *pmotor;
			break;
		}
		case (SM_GET_CUR_DIR):
			pmotor_data->direction = (resp[2] == 0xff)?CLKW:CCLKW;
			break;
		}
        }
    }
}

/* Data request commands for the positional and velocity mode motors */
char compu_velo_reqs[] = { SM_GET_VEL,     SM_GET_MOV_STAT };
#define NUM_VEL_REQS	2
char compu_pos_reqs[] =  { SM_GET_ABS_ENC, SM_GET_Z_REL_POS, SM_GET_MOV_STAT };
#define NUM_POS_REQS	3
/*
 * COMPU_TASK
 *
 * task to solicit currnet status from the compumotor 1830 cards while they
 * are active
 */
compu_task()
{
    register short		inactive_count;
    register short		card;
    register short		i;
    register struct compumotor	*pmotor;
    register char		*preqs;

    /* inactive motors get monitored once every 2 seconds in case they are */
    /* being moved manually						   */
    inactive_count = 60;
    while(1){
	/* This task is run 30 times a second */
	taskDelay(2);
	for (card = 0; card < MAX_COMPU_MOTORS; card++){
	    pmotor = pcompu_motors[card];
	    if (pmotor == 0) continue;
	    if ((compu_motor_array[card].active)
	      || (inactive_count <=0)){
		if (compu_motor_array[card].mode == VELOCITY_MODE){
			preqs = &compu_velo_reqs[0];
			/* request status data */
			for (i = 0; i < NUM_VEL_REQS; i++,preqs++)
				compu_send_msg(pmotor,preqs,1);
		}else{
			preqs = &compu_pos_reqs[0];
			/* request status data */
			for (i = 0; i < NUM_POS_REQS; i++,preqs++)
				compu_send_msg(pmotor,preqs,1);
		}
	    }
	}
	if (--inactive_count < 0) inactive_count = 60;
    }
}

/*
 * COMPU_INTR
 *
 * interrupt vector for the compumotor 1830 card
 */
compu_intr(mdnum)
register int mdnum;
{
    register struct compumotor	*pmtr;	/* memory port to motor card */
    register int	key;

    key = intLock();

    /* pointer to the compumotor card interface */
    pmtr = pcompu_motors[mdnum];

    /* place the response byte into the appropriate response buffer */
    sm_responses[mdnum][counts[mdnum]] = pmtr->cm_idb;
    counts[mdnum]++;

    /* when the buffer is full pass it onto the repsonse task */
    if (counts[mdnum] == RESPBUF_SZ){
        if (rngBufPut(smRespQ,(char *)sm_responses[mdnum],RESPBUF_SZ) != RESPBUF_SZ)
            logMsg("smRespQ %d - Full\n",mdnum);
	else
	    semGive (&smRespSem);

	/* the zero-th byte is the motor number */
        counts[mdnum] = 1;        /* start with command */

        /* inform the hardware that the response is complete */
        pmtr->cm_cb = RD_LAST;
    }else{
	/* inform the hardware there is more to send */
        pmtr->cm_cb = RD_MORE;
    }

    intUnlock(key);
}

/*
 * COMPU_DRIVER_INIT
 *
 * initialization for the compumotor 1830 card
 */
long
compu_driver_init(){
    register short i;
    int status;
    struct compumotor    *pmtr;    /* memory port to motor card */
    int    cok = CBBR;			/*to reset board */
    short	none_found;		/* flags a steppermotor is present */

   /* intialize each driver which is present */
    none_found = TRUE;
    rebootHookAdd((FUNCPTR)compu_sm_reset);
    status = sysBusToLocalAdrs(
		VME_AM_SUP_SHORT_IO,
		(char *)sm_addrs[CM57_83E], 
		(char **)&compu_addr);
    if (status != OK){ 
      printf("%s: failed to map A16 base\n", __FILE__); 
      return ERROR;
    }
   
    pmtr = (struct compumotor *)compu_addr;  
    for (i = 0; i < MAX_COMPU_MOTORS; i++) {
       pmtr =  (struct compumotor *)((int)pmtr + (i<<8)); 

	/* initialize when card is present */

        if (vxMemProbe(&pmtr->cm_cb,WRITE,1,&cok) != ERROR){
	    none_found = FALSE;
            pcompu_motors[i] = pmtr;		/* ptr to interface */
            intConnect((MD_INT_BASE+i)*4,compu_intr,i);	/* interrupt enable */
            sysIntEnable(COMPU_INT_LEVEL); 

            /* init interrupt receive buffers */
            sm_responses[i][0] = i;	/* motor number */
            counts[i] = 1;		/* buffer index */
        }else{
            pcompu_motors[i] = 0;	/* flags no board is present */
        }
    }
    if (none_found) return(0);

    /* initialize the response task ring buffer */
    if ((smRespQ = rngCreate(RESP_Q_SZ)) == (RING_ID)NULL)
        panic ("sm_init: cmRespQ\n");

    /* intialize the semaphores which awakens the sleeping           *
     * stepper motor command task and the stepper motor response task */
    semInit(&smRespSem);
    semInit(&compu_wakeup);

    /* spawn the sleeping motor driver command and response tasks */
    smRespId = 
	taskSpawn("compu_resp_task",SMRESP_PRI,SMRESP_OPT,SMRESP_STACK,compu_resp_task);
    taskSpawn("compu_task",SMRESP_PRI,SMRESP_OPT,2000,compu_task);

    return(0);
}

short trigger = 0;
/*
 * COMPU_DRIVER
 *
 * driver interface to the database library layer
 */
compu_driver(card,value_flag,arg1,arg2)
register short	card;
short		value_flag;
register int	arg1;
register int	arg2;
{
        register int	*pint;
        register short	*pshort;
	short		j,i;
	char		compu_msg[20];

	/* verify the stepper motor driver card is present */
	if ((card < 0) || (card > MAX_COMPU_MOTORS) || (!pcompu_motors[card]))
		return (-1);

	switch (value_flag){
	case (SM_MODE):
		/* set the motor mode */
		compu_motor_array[card].mode = arg1;
		break;

	case (SM_VELOCITY):
		compu_motor_data_array[card].velocity = arg1;
		compu_motor_data_array[card].accel = arg2;

		/* set the velocity */
		compu_msg[0] = SM_DEF_VEL_ACC;
		compu_msg[1] = 0;		/* time is in seconds */
		compu_msg[2] = 0;
		pint = (int *)&compu_msg[3];	/* velocity */
		*pint = arg1;
		pint++;				/* acceleration */
		*pint = arg2;
		compu_send_msg(pcompu_motors[card],compu_msg,11);

		break;

	case (SM_MOVE):
		if (compu_motor_array[card].mode == VELOCITY_MODE)
			return;
i = 0;
switch (trigger){
case (0):
		/* move the motor */
		compu_msg[i++] = SM_MOV_REL_POS;
		pint = (int *)&compu_msg[i];
		*pint = arg1;
		i += 4;
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		break;
case (1): /* test sequnce buffer */
		compu_msg[i++] = 0xda;		/* delete sequence buffer */
		compu_msg[i++] = 01;		/* buffer 1 */
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0xd9;		/* fill sequence buffer */
		compu_msg[i++] = 01;		/* buffer 1 */
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = SM_MOV_REL_POS;
		pint = (int *)&compu_msg[i];
		*pint = arg1;
		i += 4;
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0xd8;		/* end sequence buffer */
		compu_msg[i++] = 0x41;		/* perform sequence buffer */
		compu_msg[i++] = 0x01;		/* buffer 1 */
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		break;
case (2): /* test move buffer */
		compu_msg[i++] = 0xc8;
		compu_msg[i++] = 0x12;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x04;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x04;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		pint = (int *)&compu_msg[i];
		*pint = arg1;
		i += 4;
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0x40;
		compu_msg[i++] = 0x12;
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		break;
case (3): /* test sequence buffer with move buffer */
		compu_msg[i++] = 0xc8;
		compu_msg[i++] = 0x12;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x04;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x04;
		compu_msg[i++] = 0x00;
		compu_msg[i++] = 0x00;
		pint = (int *)&compu_msg[i];
		*pint = arg1;
		i += 4;
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0xda;		/* delete sequence buffer */
		compu_msg[i++] = 01;		/* buffer 1 */
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0xd9;		/* fill sequence buffer */
		compu_msg[i++] = 01;		/* buffer 1 */
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0x40;
		compu_msg[i++] = 0x12;
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0xd8;		/* end sequence buffer */
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		compu_msg[i++] = 0x41;		/* perform sequence buffer */
		compu_msg[i++] = 0x01;		/* buffer 1 */
	compu_send_msg(pcompu_motors[card],compu_msg,i);
	i = 0;
		break;
case (4): /* test sequence buffer with move buffer and trigger */
		compu_msg[i++] = 0xc8;
		compu_msg[i++] = 0x12;

		compu_msg[i++] = 0x00;
		pint = (int *)&compu_msg[i];
		*pint = compu_motor_data_array[card].velocity;
		i += 4;
		pint++;
		*pint = compu_motor_data_array[card].accel;
		i += 4;
		pint++;
		*pint = arg1;
		i += 4;
		compu_send_msg(pcompu_motors[card],compu_msg,i);
		i = 0;
		compu_msg[i++] = 0xda;		/* delete sequence buffer */
		compu_msg[i++] = 01;		/* buffer 1 */
		compu_send_msg(pcompu_motors[card],compu_msg,i);
		i = 0;
		compu_msg[i++] = 0xd9;		/* fill sequence buffer */
		compu_msg[i++] = 01;		/* buffer 1 */
		compu_send_msg(pcompu_motors[card],compu_msg,i);
		i = 0;
		compu_msg[i++] = 0x2c;		/* wait for trigger */
		compu_msg[i++] = 1;		/*trigger 1 */
		compu_msg[i++] = 1;		/* don't care about state */
		compu_send_msg(pcompu_motors[card],compu_msg,i);
		i = 0;
		compu_msg[i++] = 0x40;
		compu_msg[i++] = 0x12;
		compu_send_msg(pcompu_motors[card],compu_msg,i);
		i = 0;
		compu_msg[i++] = 0xd8;		/* end sequence buffer */
		compu_send_msg(pcompu_motors[card],compu_msg,i);
		i = 0;
		compu_msg[i++] = 0x41;		/* perform sequence buffer */
		compu_msg[i++] = 0x01;		/* buffer 1 */
		compu_send_msg(pcompu_motors[card],compu_msg,i);
		i = 0;
		break;
}
for (j = 0; j < i; j++){
printf("%x ",compu_msg[j]);
}
/*		compu_send_msg(pcompu_motors[card],compu_msg,i);
*/
		/* set the motor to active */
		compu_motor_array[card].active = TRUE;

		/* wakeup the compu task */
		semGive(&compu_wakeup);

		break;

	case (SM_MOTION):
		if (arg1 == 0){
			compu_msg[0] = SM_STOP;
			compu_send_msg(pcompu_motors[card],compu_msg,1);
		}else if (compu_motor_array[card].mode == VELOCITY_MODE){
			compu_msg[0] = SM_MOV_DEFAULT;
			compu_msg[1] = arg2;		/* direction */
			compu_send_msg(pcompu_motors[card],compu_msg,2);
			compu_motor_array[card].active = TRUE;
		}

		/* wakeup the compu task */
		semGive(&compu_wakeup);
		break;

	case (SM_CALLBACK):
		/* put the callback routine and argument into the data array */
		i = 0;
		if (compu_motor_array[card].callback != 0) return(-1);
		compu_motor_array[card].callback = arg1;
		compu_motor_array[card].callback_arg = arg2;
		break;

	case (SM_SET_HOME):
		if (compu_motor_array[card].mode == VELOCITY_MODE)
			return;

		/* set the motor and encoder position to zero */
		compu_msg[0] = SM_DEF_ABS_ZERO;
		compu_send_msg(pcompu_motors[card],compu_msg,1);

		break;
		
	case (SM_ENCODER_RATIO):
		compu_motor_array[card].motor_resolution = arg1;

		/* set the encoder ratio */
		compu_msg[0] = SM_DEF_RATIO;
		pshort = (short *)&compu_msg[1];
		*pshort = arg1;			/* motor resolution */
		pshort++;
		*pshort = arg2;			/* encoder resolution */
		compu_send_msg(pcompu_motors[card],compu_msg,5);

		/* set the motor resolution */
		compu_msg[0] = SM_DEF_RESOLTN;
		pshort = (short *)&compu_msg[1];
		*pshort = 0;
		pshort++;
		*pshort = arg1;			/* motor resolution */
		compu_send_msg(pcompu_motors[card],compu_msg,5);

		break;
	case (SM_READ):
		/* set the motor to active */
		compu_motor_array[card].active = TRUE;

		/* wakeup the compu task */
		semGive(&compu_wakeup);

		break;

	}
	return (OK);
}

/*
 * COMPU_SEND_MSG
 *
 * send a message to the compumotor 1830
 */
int	wait_count;
compu_send_msg(pmotor,pmsg,count)
register struct	compumotor	*pmotor;
register char			*pmsg;
register short			count;
{
        /* write out this command one byte at a time */
	while (count){

                /* wait for the driver to be ready */
                while ((pmotor->cm_cb & SBIRDY) == 0){
			taskDelay(0);
                	wait_count++;
		}

                /* next byte in the input data buffer of compumotor */
                pmotor->cm_idb = *pmsg;
    		pmsg++;
		count--;

                /* tell compumotor more or complete */
                if (count == 0){
			pmotor->cm_cb = SND_LAST;
		}else{
			pmotor->cm_cb = SND_MORE;
		}
        }
}


/*
 * COMPU_SM_IO_REPORT
 *
 * send a message to the compumotor 1830
 */

long compu_sm_io_report(level)
  short int level;
 {
   register int i;

	for (i = 0; i < MAX_COMPU_MOTORS; i++){
		if (pcompu_motors[i]){	
                       
                    printf("SM: CM1830:     card %d\n",i);
                    if (level > 0)
                        compu_sm_stat(i); 
                }
        }

	return OK;
 }

VOID compu_sm_stat(compu_num)
 short int compu_num;
 {
 struct motor_data  *pmotor_data;
  printf("\tCW limit = %d\t,CCW limit = %d\tMoving = %d\tDirection = %d\n",
         compu_motor_data_array[compu_num].cw_limit,
         compu_motor_data_array[compu_num].ccw_limit,
         compu_motor_data_array[compu_num].moving,
         compu_motor_data_array[compu_num].direction);

  printf("\tConstant Velocity = %d\t, Velocity = %d\t \n",
         compu_motor_data_array[compu_num].constant_velocity,
         compu_motor_data_array[compu_num].velocity);

  printf("\tAcceleration = %d\tEncoder Position = %d\tMotor Position = %d\n", 
         compu_motor_data_array[compu_num].accel,
         compu_motor_data_array[compu_num].encoder_position,
         compu_motor_data_array[compu_num].motor_position);
 }

/*
 *
 * Subroutine to be called during a CTL X reboot. Inhibits interrupts. 
 *
 */

VOID compu_sm_reset()
 {
   short int i;
   char	compu_msg[20];

	for (i = 0; i < MAX_COMPU_MOTORS; i++){
		if (pcompu_motors[i]){	
                    compu_msg[0] = SM_INT_INHBT;  
                    compu_send_msg(pcompu_motors[i],compu_msg,1);
                }
        }
 }

