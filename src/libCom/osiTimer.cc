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
 * Revision 1.1  1996/06/26 22:14:13  jhill
 * added new src files
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 */

//
// NOTES:
// 1) this should use a binary tree to speed up inserts?
// 2)  when making this safe for multi threading consider the possibility of
// object delete just after finding an active fdRegI on
// the list but before callBack().
//
//

#include <assert.h>
#include <stdio.h>

#include <osiTimer.h>

osiTimerQueue staticTimerQueue;

//
// osiTimer::arm()
//
void osiTimer::arm (const osiTime * const pInitialDelay)
{
	tsDLIter<osiTimer> iter (staticTimerQueue.pending);
	osiTimer *pTmr;

	//
	// calculate absolute expiration time
	// (dont call base's delay() virtual func
	// in the constructor)
	//
	if (pInitialDelay) {
		this->exp = osiTime::getCurrent() + *pInitialDelay;
	}
	else {
		this->exp = osiTime::getCurrent() + this->delay();
	}

#ifdef DEBUG
	double theDelay;
	osiTime copy;
	if (pInitialDelay) {
		theDelay = *pInitialDelay;
	}
	else {
		theDelay = this->delay();
	}
	printf ("Arm of \"%s\" with delay %lf at %x\n", 
		this->name(), theDelay, (unsigned)this);
#endif

	//
	// insert into the pending queue
	//
	// Finds proper time sorted location using
	// a linear search.
	// **** this should use a binary tree ????
	//
        while ( (pTmr = iter()) ) {
                if (pTmr->exp >= this->exp) {
                        break;
                }
        }
	if (pTmr) {
		pTmr = pTmr->getPrev ();
        	staticTimerQueue.pending.insert (*this, pTmr);
	}
	else {
        	staticTimerQueue.pending.add (*this);
	}
	this->state = ositPending;
	
#	ifdef DEBUG
		staticTimerQueue.show(10u);
#	endif
}

//
// osiTimer::~osiTimer()
//
osiTimer::~osiTimer()
{
	switch (this->state) {
	case ositPending:
		staticTimerQueue.pending.remove(*this);
		break;
	case ositExpired:
		staticTimerQueue.expired.remove(*this);
		break;
	case ositLimbo:
		break;
	default:
		assert(0);
	}
	this->state = ositLimbo;
}

//
// osiTimer::again()
//
osiBool osiTimer::again()
{
	//
	// default is to run the timer only once 
	//
	return osiFalse;
}

//
// osiTimer::destroy()
//
void osiTimer::destroy()
{
        delete this;
}

//
// osiTimer::delay()
//
const osiTime osiTimer::delay()
{
	//
	// default to 1 sec
	//
	return osiTime (1.0);
}

void osiTimer::show (unsigned level)
{
	osiTime	cur(osiTime::getCurrent());
	double delay;

	printf ("osiTimer at %x for \"%s\" with again = %d\n", 
		(unsigned) this, this->name(), this->again());
	if (this->exp >= cur) {
		delay = this->exp - cur;
	}
	else {
		delay = cur - this->exp;
		delay = -delay;
	}
	if (level>=1u) {
		printf ("\tdelay to expire = %f, state = %d\n", 
			delay, this->state);
	}
}

//
// osiTimerQueue::delayToFirstExpire()
//
osiTime osiTimerQueue::delayToFirstExpire()
{
	osiTimer *pTmr;
	osiTime cur(osiTime::getCurrent());
	osiTime delay;

	pTmr = pending.first();
	if (pTmr) {
		if (pTmr->exp>=cur) {
			delay = pTmr->exp - cur;
		}
		else {
			delay = osiTime(0u,0u);
		}
	}
	else {
		//
		// no timer in the queue - return a long delay - 30 min
		//
		delay = osiTime(30u * secPerMin, 0u);
	}
#ifdef DEBUG
	printf("delay to first item on the queue %lf\n", (double) delay);
#endif
	return delay;
}

//
// osiTimerQueue::process()
//
void osiTimerQueue::process()
{
	tsDLIter<osiTimer> iter (this->pending);
	osiTimer *pTmr;
	osiTimer *pNextTmr;
	osiTime cur(osiTime::getCurrent());

	// no recursion
	if (this->inProcess) {
		return;
	}
	this->inProcess = osiTrue;

	pNextTmr = iter();
	while ( (pTmr = pNextTmr) ) {	
		if (pTmr->exp >= cur) {
			break;
		}
		pNextTmr = iter();
		this->pending.remove(*pTmr);
		pTmr->state = ositExpired;
		this->expired.add(*pTmr);
	}

	//
	// prevent problems if they access the
	// above list while in an "expire()" call back
	//
	while ( (pTmr = this->expired.first()) ) {	
#ifdef DEBUG
		double diff = cur-pTmr->exp;
		printf ("expired %x for \"%s\" with error %lf\n", 
			pTmr, pTmr->name(), diff);
#endif
		pTmr->expire();
		//
		// verify that the current timer 
		// wasnt deleted in "expire()"
		//
		if (pTmr == this->expired.first()) {
			this->expired.get();
			pTmr->state = ositLimbo;
			if (pTmr->again()) {
				pTmr->arm();
			}
			else {
				pTmr->destroy();
			}
		}
	}
	this->inProcess = osiFalse;
}

//
// osiTimerQueue::show()
//
void osiTimerQueue::show(unsigned level)
{
	tsDLIter<osiTimer> iter (this->pending);
	osiTimer *pTmr;

	printf("osiTimerQueue with %d items pending and %d items expired\n",
		this->pending.count(), this->expired.count());
	while ( (pTmr = iter()) ) {	
		pTmr->show(level);
	}
}

//
// osiTimerQueue::~osiTimerQueue()
//
osiTimerQueue::~osiTimerQueue()
{
	osiTimer *pTmr;

	//
	// destroy any unexpired timers
	//
	while ( (pTmr = this->pending.get()) ) {	
		pTmr->state = ositLimbo;
		pTmr->destroy();
	}

	//
	// destroy any expired timers
	//
	while ( (pTmr = this->expired.get()) ) {	
		pTmr->state = ositLimbo;
		pTmr->destroy();
	}
}


