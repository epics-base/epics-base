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
#include "locationException.h"
#include "errlog.h"

class osiTimerThread : public osiThread {
public:
    osiTimerThread (osiTimerQueue &, unsigned priority);
private:
    osiTimerQueue &queue;

    virtual void entryPoint ();
};

//
// default global timer queue
//
osiTimerQueue osiDefaultTimerQueue;

//
// osiTimer::osiTimer ()
//
// create an active timer that will expire in delay seconds
//
osiTimer::osiTimer (double delay, osiTimerQueue & queueIn) :
    queue (queueIn)
{
	this->arm (delay);
}

//
// osiTimer::osiTimer ()
//
// create an active timer that will expire in delay seconds
//
osiTimer::osiTimer (double delay) :
    queue (osiDefaultTimerQueue)
{
	this->arm (delay);
}

//
// osiTimer::osiTimer ()
//
// create an inactive timer
//
osiTimer::osiTimer (osiTimerQueue & queueIn) :
curState (osiTimer::stateIdle), queue (queueIn) 
{
    this->queue.mutex.lock ();
	this->queue.idle.add (*this);
    this->queue.mutex.unlock ();
}

//
// osiTimer::osiTimer ()
//
// create an inactive timer
//
osiTimer::osiTimer () :
curState (osiTimer::stateIdle), queue (osiDefaultTimerQueue) 
{
    this->queue.mutex.lock ();
	this->queue.idle.add (*this);
    this->queue.mutex.unlock ();
}

//
// osiTimer::~osiTimer()
//
osiTimer::~osiTimer()
{
    if ( this->curState == stateLimbo ) {
        return; // queue was destroyed
    }

    this->queue.mutex.lock ();
	//
	// signal the timer queue if this
	// occurring during the expire call
	// back
	//	    
    if (this == this->queue.pExpireTmr) {
		this->queue.pExpireTmr = 0;
	}
	switch (this->curState) {
	case statePending:
		this->queue.pending.remove (*this);
		break;
	case stateExpired:
		this->queue.expired.remove (*this);
		break;
	case stateIdle:
		this->queue.idle.remove (*this);
		break;
	default:
        errlogPrintf ("observed osiTimer is in undefined state?\n");
        break;
	}
    this->curState = stateLimbo;
    this->queue.mutex.unlock ();
}

//
// osiTimer::cancel ()
//
void osiTimer::cancel ()
{
    if ( this->curState == stateLimbo ) {
        return; // queue was destroyed
    }

    this->queue.mutex.lock ();

	//
	// signal the timer queue if this
	// occurring during the expire call
	// back
	//	    
    if (this == this->queue.pExpireTmr) {
		this->queue.pExpireTmr = 0;
	}
	switch (this->curState) {
	case statePending:
		this->queue.pending.remove (*this);
        this->queue.idle.add (*this);
		break;
	case stateExpired:
		this->queue.expired.remove (*this);
        this->queue.idle.add (*this);
		break;
	case stateIdle:
		break;
	default:
        errlogPrintf ("observed osiTimer is in undefined state?\n");
        break;
	}

    this->queue.mutex.unlock ();

    this->destroy ();
}

//
// osiTimer::reschedule()
// 
// pull this timer out of the queue and reinstall
// it with a new experation time
//
void osiTimer::reschedule ()
{
    this->reschedule ( this->delay () );
}

//
// osiTimer::reschedule()
// 
// pull this timer out of the queue ans reinstall
// it with a new experation time
//
void osiTimer::reschedule (double newDelay)
{
    if ( this->curState == stateLimbo ) {
        return; // queue was destroyed
    }

    this->queue.mutex.lock ();

	//
	// signal the timer queue if this
	// occurring during the expire call
	// back
	//	    
    if (this == this->queue.pExpireTmr) {
		this->queue.pExpireTmr = 0;
	}
	switch (this->curState) {
	case statePending:
		this->queue.pending.remove (*this);
		break;
	case stateExpired:
		this->queue.expired.remove (*this);
		break;
	case stateIdle:
		this->queue.idle.remove (*this);
		break;
	default:
        errlogPrintf ("observed osiTimer is in undefined state?\n");
        break;
	}
    this->curState = stateLimbo;
    this->arm (newDelay);
    this->queue.mutex.unlock ();
}

//
// osiTimer::arm()
// NOTE: The osiTimer lock is properly applied externally to this routine
// when it is needed.
//
void osiTimer::arm (double initialDelay)
{
#	ifdef DEBUG
	unsigned preemptCount=0u;
#	endif

    this->queue.mutex.lock ();

    //
    // create manager thread on demand so we dont have threads hanging
    // around that are not used
    //
    if ( this->queue.pMgrThread == NULL ) {
        this->queue.pMgrThread = new osiTimerThread (this->queue, this->queue.mgrThreadPriority);
        if ( this->queue.pMgrThread == NULL ) {
            this->queue.mutex.unlock ();
            throwWithLocation ( noMemory () );
        }
    }

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
	tsDLIterBD<osiTimer> iter = this->queue.pending.last ();
	while (1) {
		if ( iter == tsDLIterBD<osiTimer>::eol () ) {
			//
			// add to the beginning of the list
			//
			this->queue.pending.push (*this);
			break;
		}
		if ( iter->exp <= this->exp ) {
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

    this->queue.mutex.unlock ();

    this->queue.rescheduleEvent.signal ();
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
    throwWithLocation ( noDelaySpecified () );
    return DBL_MAX; // never here
}

//
// osiTimer::show()
//
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
    double delay;

    if ( this->curState == stateLimbo ) {
        errlogPrintf ("time remaning fetched on a osiTimer with no queue?\n");
        return DBL_MAX; // queue was destroyed
    }

    this->queue.mutex.lock ();
    switch (this->curState) {
    case statePending:
        {
	        double remaining = this->exp - osiTime::getCurrent();
	        if ( remaining > 0.0 ) {
		        delay = remaining;
	        }
	        else {
		        delay = 0.0;
	        }
            break;
        }
    case stateIdle:
        delay = DBL_MAX;
        break;
    case stateExpired:
        delay = 0.0;
        break;
    default:
        errlogPrintf ("saw osiTimer in undefined state\n");
        delay = DBL_MAX;
        break;
    }
    this->queue.mutex.unlock ();

    return delay;
}

//
// osiTimerQueue::osiTimerQueue ()
//
osiTimerQueue::osiTimerQueue (unsigned managerThreadPriority) :
    pExpireTmr (0), pMgrThread(0), mgrThreadPriority(managerThreadPriority), 
    inProcess (false), terminateFlag (false), exitFlag (false)
{
}

//
// osiTimerQueue::~osiTimerQueue()
//
osiTimerQueue::~osiTimerQueue()
{
	osiTimer *pTmr;

    if (this->pMgrThread) {
        this->terminateFlag = true;
        this->rescheduleEvent.signal ();
        this->exitEvent.wait (0.1);
        if ( ! this->exitFlag && ! this->pMgrThread->isSuspended () ) {
            static const unsigned maxCount = 25;
            static const double delay = 0.25;
            unsigned count = 0;
            printf ("waiting %f seconds for timer queue to shut down", 
                delay * maxCount);
            while ( ! this->exitFlag && ! this->pMgrThread->isSuspended () && count < 10) {
                this->exitEvent.wait (delay);
                printf (".");
                count++;
            }
            printf ("\n");
        }
        delete this->pMgrThread;
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
// osiTimerThread::osiTimerThread ()
//
osiTimerThread::osiTimerThread (osiTimerQueue &queueIn, unsigned priority) :
    osiThread ("osiTimerQueue", threadGetStackSize (threadStackMedium), priority),
        queue (queueIn)
{
}

//
// osiTimerThread::entryPoint ()
//
void osiTimerThread::entryPoint ()
{
    queue.exitFlag = false;
    while (!queue.terminateFlag) {
        queue.process ();
        queue.rescheduleEvent.wait ( queue.delayToFirstExpire () );
    }
    queue.exitFlag = true; 
    queue.exitEvent.signal (); // no access to queue after exitEvent signal
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
		
		pTmr->curState = osiTimer::stateIdle;
		this->idle.add (*pTmr);

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
		        this->idle.remove (*pTmr);
				pTmr->arm (pTmr->delay());
			}
			else {
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
    osiTimerForC (const osiTimerJumpTable &jtIn, osiTimerQueue &queue, void *pPrivateIn);
};

osiTimerForC::osiTimerForC (const osiTimerJumpTable &jtIn, osiTimerQueue &queue, void *pPrivateIn) :
    osiTimer (queue), jt (jtIn), pPrivate (pPrivateIn) {}

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

extern "C" epicsShareFunc osiTimerId epicsShareAPI osiTimerCreate (const osiTimerJumpTable *pjtIn, osiTimerQueueId queueIdIn, void *pPrivateIn)
{
    assert (pjtIn);
    assert (queueIdIn);
    return (osiTimerId) new osiTimerForC (*pjtIn, *static_cast<osiTimerQueue *>(queueIdIn), pPrivateIn);
}

extern "C" epicsShareFunc void epicsShareAPI osiTimerArm  (osiTimerId tmrIdIn, double delay)
{
    osiTimerForC *pTmr = static_cast<osiTimerForC *>(tmrIdIn);
    assert (pTmr);
    pTmr->reschedule (delay);
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
