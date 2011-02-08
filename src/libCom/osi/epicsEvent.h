/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef epicsEventh
#define epicsEventh

#include "shareLib.h"

typedef struct epicsEventOSD *epicsEventId;

typedef enum {
    epicsEventOK = 0,
    epicsEventWaitTimeout,
    epicsEventError
} epicsEventStatus;

/* Backwards compatibility */
#define epicsEventWaitStatus epicsEventStatus
#define epicsEventWaitOK epicsEventOK
#define epicsEventWaitError epicsEventError

typedef enum {
    epicsEventEmpty,
    epicsEventFull
} epicsEventInitialState;

#ifdef __cplusplus

class epicsShareClass epicsEvent {
public:
    epicsEvent ( epicsEventInitialState initial = epicsEventEmpty );
    ~epicsEvent ();
    void trigger ();
    void signal () { this->trigger(); }
    void wait ();                   /* blocks until full */
    bool wait ( double timeOut );   /* false if still empty at time out */
    bool tryWait ();                /* false if empty */
    void show ( unsigned level ) const;

    class invalidSemaphore;         /* exception payload */
private:
    epicsEvent ( const epicsEvent & );
    epicsEvent & operator = ( const epicsEvent & );
    epicsEventId id;
};

extern "C" {
#endif /*__cplusplus */

epicsShareFunc epicsEventId epicsShareAPI epicsEventCreate(
    epicsEventInitialState initialState);
epicsShareFunc epicsEventId epicsShareAPI epicsEventMustCreate (
    epicsEventInitialState initialState);
epicsShareFunc void epicsShareAPI epicsEventDestroy(epicsEventId id);
epicsShareFunc epicsEventStatus epicsShareAPI epicsEventTrigger(
    epicsEventId id);
epicsShareFunc void epicsShareAPI epicsEventMustTrigger(epicsEventId id);
#define epicsEventSignal(ID) epicsEventMustTrigger(ID)
epicsShareFunc epicsEventStatus epicsShareAPI epicsEventWait(
    epicsEventId id);
epicsShareFunc void epicsShareAPI epicsEventMustWait(epicsEventId id);
epicsShareFunc epicsEventStatus epicsShareAPI epicsEventWaitWithTimeout(
    epicsEventId id, double timeOut);
epicsShareFunc epicsEventStatus epicsShareAPI epicsEventTryWait(
    epicsEventId id);
epicsShareFunc void epicsShareAPI epicsEventShow(
    epicsEventId id, unsigned int level);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#include "osdEvent.h"

#endif /* epicsEventh */
