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
#include <float.h>

#define epicsExportSharedSymbols
#include "osiTimer.h"

//
// global lock used when moving a timer between timer queues
//
osiMutex osiTimer::mutex;

//
// default global timer queue
//
osiTimerQueue osiDefaultTimerQueue;

//
// osiTimer::osiTimer ()
//
// create an active timer that will expire in delay seconds
//
osiTimer::osiTimer (double delay, osiTimerQueue & queueIn)
{
	this->arm (queueIn, delay);
}

//
// osiTimer::osiTimer ()
//
// create an inactive timer
//
osiTimer::osiTimer () :
curState (osiTimer::stateLimbo), pQueue (0) 
{
}

//
// osiTimer::~osiTimer()
// NOTE: The osiTimer lock is not applied for cleanup here because we are 
// synchronizing properly with the queue, and the creator of this object 
// should have removed all knowledge of this object from other threads 
// before deleting it.
//
osiTimer::~osiTimer()
{
    this->cleanup ();
}

//
// osiTimer::cancel ()
//
void osiTimer::cancel ()
{
    this->lock ();
    this->cleanup ();
    this->unlock ();
    this->destroy ();
}

//
// osiTimer::reschedule()
// 
// pull this timer out of the queue and reinstall
// it with a new experation time
//
void osiTimer::reschedule (osiTimerQueue & queueIn)
{
    this->reschedule (this->delay(), queueIn);
}

//
// osiTimer::reschedule()
// 
// pull this timer out of the queue ans reinstall
// it with a new experation time
//
void osiTimer::reschedule (double newDelay, osiTimerQueue & queueIn)
{
    this->lock ();
    this->cleanup ();
	this->arm (queueIn, newDelay);
    this->unlock ();
}

//
// osiTimer::cleanup ()
// NOTE: osiTimer lock is applied externally because we must guarantee 
// that the lock virtual function is not called by the destructor
//
void osiTimer::cleanup ()
{
    if (this->pQueue) {
        this->pQueue->mutex.lock ();
	    //
	    // signal the timer queue if this
	    // occurring during the expire call
	    // back
	    //	    
        if (this == this->pQueue->pExpireTmr) {
		    this->pQueue->pExpireTmr = 0;
	    }
	    switch (this->curState) {
	    case statePending:
		    this->pQueue->pending.remove (*this);
		    break;
	    case stateExpired:
		    this->pQueue->expired.remove (*this);
		    break;
	    default:
            break;
	    }
        this->pQueue = NULL;
        this->curState = stateLimbo;

        this->pQueue->mutex.unlock ();
    }
}

//
// osiTimer::arm()
// NOTE: The osiTimer lock is properly applied externally to this routine
// when it is needed.
//
void osiTimer::arm (osiTimerQueue &queueIn, double initialDelay)
{
#	ifdef DEBUG
	unsigned preemptCount=0u;
#	endif
	
    queueIn.mutex.lock ();

	//
	// calculate absolute expiration time
	//
	this->exp = osiTime::getCurrent () + initialDelay;

	//
	// insert into the pending queue
	//
	// Finds proper time sorted location using
	// a linear search.
	//
	// **** this should use a binary tree ????
	//
	tsDLIterBD<osiTimer> iter = queueIn.pending.last ();
	while (1) {
		if ( iter == tsDLIterBD<osiTimer>::eol () ) {
			//
			// add to the beginning of the list
			//
			queueIn.pending.push (*this);
			break;
		}
		if ( iter->exp <= this->exp ) {
			//
			// add after the item found that expires earlier
			//
			queueIn.pending.insertAfter (*this, *iter);
			break;
		}
#		ifdef DEBUG
		preemptCount++;
#		endif
		--iter;
	}

	this->curState = osiTimer::statePending;
	this->pQueue = &queueIn;

#	ifdef DEBUG
	this->show (10u);
#	endif
	
#	ifdef DEBUG 
	//
	// name virtual function isnt always useful here because this is
	// often called inside the constructor (unless we are
	// rearming the same timer)
	//
	printf ("Arm of \"%s\" with delay %f at %lx preempting %u\n", 
		this->name(), initialDelay, (unsigned long)this, preemptCount);
#	endif

    queueIn.mutex.unlock ();

    queueIn.rescheduleEvent.signal ();
}

//
// osiTimer::again()
//
bool osiTimer::again () const
{
	//
	// default is to run the timer only once 
	//
	return false;
}

//
// osiTimer::destroy()
//
void osiTimer::destroy ()
{
	delete this;
}

//
// osiTimer::delay()
//
double osiTimer::delay () const
{
#   ifdef noExceptionsFromCXX
        assert (0);
#   else            
        throw noDelaySpecified ();
#   endif
    return DBL_MAX;
}

void osiTimer::show (unsigned level) const
{
	osiTime	cur = osiTime::getCurrent ();
	printf ("osiTimer at %p for \"%s\" with again = %d\n", 
		this, this->name(), this->again());
	if (level>=1u) {
	    double delay = this->exp - cur;
		printf ("\tdelay to expire = %f, state = %d\n", 
			delay, this->curState);
	}
}

//
// osiTimer::name()
// virtual base default 
//
const char *osiTimer::name() const
{
	return "osiTimer";
}

//
// osiTimer::timeRemaining()
//
// return the number of seconds remaining before
// this timer will expire
//
double osiTimer::timeRemaining () const
{
	double remaining = this->exp - osiTime::getCurrent();
	if (remaining>0.0) {
		return remaining;
	}
	else {
		return 0.0;
	}
}

//
// osiTimer::lock ()
// (defaults to one global lock for all timers)
//
void osiTimer::lock () const
{
    this->mutex.lock ();
}

//
// osiTimer::unlock ()
// (defaults to one global lock for all timers)
//
void osiTimer::unlock () const
{
    this->mutex.unlock ();
}

//
// osiTimerQueue::osiTimerQueue ()
//
osiTimerQueue::osiTimerQueue (unsigned managerThreadPriority) :
    osiThread ("osiTimerQueue", threadGetStackSize (threadStackMedium), managerThreadPriority),
    pExpireTmr (0), inProcess (false), terminateFlag (false), exitFlag (false)
{
}

//
// osiTimerQueue::~osiTimerQueue()
//
osiTimerQueue::~osiTimerQueue()
{
	osiTimer *pTmr;

    this->terminateFlag = true;
    this->rescheduleEvent.signal ();
    this->exitEvent.wait (0.1);
    if ( ! this->exitFlag && ! this->isSuspended () ) {
        static const unsigned maxCount = 25;
        static const double delay = 0.25;
        unsigned count = 0;
        printf ("waiting %f seconds for timer queue to shut down", 
            delay * maxCount);
        while ( ! this->exitFlag && ! this->isSuspended () && count < 10) {
            this->exitEvent.wait (delay);
            printf (".");
            count++;
        }
        printf ("\n");
    }

    this->mutex.lock ();

	//
	// destroy any unexpired timers
	//
	while ( ( pTmr = this->pending.get () ) ) {	
		pTmr->curState = osiTimer::stateLimbo;
		pTmr->destroy ();
	}

	//
	// destroy any expired timers
	//
	while ( (pTmr = this->expired.get()) ) {	
		pTmr->curState = osiTimer::stateLimbo;
		pTmr->destroy ();
	}
}

//
// osiTimerQueue::entryPoint ()
//
void osiTimerQueue::entryPoint ()
{
    this->exitFlag = false;
    while (!this->terminateFlag) {
        this->process ();
        this->rescheduleEvent.wait ( this->delayToFirstExpire () );
    }
    this->exitFlag = true; 
    this->exitEvent.signal (); // no access to this ptr after this statement
}

//
// osiTimerQueue::delayToFirstExpire ()
//
double osiTimerQueue::delayToFirstExpire () const
{
	osiTimer *pTmr;
	double delay;

    this->mutex.lock ();

	pTmr = this->pending.first ();
	if (pTmr) {
		delay = pTmr->timeRemaining ();
	}
	else {
		//
		// no timer in the queue - return a long delay - 30 min
		//
		delay = 30u * osiTime::secPerMin;
	}

    this->mutex.unlock ();

#ifdef DEBUG
	printf ("delay to first item on the queue %f\n", (double) delay);
#endif
	return delay; // seconds
}

//
// osiTimerQueue::process()
//
void osiTimerQueue::process ()
{
	osiTime cur (osiTime::getCurrent());
	osiTimer *pTmr;
	
    this->mutex.lock ();

	// no recursion
	if (this->inProcess) {
        this->mutex.unlock ();
		return;
	}
	this->inProcess = true;
	

	tsDLIterBD<osiTimer> iter = this->pending.first ();
	while ( iter != tsDLIterBD<osiTimer>::eol () ) {	
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
	while ( ( pTmr = this->expired.get () ) ) {
		
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
		if ( this->pExpireTmr == pTmr ) {
			if ( pTmr->again () ) {
				pTmr->arm (*pTmr->pQueue, pTmr->delay());
			}
			else {
                pTmr->pQueue = NULL;
				pTmr->destroy ();
			}
		}
		else {
			//
			// no recursive calls  to process allowed
			//
			assert (this->pExpireTmr == 0);
		}
	}
	this->inProcess = false;

    this->mutex.unlock();
}

//
// osiTimerQueue::show() const
//
void osiTimerQueue::show(unsigned level) const
{
    this->mutex.lock();
	printf("osiTimerQueue with %u items pending and %u items expired\n",
		this->pending.count(), this->expired.count());
	tsDLIterBD<osiTimer> iter(this->pending.first());
	while ( iter!=tsDLIterBD<osiTimer>::eol() ) {	
		iter->show(level);
		++iter;
	}
    this->mutex.unlock();
}

extern "C" epicsShareFunc osiTimerQueueId epicsShareAPI osiTimerQueueCreate (unsigned managerThreadPriority)
{
    return (osiTimerQueueId) new osiTimerQueue (managerThreadPriority);
}

class osiTimerForC : public osiTimer {
	epicsShareFunc virtual void expire();
	epicsShareFunc virtual void destroy();
	epicsShareFunc virtual bool again() const;
	epicsShareFunc virtual double delay() const;
	epicsShareFunc virtual void show (unsigned level) const;
    const osiTimerJumpTable &jt;
    void * pPrivate;
public:
    osiTimerForC (const osiTimerJumpTable &jtIn, void *pPrivateIn);
};

osiTimerForC::osiTimerForC (const osiTimerJumpTable &jtIn, void *pPrivateIn) :
    jt (jtIn), pPrivate (pPrivateIn) {}

void osiTimerForC::expire ()
{
    (*this->jt.expire) (this->pPrivate);
}

void osiTimerForC::destroy ()
{
    (*this->jt.destroy) (this->pPrivate);
}

bool osiTimerForC::again () const
{
    return (*this->jt.again) (this->pPrivate) ? true : false ;
}

double osiTimerForC::delay () const 
{
    return (*this->jt.delay) (this->pPrivate);
}

void osiTimerForC::show (unsigned level) const 
{
    (*this->jt.show) (this->pPrivate, level);
}

extern "C" epicsShareFunc osiTimerId epicsShareAPI osiTimerCreate (const osiTimerJumpTable *pjtIn, void *pPrivateIn)
{
    assert (pjtIn);
    return (osiTimerId) new osiTimerForC (*pjtIn, pPrivateIn);
}

extern "C" epicsShareFunc void epicsShareAPI osiTimerArm  (osiTimerId tmrIdIn, osiTimerQueueId queueIdIn, double delay)
{
    osiTimerForC *pTmr = static_cast<osiTimerForC *>(tmrIdIn);
    assert (pTmr);
    assert (queueIdIn);
    pTmr->reschedule (delay, *static_cast<osiTimerQueue *>(queueIdIn));
}

extern "C" epicsShareFunc void epicsShareAPI osiTimerCancel (osiTimerId tmrIdIn)
{
    osiTimerForC *pTmr = static_cast<osiTimerForC *>(tmrIdIn);
    assert (pTmr);
    pTmr->cancel ();
}

extern "C" epicsShareFunc double epicsShareAPI osiTimerTimeRemaining (osiTimerId idIn)
{
    osiTimerForC *pTmr = static_cast<osiTimerForC *> (idIn);
    assert (pTmr);
    return pTmr->timeRemaining ();
}

extern "C" epicsShareFunc TS_STAMP epicsShareAPI osiTimerExpirationDate (osiTimerId idIn)
{
    osiTimerForC *pTmr = static_cast<osiTimerForC *> (idIn);
    assert (pTmr);
    return pTmr->expirationDate ();
}
