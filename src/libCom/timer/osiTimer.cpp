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
 * Revision 1.8  1997/06/25 05:45:54  jhill
 * cleaned up pc port
 *
 * Revision 1.7  1997/04/10 19:45:34  jhill
 * API changes and include with  not <>
 *
 * Revision 1.6  1996/11/02 02:06:58  jhill
 * fixed several subtle problems
 *
 * Revision 1.5  1996/09/16 21:19:25  jhill
 * removed unused variable
 *
 * Revision 1.4  1996/08/05 21:51:11  jhill
 * fixed delete this confusion
 *
 * Revision 1.3  1996/07/24 23:01:53  jhill
 * use iter.remove()
 *
 * Revision 1.2  1996/07/09 23:00:06  jhill
 * force timer into limbo state during delete
 *
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

#define epicsExportSharedSymbols
#include "osiTimer.h"

osiTimerQueue staticTimerQueue;
static const tsDLIterBD<osiTimer> eol; // end of list

//
// osiTimer::arm()
//
epicsShareFunc void osiTimer::arm (const osiTime * const pInitialDelay)
{
	tsDLIterBD<osiTimer> iter;
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
	iter = staticTimerQueue.pending.last();
        while (1) {
		if (iter==eol) {
			//
			// add to the beginning of the list
			//
			staticTimerQueue.pending.push (*this);
			break;
		}
                if (iter->exp <= this->exp) {
			//
			// add after the item found that expires earlier
			//
			staticTimerQueue.pending.insertAfter (*this, *iter);
                        break;
                }
#		ifdef DEBUG
			preemptCount++;
#		endif
		--iter;
        }
	this->state = ositPending;
	
#	ifdef DEBUG
		staticTimerQueue.show(10u);
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
	if (this == staticTimerQueue.pExpireTmr) {
		staticTimerQueue.pExpireTmr = 0;
	}
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
epicsShareFunc osiBool osiTimer::again() const
{
	//
	// default is to run the timer only once 
	//
	return osiFalse;
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
epicsShareFunc const osiTime osiTimer::delay() const
{
	//
	// default to 1 sec
	//
	return osiTime (1.0);
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
			delay, this->state);
	}
}

//
// osiTimerQueue::delayToFirstExpire()
//
osiTime osiTimerQueue::delayToFirstExpire() const
{
	osiTimer *pTmr;
	osiTime delay;

	pTmr = this->pending.first();
	if (pTmr) {
		delay = pTmr->timeRemaining();
	}
	else {
		//
		// no timer in the queue - return a long delay - 30 min
		//
		delay = osiTime(30u * secPerMin, 0u);
	}
#ifdef DEBUG
	printf("delay to first item on the queue %f\n", (double) delay);
#endif
	return delay;
}

//
// osiTimerQueue::process()
//
void osiTimerQueue::process()
{
	tsDLIterBD<osiTimer> iter;
	tsDLIterBD<osiTimer> tmp;
	osiTime cur(osiTime::getCurrent());
	osiTimer *pTmr;

	// no recursion
	if (this->inProcess) {
		return;
	}
	this->inProcess = osiTrue;

	iter = this->pending.first();
	while ( iter!=eol ) {	
		if (iter->exp >= cur) {
			break;
		}
		tmp = iter;
		++tmp;
		this->pending.remove(*iter);
		iter->state = ositExpired;
		this->expired.add(*iter);
		iter = tmp;
	}

	//
	// I am careful to prevent problems if they access the
	// above list while in an "expire()" call back
	//
	while ( (pTmr = this->expired.get()) ) {

		pTmr->state = ositLimbo;

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
	this->inProcess = osiFalse;
}

//
// osiTimerQueue::show() const
//
void osiTimerQueue::show(unsigned level) const
{
	printf("osiTimerQueue with %u items pending and %u items expired\n",
		this->pending.count(), this->expired.count());
	tsDLIterBD<osiTimer> iter(this->pending.first());
	while ( iter!=eol ) {	
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
epicsShareFunc void osiTimer::reschedule(const osiTime &newDelay)
{
	//
	// signal the timer queue if this
	// occurrring during the expire call
	// back
	//
	if (this == staticTimerQueue.pExpireTmr) {
		staticTimerQueue.pExpireTmr = 0;
	}
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
	this->arm(&newDelay);
}

//
// osiTimer::timeRemaining()
//
// return the number of seconds remaining before
// this timer will expire
//
epicsShareFunc osiTime osiTimer::timeRemaining()
{
	osiTime cur = osiTime::getCurrent();
	osiTime delay;

	if (this->exp>cur) {
		delay = this->exp - cur;
	}
	else {
		delay = osiTime(0u,0u);
	}
	return delay;
}

