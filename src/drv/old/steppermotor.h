/* steppermotor.h */
/* base/src/drv $Id$ */
/*
 * header file to support database library interface to motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        12-11-89
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
 * .01	mm-dd-yy	iii	Comment
 */

/* readback data passed to the database library routine from the motor driver */
struct motor_data{
short	cw_limit;
short	ccw_limit;
short	moving;
short	direction;
short	constant_velocity;
long	velocity;
long	encoder_position;
long	motor_position;
long	accel;
};

/*
 * Sets values for the database library based on the value flag:
 * 0 - set the mode of the motor (position/velocity)
 * 1 - set the velocity of the motor
 * 2 - set the poistion of the motor
 * 3 - start motor rotating
 * 4 - set the callback routine for a motor
 */
#define	SM_MODE			0
#define	SM_VELOCITY		1
#define	SM_MOVE			2
#define	SM_MOTION		3
#define	SM_CALLBACK		4
#define	SM_SET_HOME		5
#define	SM_ENCODER_RATIO	6
#define	SM_MOTOR_RESOLUTION	7
#define	SM_READ			8
#define SM_FIND_LIMIT		9
#define SM_FIND_HOME		10
