/* drvOms.c */
/* base/src/drv $Id$ */
/*
 * subroutines and tasks that are used to interface to the 
 * Oregon Micro Systems six axis stepper motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        12-28-89
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
 * Modification Log
 * -----------------
 * .01	02-07-90	lrd	add command to read status
 * .02	02-09-90	lrd	removed the clamp on the MR command
 * .03	03-22-90	lrd	added the acceleration
 * .04	04-12-90	lrd	only allow one record to connect to each motor
 * .05	04-28-90	lrd	request motor data at 10 Hz. Needed delay to let
 *				motor start moving
 * .06	04-30-90	lrd	fix interrupt vectors for more than one motor
 * .07	07-31-90	lrd	lock the communication to the oms card for
 *				one user
 * .08	08-01-90	lrd	fix turn off of auxilary output when move is
 *				complete
 * .09	08-01-90	lrd	fix the initialization of the card to only
 * 				enable the buffer full interrupt
 * .10	10-23-90	lrd	clamp the send value to something the motor
 *				driver can handle
 * .11	11-13-90	lrd	add intelligence in looking for an OMS card with
 *				an encoder
 * .12  05-15-91        lrd     add initialization of encoder and motor position
 * .13	09-05-91	joh	updated for v5 vxWorks
 * .14  12-10-91	 bg	added sysBusToLocalAddrs().  Added 
 *                              compu_sm_driver.c.	
 * .15  03-10-92	 bg	Added level to io_report and gave 
 *                              compu_sm_io_report() the ability to print out
 *                              contents of motor_data array if level > 1.	
 * .16	06-26-92	 bg	Combined drvOms.c with oms_driver.c
 * .17  06-29-92	joh	took file pointer arg out of io report
 * .18  08-11-92	joh	io report format cleanup	
 * .19  08-02-93	mrk	Added call to taskwdInsert
 * .20  08-05-93	jbk	took out 200000 pulse limit
 * .21  02-28-94	mrk	Replaced itob by cvtLongToString
 * .22  05-05-94	kornke	Now supports VMEX-8 and VMEX-44E
 *				(8 axis s'motors and 4 encoded s'motors)
 * .23  04-09-96	ric	Added SM_FIND_LIMIT, SM_FIND_HOME
 */

/* data requests are made from the oms_task at
 * a rate of 10Hz when a motor is active
 * post every .1 second or not moving
 * requests are sent at 10Hz in oms_task
 */

/* drvOms.c -  Driver Support Routines for Oms */
#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<intLib.h>
#include	<vxLib.h>
#include	<rebootLib.h>
#include	<taskLib.h>
#include	<rngLib.h>             /* library for ring buffer support */
#include	<semLib.h>
#include        <vme.h>                /* library to support sysBusToLocalAdrs. */
#include	<iv.h>
#include	<logLib.h>
#include	<string.h>

#include	<dbDefs.h>
#include	<epicsPrint.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<drvOms.h>
#include	<steppermotor.h>
#include	<taskwd.h>

#define OMS_INT_LEV 5

/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvOms={
	2,
	report,
	init};

static long report(level)
    int level;
{
    oms_io_report(level);
    return(0);
}


static long init()
{
    oms_driver_init();
    return(0);
}

/*
 * a rate of 10Hz when a motor is active    
 * post every .1 second or not moving
 * requests are sent at 10Hz in oms_task
 */

/* addresses of all motors present */
struct vmex_motor	*oms_motor_present[MAX_OMS_CARDS];
char			encoder_present[MAX_OMS_CARDS];

static char *localaddr;   /* Local address used to address cards. */  

/* motor information */
struct oms_motor	oms_motor_array[MAX_OMS_CARDS][MAX_OMS_CHANNELS];

/* motor status - returned to the database library routines */
struct motor_data	motor_data_array[MAX_OMS_CARDS][MAX_OMS_CHANNELS];

char	oms_motor_specifier[MAX_OMS_CHANNELS+1] = {'X','Y','Z','T','U','V'};

/* scan task parameters */
LOCAL SEM_ID		oms_wakeup;	/* oms_task wakeup semaphore */
LOCAL SEM_ID		oms_send_sem;	/* oms_task wakeup semaphore */

/* response task variables */
LOCAL SEM_ID		oms_resp_sem;	/* wakeup semaphore for the resp task */
LOCAL RING_ID 		oms_resp_q;	/* queue of responses */

/* interrupt routine message buffers */
int 	resp_inx[MAX_OMS_CARDS];
char	read_buffer[MAX_OMS_CARDS][34];

/* forward reference. */
VOID oms_reset();


int oms_intr(card)
register short	card;
{
    register struct vmex_motor	*pmotor;
    register int		key;
    register char		inp_char;
    register int		*pinx;

    key = intLock();
    
    /* pointer to this motor */
    if ((pmotor = oms_motor_present[card]) == 0){
	intUnlock(key);
	return(0);
    }
    pinx = &resp_inx[card];

    /* get the next character */
    if (pmotor->status & INPUT_BUFFER_FULL){

	/* check for end of command */
	inp_char = pmotor->data;
	if (((inp_char == 0xa) || (inp_char == 0xd)) && (*pinx > 0)){
	    /* check if the encoder command caused an error */
	    if (pmotor->status & (OMS_ENCODER | OMS_CMD_ERROR))
		encoder_present[card] = FALSE;
	    else encoder_present[card] = TRUE;
	    /* terminate and send the message */
	    read_buffer[card][*pinx] = 0;
	    if (rngBufPut(oms_resp_q,read_buffer[card],OMS_MSG_SZ)
	      != OMS_MSG_SZ){
          	logMsg("oms_resp_q full\n");
	    }else{
		semGive (oms_resp_sem);
	    }
	    *pinx = 0;        /* reset buffer */

	/* save printable ascii characters */
	}else if ((inp_char >= 0x20) && (inp_char < 0x7f)){
	    if (*pinx == 0){
		read_buffer[card][*pinx] = card;
		*pinx += 1;
	    }
	    if (*pinx < OMS_MSG_SZ){
		read_buffer[card][*pinx] = inp_char;
		*pinx += 1;
	    }else{
           	logMsg("oms intr buffer full\n");
	    }
	}
    }
    intUnlock(key);
    return(0);
}

/*
 * STEPPER MOTOR RESPONSE TASK - REMOVES RESPONSES FROM the INTERRUPT QUEUE
 */
short		oms_channel[MAX_OMS_CARDS];
short		oms_state[MAX_OMS_CARDS];
char		off_msg[40];
int		oms_debug = 0;
int		oms_compare = 3;
int oms_resp_task()
{
    unsigned char		resp[OMS_MSG_SZ*4];
    register struct motor_data	*pmotor_data_array;
    register struct oms_motor	*poms_motor_array;
    short			*pchannel;
    int				(*psmcb_routine)();
    register short		*pstate;
    register short		card;
    int				temp;

    FOREVER {
        /* wait for somebody to wake us up */
       	semTake (oms_resp_sem, WAIT_FOREVER);
        /* process requests in the command ring buffer */
        while (rngBufGet(oms_resp_q,resp,OMS_MSG_SZ) == OMS_MSG_SZ){
            if (oms_debug) 
              printf("card: %d  msg:%s\n",resp[0],&resp[1]);
	    /* get the card number and pointers to the state and channel */
	    card = resp[0];
	    pchannel = &oms_channel[card];
	    pstate = &oms_state[card];
	    pmotor_data_array = &motor_data_array[card][*pchannel];
	    poms_motor_array = &oms_motor_array[card][*pchannel];

	    /* motor selection */
            if (resp[1] == 'A')
                {
                    switch (resp[2])
                        {
                        case 'X':
                                *pchannel = 0;
                                break;
                        case 'Y':
                                *pchannel = 1;
                                break;
                        case 'Z':
                                *pchannel = 2;
                                break;
                        case 'T':
                                *pchannel = 3;
                                break;
                        case 'U':
                                *pchannel = 4;
                                break;
                        case 'V':
                                *pchannel = 5;
                                break;
                        case 'R':
                                *pchannel = 6;
                                break;
                        case 'S':
                                *pchannel = 7;
                                break;
                        }

		    *pstate = 0;
	    /* position readback */
	    }else if (resp[1] == 'R'){
		    if (resp[2] == 'E')		*pstate = 1;
		    else if (resp[2] == 'P')	*pstate = 2;
		    else if (resp[2] == 'A')	*pstate = 3;
		    else			*pstate = 0;

	    /* convert encoder position */
	    }else if (*pstate == 1){
		    sscanf(&resp[1],"%d",&temp);
		    pmotor_data_array->encoder_position = temp;
		    *pstate = 0;
	    /* convert motor position */
	    /* use the motor position for detecting end of motion because */
	    /* all motors use this, not all motors have encoders	  */
	    }else if (*pstate == 2){
		    sscanf(&resp[1],"%d",&temp);
		    if ((pmotor_data_array->motor_position == temp)
		      && (poms_motor_array->active == TRUE)){
			poms_motor_array->stop_count++;
			if (poms_motor_array->stop_count >= oms_compare) {
				poms_motor_array->active = FALSE;
				strcpy(off_msg,"AB\nAN\n");
				off_msg[1] = oms_motor_specifier[*pchannel];
				oms_send_msg(oms_motor_present[card],off_msg);
				poms_motor_array->stop_count = 0;
			}

		    }else{
			pmotor_data_array->motor_position = temp;
			poms_motor_array->stop_count = 0;
		    }
		    *pstate = 0;

	    /* convert axis status */
	    }else if (*pstate == 3){
		    pmotor_data_array->ccw_limit = 0;
		    pmotor_data_array->cw_limit = 0;
		    if (resp[1] == 'P'){
			pmotor_data_array->direction = 0;
			if (resp[3] == 'L'){
			    pmotor_data_array->ccw_limit= 1;
			}
		    }else{
			pmotor_data_array->direction = 1;
			if (resp[3] == 'L'){
			    pmotor_data_array->cw_limit = 1;
			}
		    }
		    pmotor_data_array->moving = poms_motor_array->active;
		    *pstate = 0;

 		    /* post every .1 second or not moving */
		    if ((poms_motor_array->update_count-- <= 0) 
		      || (pmotor_data_array->moving == 0)){
		        if (poms_motor_array->callback != 0){
			    (int)psmcb_routine = poms_motor_array->callback;
			    (*psmcb_routine)(pmotor_data_array,poms_motor_array->callback_arg);
			}
			if (pmotor_data_array->moving){
			    poms_motor_array->update_count = 2;
			}else{
			    poms_motor_array->update_count = 0;
			}
		    }
	    /* reset state */
	    }else{
		    *pstate = 0;
	    }
        }
    }
}

int oms_task()
{
    register short		motor_active;
    register short		card,channel;
    char			oms_msg[40];
    register struct vmex_motor	*pmotor;

    while(1){
	semTake(oms_wakeup, WAIT_FOREVER);
	motor_active = TRUE;
	while (motor_active){
	    motor_active = FALSE;
	    taskDelay(2);
	    for (channel = 0; channel < MAX_OMS_CHANNELS; channel++){
		pmotor = oms_motor_present[0];
		for (card = 0; card < MAX_OMS_CARDS; card++,pmotor++){
		    if (pmotor == 0) continue;
		    if (oms_motor_array[card][channel].active){
			motor_active = TRUE;

			/* request status data */
			if ((channel <= 3) && (encoder_present[card]))
				strcpy(oms_msg,"A?\nRE\nRP\nRA\n");
			else
				strcpy(oms_msg,"A?\nRP\nRA\n");
			oms_msg[1] = oms_motor_specifier[channel];
			oms_send_msg(oms_motor_present[card],oms_msg);
		    }
		}
	    }
	}
    }
}

/*
 * OMS_DRIVER_INIT
 *
 * initialize all oms drivers present
 */
int oms_driver_init(){
	struct vmex_motor	*pmotor;
        short                   i,j,got_one;
        int			status;
	short			dummy;
	int			taskId;

	for (i = 0; i < MAX_OMS_CARDS; i++){
		resp_inx[i] = 0;
	}

	/* find all cards present */
	got_one = FALSE;
        
        status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,sm_addrs[OMS_6AXIS], &localaddr);
        if (status != OK){ 
           logMsg("Addressing error in oms driver\n");
           return(ERROR);
        }
        rebootHookAdd(oms_reset);  
        pmotor = (struct vmex_motor *)localaddr;
	for (i = 0; i < MAX_OMS_CARDS; i++,pmotor++){
                if (vxMemProbe(pmotor,READ,sizeof(short),&dummy) == OK){
                        got_one = TRUE;

			/* intialize the motor */
			pmotor->control = 0;

			/* interrupt vector */
			pmotor->vector = 0x80+i;
			intConnect(INUM_TO_IVEC(0x80+i),oms_intr,i);
                        sysIntEnable(OMS_INT_LEV);

			/* enable interrupt on input buffer full */
			pmotor->control |= 0xa0;

			/* mark the motor card as present */
			oms_motor_present[i] = pmotor;
			encoder_present[i] = TRUE;
		}else{
			/* mark the motor card as not present */
			oms_motor_present[i] = 0;
		}
		for (j = 0; j < MAX_OMS_CHANNELS; j++){
			oms_motor_array[i][j].active = FALSE;
			oms_motor_array[i][j].callback = 0;
			motor_data_array[i][j].encoder_position = 0;
			motor_data_array[i][j].motor_position = 0;
		}
	}

	if (got_one){
		/* initialize the command task ring buffer */
		if ((oms_resp_q = rngCreate(OMS_RESP_Q_SZ)) == (RING_ID)NULL)
			panic ("oms_driver_init: oms_resp_q not created\n");

		/* initialize the oms response task semaphore */
		if(!(oms_resp_sem=semBCreate(SEM_Q_FIFO,SEM_EMPTY)))
			errMessage(0,"semBcreate failed in oms_driver_init");
		/* intialize the data request wakeup semaphore */
		if(!(oms_wakeup=semBCreate(SEM_Q_FIFO,SEM_EMPTY)))
			errMessage(0,"semBcreate failed in oms_driver_init");
		/* oms card mutual exclusion semaphore */
		if(!(oms_send_sem=semBCreate(SEM_Q_FIFO,SEM_FULL)))
			errMessage(0,"semBcreate failed in oms_driver_init");
		/* spawn the motor data request task */
		taskId = taskSpawn("oms_task",42,VX_FP_TASK,8000,oms_task);
		taskwdInsert(taskId,NULL,NULL);

		/* spawn the motor data request task */
		taskId = taskSpawn("oms_resp_task",42,VX_FP_TASK,8000,oms_resp_task);
		taskwdInsert(taskId,NULL,NULL);

		/* enable echo on each motor that is present */
              
               pmotor = (struct vmex_motor *)localaddr;

		for (i = 0; i < MAX_OMS_CARDS; i++,pmotor++){
			if (oms_motor_present[i]){

				/* give it the initialization commands */
				oms_send_msg(pmotor,"EN\nWY\n"); 
                         }
		}
	}
	return(0);
}

/*
 * OMS_DRIVER
 *
 * interface routine called from the database library
 */
#define	MOTOR_POS	1
int oms_driver(card,channel,value_flag,arg1,arg2)
register short	card;
register short	channel;
short		 value_flag;
register int	arg1;
int		arg2;
{
	char	oms_move_msg[100];
	short	i,count;

	if (!oms_motor_present[card]) return(-1);
	switch (value_flag){
	case (SM_MODE):			/* only supports positional mode */
		break;
	case (SM_VELOCITY):
		/* set the velocity */
		motor_data_array[card][channel].velocity = arg1;
		strcpy(oms_move_msg,"A?\nVL");
		oms_move_msg[MOTOR_POS] = oms_motor_specifier[channel];
		count = cvtLongToString(arg1,&oms_move_msg[5]);
		strcat(oms_move_msg,"\n");
		oms_send_msg(oms_motor_present[card],oms_move_msg);

		/* set the acceleration */
		strcpy(oms_move_msg,"A?\nAC");
		oms_move_msg[MOTOR_POS] = oms_motor_specifier[channel];
		count = cvtLongToString(arg2,&oms_move_msg[5]);
		strcat(oms_move_msg,"\n");
		oms_send_msg(oms_motor_present[card],oms_move_msg);

		break;

	case (SM_FIND_HOME):	/* Move to a home switch */
		break;		/* Not supported by this device */

	case (SM_FIND_LIMIT):	/* Move to a limit switch */
		arg1 = arg1 >= 0 ? 0x000fffff : -0x000fffff;
		/* break purposely left out to continue with SM_MOVE */

	case (SM_MOVE):
		/* move the motor */
		strcpy(oms_move_msg,"A?\nAF\nMR");
		oms_move_msg[1] = oms_motor_specifier[channel];
		count = cvtLongToString(arg1,&oms_move_msg[8]);
		strcat(oms_move_msg,"\nGO\n");
		oms_send_msg(oms_motor_present[card],oms_move_msg);

		/* set the motor to active */
		oms_motor_array[card][channel].active = TRUE;

		/* wakeup the oms task */
		semGive(oms_wakeup);

		break;
	case (SM_MOTION):
		/* stop the motor */
		strcpy(oms_move_msg,"A?ST\n");
		if (arg1 == 0){
			oms_move_msg[1] = oms_motor_specifier[channel];
		}else{
			return(0);
		}
		oms_send_msg(oms_motor_present[card],oms_move_msg);

		/* wakeup the oms task */
		semGive(oms_wakeup);
		break;

	case (SM_CALLBACK):
		i = 0;
		if (oms_motor_array[card][channel].callback != 0) return(-1);
		oms_motor_array[card][channel].callback = arg1;
		oms_motor_array[card][channel].callback_arg = arg2;
		break;

	/* reset encoder and motor positions to zero */
	case (SM_SET_HOME):
		/* load the position to be zero */
		strcpy(oms_move_msg,"A?\nLP0\n");
		oms_move_msg[1] = oms_motor_specifier[channel];
		oms_send_msg(oms_motor_present[card],oms_move_msg);

		/* set the motor to active */
		oms_motor_array[card][channel].active = TRUE;

		/* wakeup the oms task */
		semGive(oms_wakeup);

		break;
		
	case (SM_ENCODER_RATIO):
		/* set the encoder ratio */
		/* The "ER" command changes how far a pulse will move the */
		/* motor. 						  */
		/* As this is not the desired action this command is not  */
		/* implemented here.					  */
		break;

	case (SM_READ):
		/* set the motor to active */
		oms_motor_array[card][channel].active = TRUE;

		/* wakeup the oms task */
		semGive(oms_wakeup);

		break;
	}
	return(0);
}

char	last_msg[80];
int	oms_count,oms_icount,oms_illcmd,oms_sleep,oms_isleep;
/*
 * OMS_SEND_MSG
 *
 * Gives messages to the OMS card
 */
int oms_send_msg(pmotor,pmsg)
struct vmex_motor		*pmotor;
register char			*pmsg;
{
int	i;
i = 0;
	/* take the mutual exclusion semaphore */
      	semTake(oms_send_sem, WAIT_FOREVER);
	while (*pmsg){
		if (pmotor->status & 0x01){
			oms_illcmd++;
			while ((pmotor->status & TRANSMIT_BUFFER_EMPTY) == 0){
				oms_icount++;
				if ((oms_icount % 5) == 0){
					oms_isleep++;
/* A taskDelay makes a 68040 wait frequently */
					/*taskDelay(1);*/
				}
			}
			pmotor->data = 0x19;	/* reset */
		}else{
			while ((pmotor->status & TRANSMIT_BUFFER_EMPTY) == 0){
				oms_count++;
				if ((oms_count % 5) == 0){
					oms_sleep++;
/* A taskDelay makes a 68040 wait frequently */
					/*taskDelay(1);*/
				}
			}
			pmotor->data = *pmsg;
			pmsg++;
		}
	}
	/* release the mutual exclusion semaphore */
        semGive(oms_send_sem);
	return(0);
}

int oms_io_report(level)
short int level;
{
    register short int i,j;

        for (i = 0; i < MAX_OMS_CARDS; i++) {
                if (oms_motor_present[i]){  
                        printf("SM: OMS:\tcard %d\n",i);
	        	for (j = 0; j < MAX_OMS_CHANNELS; j++){
                           if (level > 0)
                              oms_sm_stat(i,j); 
                       }  
                }
             
        }
	return(0);
 }

VOID oms_sm_stat(card,channel)
 short int card,channel;
 {

  printf("SM: OMS: Card = %d,channel = %d\n",card,channel);
  printf("\tCW limit = %d\tCCW limit = %d\tMoving = %d\tDirection = %d\n",
         motor_data_array[card][channel].cw_limit,
         motor_data_array[card][channel].ccw_limit,
         motor_data_array[card][channel].moving,
         motor_data_array[card][channel].direction);

  printf("\tConstant Velocity = %ld\tVelocity = %ld\t \n",
         motor_data_array[card][channel].constant_velocity,
         motor_data_array[card][channel].velocity);

  printf("\tAcceleration = %ld\tEncoder Position = %ld\tMotor Position = %ld\n", 
         motor_data_array[card][channel].accel,
         motor_data_array[card][channel] .encoder_position,
         motor_data_array[card][channel].motor_position);
 }


/*
 *
 *  Disables interrupts. Called on CTL X reboot.
 *
 */
 
VOID oms_reset(){
	short 			card;        
	struct vmex_motor	*pmotor;
	short			dummy;

	pmotor = (struct vmex_motor *)localaddr;
	for (card = 0; card < MAX_OMS_CARDS; card++,pmotor++){
		if(vxMemProbe(pmotor,READ,sizeof(short),&dummy) == OK){
                        pmotor->control &= 0x5f;
                 } 
        }

}
