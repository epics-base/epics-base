/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 * Revision 1.3  1996/11/02 02:06:59  jhill
 * fixed several subtle problems
 *
 * Revision 1.2  1996/08/05 21:51:11  jhill
 * fixed delete this confusion
 *
 * Revision 1.1  1996/06/26 22:14:15  jhill
 * added new src files
 *
 * Revision 1.1.1.1  1996/06/20 22:15:55  jhill
 * installed  ca server templates
 *
 *
 */


#ifndef osiTimerHInclude
#define osiTimerHInclude

#include "tsDLList.h"
#include "osiTime.h"

enum osiBool {osiFalse=0, osiTrue=1};
enum osiTimerState {ositPending, ositExpired, ositLimbo};

//
// osiTimer
//
class epicsShareClass osiTimer : public tsDLNode<osiTimer> {
	friend class osiTimerQueue;
public:
	osiTimer (const osiTime &delay)
	{
		this->arm(&delay);
	}
	virtual ~osiTimer();

	//
	// called when the timer expires
	//
	virtual void expire()=0;

	//
	// called if 
	// 1) osiTimer exists and the osiTimerQueue is deleted
	// 2) when the timer expies and again() returs false
	//
	// osiTimer::destroy() does a "delete this"
	//
	virtual void destroy();

	//
	// osiTimer::again() returns false
	// (run the timer once only)
	// returning true indicates that the
	// timer should be rearmed with delay
	// "delay()" when it expires
	//
	virtual osiBool again() const;

	//
	// returns the delay prior to expire
	// for subsequent iterations (if "again()"
	// returns true)
	//
	// osiTimer::delay() returns 1 sec
	//
	virtual const osiTime delay() const;

	//
	// change the timers expiration to newDelay
	// seconds after when reschedule() is called
	//
	void reschedule(const osiTime &newDelay);

	//
	// return the number of seconds remaining before
	// this timer will expire
	//
	osiTime timeRemaining();

	virtual void show (unsigned level) const;

	//
	// for diagnostics
	//
	virtual const char *name() const;
private:
	osiTime		exp;
	osiTimerState 	state;

	//
	// arm()
	// place timer in the pending queue
	//
	void arm (const osiTime * const pInitialDelay=0);
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

