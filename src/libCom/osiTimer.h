/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 */


#ifndef osiTimerHInclude
#define osiTimerHInclude

#include "shareLib.h" // reset share lib defines
#include "tsDLList.h"
#include "osiTime.h"

enum osiBool {osiFalse=0, osiTrue=1};
enum osiTimerState {ositPending, ositExpired, ositLimbo};

//
// osiTimer
//
class osiTimer : public tsDLNode<osiTimer> {
	friend class osiTimerQueue;
public:
	epicsShareFunc osiTimer (const osiTime &delay)
	{
		this->arm(&delay);
	}
	epicsShareFunc virtual ~osiTimer();

	//
	// called when the timer expires
	//
	epicsShareFunc virtual void expire()=0;

	//
	// called if 
	// 1) osiTimer exists and the osiTimerQueue is deleted
	// 2) when the timer expies and again() returs false
	//
	// osiTimer::destroy() does a "delete this"
	//
	epicsShareFunc virtual void destroy();

	//
	// osiTimer::again() returns false
	// (run the timer once only)
	// returning true indicates that the
	// timer should be rearmed with delay
	// "delay()" when it expires
	//
	epicsShareFunc virtual osiBool again() const;

	//
	// returns the delay prior to expire
	// for subsequent iterations (if "again()"
	// returns true)
	//
	// osiTimer::delay() returns 1 sec
	//
	epicsShareFunc virtual const osiTime delay() const;

	//
	// change the timers expiration to newDelay
	// seconds after when reschedule() is called
	//
	epicsShareFunc void reschedule(const osiTime &newDelay);

	//
	// return the number of seconds remaining before
	// this timer will expire
	//
	epicsShareFunc osiTime timeRemaining();

	epicsShareFunc virtual void show (unsigned level) const;

	//
	// for diagnostics
	//
	epicsShareFunc virtual const char *name() const;
private:
	osiTime		exp;
	osiTimerState 	state;

	//
	// arm()
	// place timer in the pending queue
	//
	epicsShareFunc void arm (const osiTime * const pInitialDelay=0);
};



//
// osiTimerQueue
//
class osiTimerQueue {
friend class osiTimer;
public:
	osiTimerQueue() : inProcess(osiFalse), pExpireTmr(0) {};
	~osiTimerQueue();
	osiTime delayToFirstExpire () const;
	void process ();
	void show (unsigned level) const;
private:
	tsDLList<osiTimer>	pending;	
	tsDLList<osiTimer>	expired;	
	osiBool			inProcess;
	osiTimer		*pExpireTmr;

	void install (osiTimer &tmr, osiTime delay);
};

extern osiTimerQueue staticTimerQueue;


#endif // osiTimerHInclude

