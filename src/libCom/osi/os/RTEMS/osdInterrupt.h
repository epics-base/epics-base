/*
 * RTEMS osiInterrupt.h
 *	$Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */
int interruptLock (void);
void interruptUnlock (int key);
int interruptIsInterruptContext (void);
void interruptContextMessage (const char *message);
