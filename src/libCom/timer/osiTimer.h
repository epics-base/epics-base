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
 */

#ifndef osiTimerHInclude
#define osiTimerHInclude

#include "shareLib.h" // reset share lib defines
#include "tsDLList.h"
#include "osiTime.h"

class osiTimerQueue;

epicsShareExtern osiTimerQueue osiDefaultTimerQueue;

//
// osiTimer
//
class osiTimer : public tsDLNode<osiTimer> {
	friend class osiTimerQueue;
public:
    //
    // exceptions
    //
    class noDelaySpecified {};

	epicsShareFunc osiTimer (double delay, osiTimerQueue & queueIn = osiDefaultTimerQueue) :
		queue (queueIn)
	{
		this->arm (&delay);
	}

	epicsShareFunc virtual ~osiTimer();

	//
	// called when the osiTimer expires
	//
	epicsShareFunc virtual void expire()=0;

	//
	// called if 
	// 1) osiTimer exists and the osiTimerQueue is deleted
	// 2) when the osiTimer expies and again() returs false
	//
	// osiTimer::destroy() does a "delete this"
	//
	epicsShareFunc virtual void destroy();

	//
	// returning true indicates that the
	// osiTimer should be rearmed with delay
	// "delay()" when it expires
    //
	// the defaut osiTimer::again() returns false
	// (run the osiTimer once only)
	//
	epicsShareFunc virtual bool again() const;

	//
	// returns the delay prior to expire
	// for subsequent iterations (if "again()"
	// returns true)
	//
	// the default osiTimer::delay() throws the 
    // exception type noDelaySpecified, but it will 
    // not be called unless the again() virtual 
    // function returns true.
	//
	epicsShareFunc virtual double delay() const;

	//
	// change the timers expiration to newDelay
	// seconds after when reschedule() is called
	//
	epicsShareFunc void reschedule (double newDelay);

	//
	// return the number of seconds remaining before
	// this osiTimer will expire
	//
	epicsShareFunc double timeRemaining();

	epicsShareFunc virtual void show (unsigned level) const;

	//
	// for diagnostics
	//
	epicsShareFunc virtual const char *name() const;

private:
	enum state {statePending, stateExpired, stateLimbo};

	osiTime exp; // experation time
	state curState; // current state
	osiTimerQueue &queue;

	//
	// arm()
	// place osiTimer in the pending queue
	//
	epicsShareFunc void arm (double *pInitialDelay=0);
};

//
// osiTimerQueue
//
class osiTimerQueue {
friend class osiTimer;
public:
	osiTimerQueue() : inProcess(false), pExpireTmr(0) {};
	~osiTimerQueue();
	double delayToFirstExpire () const; // returns seconds
	void process ();
	void show (unsigned level) const;
private:
	tsDLList<osiTimer> pending;	
	tsDLList<osiTimer> expired;	
	bool inProcess;
	osiTimer *pExpireTmr;

	void install (osiTimer &tmr, double delay);
};

#endif // osiTimerHInclude

