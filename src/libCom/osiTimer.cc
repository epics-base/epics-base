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

#define epicsExportSharedSymbols
#include "osiTimer.h"

osiTimerQueue osiDefaultTimerQueue;

//
// osiTimer::arm()
//
epicsShareFunc void osiTimer::arm (double *pInitialDelay)
{
#	ifdef DEBUG
	unsigned preemptCount=0u;
#	endif
	
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
	
	//
	// insert into the pending queue
	//
	// Finds proper time sorted location using
	// a linear search.
	//
	// **** this should use a binary tree ????
	//
	tsDLIterBD<osiTimer> iter = this->queue.pending.last();
	while (1) {
		if (iter==tsDLIterBD<osiTimer>::eol()) {
			//
			// add to the beginning of the list
			//
			this->queue.pending.push (*this);
			break;
		}
		if (iter->exp <= this->exp) {
			//
			// add after the item found that expires earlier
			//
			this->queue.pending.insertAfter (*this, *iter);
			break;
		}
#		ifdef DEBUG
		preemptCount++;
#		endif
		--iter;
	}
	this->curState = osiTimer::statePending;
	
#	ifdef DEBUG
	this->queue.show(10u);
#	endif
	
#	ifdef DEBUG 
	double theDelay;
	if (pInitialDelay) {
		theDelay = *pInitialDelay;
	}
	else {
		theDelay = this->delay();
	}
	//
	// name virtual function isnt always useful here because this is
	// often called inside the constructor (unless we are
	// rearming the same timer)
	//
	printf ("Arm of \"%s\" with delay %f at %lx preempting %u\n", 
		this->name(), theDelay, (unsigned long)this, preemptCount);
#	endif
	
}

//
// osiTimer::~osiTimer()
//
epicsShareFunc osiTimer::~osiTimer()
{
	//
	// signal the timer queue if this
	// was deleted during its expire call
	// back
	//
	if (this == this->queue.pExpireTmr) {
		this->queue.pExpireTmr = 0;
	}
	switch (this->curState) {
	case osiTimer::statePending:
		this->queue.pending.remove(*this);
		break;
	case osiTimer::stateExpired:
		this->queue.expired.remove(*this);
		break;
	case osiTimer::stateLimbo:
		break;
	default:
		assert(0);
	}
	this->curState = osiTimer::stateLimbo;
}

//
// osiTimer::again()
//
epicsShareFunc bool osiTimer::again() const
{
	//
	// default is to run the timer only once 
	//
	return false;
}

//
// osiTimer::destroy()
//
epicsShareFunc void osiTimer::destroy()
{
	delete this;
}

//
// osiTimer::delay()
//
epicsShareFunc double osiTimer::delay() const
{
    throw noDelaySpecified();
}

epicsShareFunc void osiTimer::show (unsigned level) const
{
	osiTime	cur(osiTime::getCurrent());
	double delay;

	printf ("osiTimer at %p for \"%s\" with again = %d\n", 
		this, this->name(), this->again());
	if (this->exp >= cur) {
		delay = this->exp - cur;
	}
	else {
		delay = cur - this->exp;
		delay = -delay;
	}
	if (level>=1u) {
		printf ("\tdelay to expire = %f, state = %d\n", 
			delay, this->curState);
	}
}

//
// osiTimerQueue::delayToFirstExpire()
//
double osiTimerQueue::delayToFirstExpire() const
{
	osiTimer *pTmr;
	double delay;

	pTmr = this->pending.first();
	if (pTmr) {
		delay = pTmr->timeRemaining();
	}
	else {
		//
		// no timer in the queue - return a long delay - 30 min
		//
		delay = 30u * osiTime::secPerMin;
	}
#ifdef DEBUG
	printf ("delay to first item on the queue %f\n", (double) delay);
#endif
	return delay; // seconds
}

//
// osiTimerQueue::process()
//
void osiTimerQueue::process()
{
	osiTime cur(osiTime::getCurrent());
	osiTimer *pTmr;
	
	// no recursion
	if (this->inProcess) {
		return;
	}
	this->inProcess = true;
	
	tsDLIterBD<osiTimer> iter = this->pending.first();
	while ( iter!=tsDLIterBD<osiTimer>::eol() ) {	
		if (iter->exp >= cur) {
			break;
		}
		tsDLIterBD<osiTimer> tmp = iter;
		++tmp;
		this->pending.remove(*iter);
		iter->curState = osiTimer::stateExpired;
		this->expired.add(*iter);
		iter = tmp;
	}
	
	//
	// I am careful to prevent problems if they access the
	// above list while in an "expire()" call back
	//
	while ( (pTmr = this->expired.get()) ) {
		
		pTmr->curState = osiTimer::stateLimbo;
		
#ifdef DEBUG
		double diff = cur-pTmr->exp;
		printf ("expired %lx for \"%s\" with error %f\n", 
			(unsigned long)pTmr, pTmr->name(), diff);
#endif
		
		//
		// Tag current tmr so that we
		// can detect if it was deleted
		// during the expire call back
		//
		this->pExpireTmr = pTmr;
		pTmr->expire();
		if (this->pExpireTmr == pTmr) {
			if (pTmr->again()) {
				pTmr->arm();
			}
			else {
				pTmr->destroy();
			}
		}
		else {
			//
			// no recursive calls  to process allowed
			//
			assert(this->pExpireTmr == 0);
		}
	}
	this->inProcess = false;
}

//
// osiTimerQueue::show() const
//
void osiTimerQueue::show(unsigned level) const
{
	printf("osiTimerQueue with %u items pending and %u items expired\n",
		this->pending.count(), this->expired.count());
	tsDLIterBD<osiTimer> iter(this->pending.first());
	while ( iter!=tsDLIterBD<osiTimer>::eol() ) {	
		iter->show(level);
		++iter;
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
		pTmr->curState = osiTimer::stateLimbo;
		pTmr->destroy();
	}

	//
	// destroy any expired timers
	//
	while ( (pTmr = this->expired.get()) ) {	
		pTmr->curState = osiTimer::stateLimbo;
		pTmr->destroy();
	}
}

//
// osiTimer::name()
// virtual base default 
//
epicsShareFunc const char *osiTimer::name() const
{
	return "osiTimer";
}

//
// osiTimer::reschedule()
// 
// pull this timer out of the queue ans reinstall
// it with a new experation time
//
epicsShareFunc void osiTimer::reschedule (double newDelay)
{
	//
	// signal the timer queue if this
	// occurrring during the expire call
	// back
	//
	if (this == this->queue.pExpireTmr) {
		this->queue.pExpireTmr = 0;
	}
	switch (this->curState) {
	case osiTimer::statePending:
		this->queue.pending.remove(*this);
		break;
	case osiTimer::stateExpired:
		this->queue.expired.remove(*this);
		break;
	case osiTimer::stateLimbo:
		break;
	default:
		assert(0);
	}
	this->curState = osiTimer::stateLimbo;
	this->arm (&newDelay);
}

//
// osiTimer::timeRemaining()
//
// return the number of seconds remaining before
// this timer will expire
//
epicsShareFunc double osiTimer::timeRemaining ()
{
	double remaining = this->exp - osiTime::getCurrent();
	if (remaining>0.0) {
		return remaining;
	}
	else {
		return 0.0;
	}
}