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
 *
 * NOTES:
 * 1) this should use a binary tree to speed up inserts?
 */

#include <assert.h>
#include <stdio.h>

#include <osiTimer.h>

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

