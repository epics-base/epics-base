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
 *
 */

//
// NOTES:
// 1)  when making this safe for multi threading consider the possibility of
// object delete just after finding an active fdRegI on
// the list but before callBack().
//
//

#include <stdio.h>

#include <osiTimer.h>

osiTimerQueue staticTimerQueue;

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
		pTmr->destroy();
	}

	//
	// destroy any expired timers
	//
	while ( (pTmr = this->expired.get()) ) {	
		pTmr->destroy();
	}
}


