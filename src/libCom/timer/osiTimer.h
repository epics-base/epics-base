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

#include "shareLib.h" /* reset share lib defines */
#include "osiThread.h"
#include "tsStamp.h"

#ifdef __cplusplus

#include "osiTime.h"
#include "tsDLList.h"
#include "osiMutex.h"
#include "osiEvent.h"

class osiTimerQueue;
class managerThread;

epicsShareExtern osiTimerQueue osiDefaultTimerQueue;

//
// osiTimer
//
class osiTimer : public tsDLNode<osiTimer> {
friend class osiTimerQueue;
public:
    class noDelaySpecified {}; // exception

    //
    // create an active timer that will expire delay secods after it is created
    // or create an inactive timer respectively
    //
	epicsShareFunc osiTimer (double delay, osiTimerQueue & queueIn = osiDefaultTimerQueue);
	epicsShareFunc osiTimer ();

	//
	// change the timer's expiration to newDelay
	// seconds after when reschedule() is called
	//	
    epicsShareFunc void reschedule (double newDelay, osiTimerQueue & queueIn = osiDefaultTimerQueue);

	//
	// change the timers expiration to this->delay()
	// seconds after when reschedule() is called
	//	
	epicsShareFunc void reschedule (osiTimerQueue & queueIn = osiDefaultTimerQueue);

    //
    // inactivate the timer and call the virtual destroy()
    // member function
    //
	epicsShareFunc void cancel ();

	//
	// return the number of seconds remaining before
	// this osiTimer will expire or the expiration date
    // respectively
	//
	epicsShareFunc double timeRemaining () const; // returns seconds, but inefficent
    epicsShareFunc osiTime expirationDate () const; // efficent

	//
	// called when the osiTimer expires
	//
	epicsShareFunc virtual void expire () = 0;

	//
	// called if 
	// 1) osiTimer exists and the osiTimerQueue is deleted
	// 2) when the osiTimer expires and again() returns false
	//
	// osiTimer::destroy() does a "delete this"
    //
    // if the derived class replaces this function then it
    // is taking responsibility for freeing (deleting)
    // timer resources when they are nolonger needed.
	//
	epicsShareFunc virtual void destroy ();

	//
	// returning true indicates that the
	// osiTimer should be rearmed with delay
	// "delay()" when it expires
    //
	// the defaut osiTimer::again() returns false
	// (run the osiTimer once only)
	//
	epicsShareFunc virtual bool again () const;

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
	epicsShareFunc virtual double delay () const;

	epicsShareFunc virtual void show (unsigned level) const;

	//
	// for diagnostics
	//
	epicsShareFunc virtual const char *name () const;

	epicsShareFunc virtual ~osiTimer ();

private:

	enum state {statePending, stateExpired, stateLimbo};

	osiTime exp; // experation time
	state curState; // current state
	osiTimerQueue *pQueue; // pointer to current timer queue

	//
	// place osiTimer in the pending queue
	//
	void arm (osiTimerQueue &queueIn, double initialDelay);

    //
    // detach from any timer queues
    //
    void cleanup ();

    static osiMutex mutex;

    virtual void lock () const;
    virtual void unlock () const;
};

//
// osiTimerQueue
//
class osiTimerQueue : public osiThread {
friend class osiTimer;
public:
	osiTimerQueue (unsigned managerThreadPriority = threadPriorityMin);
	virtual ~osiTimerQueue();
	double delayToFirstExpire () const; // returns seconds
	void process ();
	void show (unsigned level) const;
private:
    osiMutex mutex;
    osiEvent rescheduleEvent;
    osiEvent exitEvent;
	tsDLList <osiTimer> pending;	
	tsDLList <osiTimer> expired;
	osiTimer *pExpireTmr;
	bool inProcess;
    bool terminateFlag;
    bool exitFlag;

    virtual void entryPoint ();
	void install (osiTimer &tmr, double delay);
};

inline osiTime osiTimer::expirationDate () const
{
    return this->exp;
}

#endif /* ifdef __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * osiTimerJumpTable
 */
typedef struct osiTimerJumpTable {
    void (*expire) (void *pPrivate); /* called when timer expires */
    void (*destroy) (void *pPrivate); /* called after expire() when again() is false */
    int (*again) (void *pPrivate); /* rearm after expire when again() is true */
    double (*delay) (void *pPrivate); /* rearm delay */
    void (*show) (void *pPrivate, unsigned level); /* diagnostic */
}osiTimerJumpTable;

typedef void * osiTimerQueueId;
/* see osiThread.h for the range of priorities allowed here */
epicsShareFunc osiTimerQueueId epicsShareAPI osiTimerQueueCreate (unsigned managerThreadPriority);

typedef void * osiTimerId;
epicsShareFunc osiTimerId epicsShareAPI osiTimerCreate (const osiTimerJumpTable *, void *pPrivate);
epicsShareFunc void epicsShareAPI osiTimerArm  (osiTimerId, osiTimerQueueId, double delay);
epicsShareFunc void epicsShareAPI osiTimerCancel (osiTimerId);
epicsShareFunc double epicsShareAPI osiTimerTimeRemaining (osiTimerId);
epicsShareFunc TS_STAMP epicsShareAPI osiTimerExpirationDate (osiTimerId);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // osiTimerHInclude

