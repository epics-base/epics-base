#ifndef TIMERLIB_H
#define TIMERLIB_H

/*
 * Author: Jim Kowalkowski
 * Date: 4/97
 *
 * Based heavily on the HiDEOS ticktimer scheme - but using c instead of c++
 *
 * $Id$
 * $Log$
 */

typedef enum {
	timerClear=0,     /* clear the timer value */
	timerDisable=1,   /* disable the timer from counting */
	timerEnable=2,    /* enable counting */
	timerResetMode=3, /* reset counter to zero upon compare (periodic) */
	timerCycleMode=4, /* counter stays at compare value and stops counting */
	timerIntEnable=5, /* enable timer interrupt upon compare */
	timerIntDisable=6,/* disable timer interrupt */
	timerIntClear=7   /* clear the interrupt condition (done automatically) */
} timerCmds;

#define timerTotalCmds 8

typedef void (*INTFUNC)(void*);

/* must be run before the timerLib functions can be used */
int timerSystemInit(void);
void* timerAlloc(void); /* get a timer to use, returns handle to it */
void timerFree(void* handle); /* return a timer back to the timer pool */

/* tell the timer to do something - see above timerCmds */
int timerCmd(void* handle, timerCmds cmd);

/* information about the timer */
unsigned long timerResolution(void* handle);
unsigned long timerCurrentValue(void* handle); /* read the timer value */

/* set the number of microsecond before timer goes off */
int timerCompareValue(void* handle,unsigned long usec);
/* the timer's interrupt level */
int timerIntLevel(void* handle,int level);

/* provide function called when the timer goes off (at interrupt level) */
int timerIntFunction(void* handle,INTFUNC func, void* parm);

/* simple way to set up a periodic interrupt every usec microseconds at
   interrupt level lev. The function func will be called with parameter
   parm */
int timerPeriodic(void* h,unsigned long usec,int lev,INTFUNC func,void* parm);

#endif
