
/* oms_sm_driver.h */
/* share/src/drv @(#)oms_sm_driver.h	1.1     10/17/90 */

/*
 * oms_sm_driver.h
 *
 * headers that are used to interface to the 
 * Oregon Micro Systems six axis stepper motor drivers
 *
 * Author:      Bob Dalesio
 * Date:        02-25-90
 *
 *      Control System Software for the GTA Project
 *
 *      Copyright 1988, 1989, 1990 the Regents of the University of California.
 *
 *      This software was produced under a U.S. Government contract
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *      operated by the University of California for the U.S. Department
 *      of Energy.
 *
 *      Developed by the Controls and Automation Group (AT-8)
 *      Accelerator Technology Division
 *      Los Alamos National Laboratory
 *
 *      Direct inqueries to:
 *      Bob Dalesio, AT-8, Mail Stop H820
 *      Los Alamos National Laboratory
 *      Los Alamos, New Mexico 87545
 *      Phone: (505) 667-3414
 *      E-mail: dalesio@luke.lanl.gov
 *
 * Modification Log:
 * -----------------
 * .01	04-12-90	lrd	only allow one connection to each motor
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

#define SM_BASE_ADDR	0xfffc00

#define	MIRQE	0x80
#define	TRANSMIT_BUFFER_EMPTY	0x40
#define	INPUT_BUFFER_FULL	0x20
#define	MDONE	0x10

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

