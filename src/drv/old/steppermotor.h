/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* steppermotor.h */
/* base/src/drv $Id$ */
/*
 * header file to support database library interface to motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        12-11-89
 *
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
