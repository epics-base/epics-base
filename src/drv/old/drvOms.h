/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* oms_sm_driver.h */
/* base/src/drv $Id$ */
/*
 * headers that are used to interface to the 
 * Oregon Micro Systems six axis stepper motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        02-25-90
 */
#define	MAX_OMS_CARDS		8
#define	MAX_OMS_CHANNELS	8


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

