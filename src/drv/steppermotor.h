
/* steppermotor.h */
/* share/src/drv $Id$ */

/*
 * steppermotor.h
 *
 * header file to support database library interface to motor drivers
 *
 * Author:      Bob Dalesio
 * Date:        12-11-89
 * @(#)ai_driver.h      1.1     9/22/88
 *
 *      Control System Software for the GTA Project
 *
 *      Copyright 1988, 1989, the Regents of the University of California.
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
