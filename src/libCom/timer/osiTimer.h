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

/*
 * osiTimer
 */
class osiTimer : public tsDLNode <osiTimer> {
public:
    class noDelaySpecified {}; /* exception */
    class noMemory {}; /* exception */

    /*
     * Create an active timer that will expire delay seconds after it is created
     * or create an inactive timer respectively. 
     *
     * ** Warning ** default timer queue has no manager thread and is typically
     * processed by the file descriptor manager
     */
    epicsShareFunc osiTimer ( double delay, osiTimerQueue & queueIn );
    epicsShareFunc osiTimer ( double delay);
    epicsShareFunc osiTimer ( osiTimerQueue & queueIn );
    epicsShareFunc osiTimer ();

    /*
     * change the timers expiration to newDelay
     * seconds after when reschedule() is called
     */ 
    epicsShareFunc void reschedule (double newDelay);

    /*
     * change the timers expiration to this->delay()
     * seconds after when reschedule() is called
     */ 
    epicsShareFunc void reschedule ();

    /*
     * Start the timer with delay newDelay if inactive.
     */ 
    epicsShareFunc void activate ( double newDelay );

    /*
     * Start the timer with delay this->delay() if inactive.
     */ 
    epicsShareFunc void activate ();

    /*
     * inactivate the timer and call the virtual destroy()
     * member function
     */
    epicsShareFunc void cancel ();

    /*
     * return the number of seconds remaining before
     * this osiTimer will expire or the expiration date
     * respectively
     */
    epicsShareFunc double timeRemaining () const; /* returns seconds, but inefficent */
    epicsShareFunc osiTime expirationDate () const; /* efficent */

    /*
     * called when the osiTimer expires
     */
    epicsShareFunc virtual void expire () = 0;

    /*
     * called if 
     * 1) osiTimer exists and the osiTimerQueue is deleted
     * 2) when the osiTimer expires and again() returns false
     *
     * osiTimer::destroy() does a "delete this"
     *
     * if the derived class replaces this function then it
     * is taking responsibility for freeing (deleting)
     * timer resources when they are nolonger needed.
     */
    epicsShareFunc virtual void destroy ();

    /*
     * returning true indicates that the
     * osiTimer should be rearmed with delay
     * "delay()" when it expires
     *
     * the defaut osiTimer::again() returns false
     * (run the osiTimer once only)
     */
    epicsShareFunc virtual bool again () const;

    /*
     * returns the delay prior to expire
     * for subsequent iterations (if "again()"
     * returns true)
     *
     * the default osiTimer::delay() throws the 
     * exception type noDelaySpecified, but it will 
     * not be called unless the again() virtual 
     * function returns true.
     */
    epicsShareFunc virtual double delay () const;

    epicsShareFunc virtual void show (unsigned level) const;

    /*
     * for diagnostics
     */
    epicsShareFunc virtual const char *name () const;

    epicsShareFunc virtual ~osiTimer ();

private:
    friend class osiTimerQueue;
    enum state {statePending, stateExpired, stateIdle, 
        numberOfTimerLists, stateLimbo};

    osiTime exp; /* experation time */
    state curState; /* current state */
    osiTimerQueue &queue; /* pointer to current timer queue */

    /*
     * place osiTimer in the pending queue
     */
    void arm (double initialDelay);
};

/*
 * osiTimerQueue
 */
class osiTimerQueue {
friend class osiTimer;
friend class osiTimerThread;
public:
    enum managerThreadSelect { 
        mtsNoManagerThread, // you must call the process method (or the fdManager) to expire timers
        mtsCreateManagerThread // manager thread expires timers asnychronously
    };
    epicsShareFunc osiTimerQueue ( managerThreadSelect mts, 
        unsigned managerThreadPriority = threadPriorityMin );
    epicsShareFunc virtual ~osiTimerQueue();
    epicsShareFunc double delayToFirstExpire () const; /* returns seconds */
    epicsShareFunc void process ();
    epicsShareFunc void show (unsigned level) const;
private:
    osiMutex mutex;
    osiEvent rescheduleEvent;
    osiEvent exitEvent;
    tsDLList <osiTimer> timerLists [osiTimer::numberOfTimerLists];
    osiTimer *pExpireTmr;
    class osiTimerThread *pMgrThread;
    unsigned mgrThreadPriority;
    bool inProcess;
    bool terminateFlag;
    bool exitFlag;

    void install (osiTimer &tmr, double delay);
    void privateProcess ();
    void terminateManagerThread ();
};

/*
 * ** Warning ** 
 * the default timer queue has no manager thread and is typically
 * processed by the file descriptor manager
 */
epicsShareExtern osiTimerQueue osiDefaultTimerQueue;

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
epicsShareFunc osiTimerId epicsShareAPI osiTimerCreate (const osiTimerJumpTable *, osiTimerQueueId, void *pPrivate);
epicsShareFunc void epicsShareAPI osiTimerArm  (osiTimerId, double delay);
epicsShareFunc void epicsShareAPI osiTimerCancel (osiTimerId);
epicsShareFunc double epicsShareAPI osiTimerTimeRemaining (osiTimerId);
epicsShareFunc TS_STAMP epicsShareAPI osiTimerExpirationDate (osiTimerId);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* osiTimerHInclude */

