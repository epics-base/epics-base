/* oms_sm_driver.h */
/* share/src/drv @(#)oms_sm_driver.h	1.1     10/17/90 */
/*
 * headers that are used to interface to the 
 * Oregon Micro Systems six axis stepper motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        02-25-90
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
 * .01	04-12-90	lrd	only allow one connection to each motor
 * .02	11-13-90	lrd	add status bit definitions for encoder commands
 */
#define	MAX_OMS_CARDS		8
#define	MAX_OMS_CHANNELS	6


/* motor information */
struct oms_motor{
short	active;		/* flag to tell the oms_task if the motor is moving */
int	callback;	/* routine in database library to call with status  */
int	callback_arg;	/* argument to callback routine  */
short	update_count;
short	stop_count;
};

#define	MIRQE			0x80
#define	TRANSMIT_BUFFER_EMPTY	0x40
#define	INPUT_BUFFER_FULL	0x20
#define	MDONE			0x10
#define	OMS_ENCODER		0x04
#define	OMS_CMD_ERROR		0x01

struct vmex_motor{
	char	unused0;
	char	data;
	char	unused1;
	char	done;
	char	unused2;
	char	control;
	char	unused3;
	char	status;
	char	unused4;
	char	vector;
	char	unused5[6];
};

/* oms message defines */
#define	OMS_MSG_SZ	32		/* response message size */
#define OMS_RESP_Q_SZ	(OMS_MSG_SZ*500)	/* response ring buffer size */

