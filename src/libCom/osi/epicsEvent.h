#ifndef epicsEventh
#define epicsEventh

#include "epicsAssert.h"
#include "shareLib.h"

typedef struct epicsEventOSD *epicsEventId;

typedef enum {
    epicsEventWaitOK,epicsEventWaitTimeout,epicsEventWaitError
} epicsEventWaitStatus;

typedef enum {epicsEventEmpty,epicsEventFull} epicsEventInitialState;

#ifdef __cplusplus

#include "locationException.h"

class epicsShareClass epicsEvent {
public:
    epicsEvent ( epicsEventInitialState initial = epicsEventEmpty );
    ~epicsEvent ();
    void signal ();
    void wait (); /* blocks until full */
    bool wait ( double timeOut ); /* false if empty at time out */
    bool tryWait (); /* false if empty */
    void show ( unsigned level ) const;

    class invalidSemaphore {}; /* exception */
    class noMemory {}; /* exception */
private:
    epicsEvent ( const epicsEvent & );
    epicsEvent & operator = ( const epicsEvent & );
    epicsEventId id;
};

#endif /*__cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus */

epicsShareFunc epicsEventId epicsShareAPI epicsEventCreate(
    epicsEventInitialState initialState);
epicsShareFunc epicsEventId epicsShareAPI epicsEventMustCreate (
    epicsEventInitialState initialState);
epicsShareFunc void epicsShareAPI epicsEventDestroy(epicsEventId id);
epicsShareFunc void epicsShareAPI epicsEventSignal(epicsEventId id);
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventWait(
    epicsEventId id);
#define epicsEventMustWait(ID) assert((epicsEventWait ((ID))==epicsEventWaitOK))
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventWaitWithTimeout(
    epicsEventId id, double timeOut);
epicsShareFunc epicsEventWaitStatus epicsShareAPI epicsEventTryWait(
    epicsEventId id);
epicsShareFunc void epicsShareAPI epicsEventShow(
    epicsEventId id, unsigned int level);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#include "osdEvent.h"

/*The c++ implementation consists of inline calls to the c code */
#ifdef __cplusplus
inline epicsEvent::epicsEvent (epicsEventInitialState initial) :
    id ( epicsEventCreate ( initial ) )
{
    if ( this->id == 0 ) {
        throwWithLocation ( noMemory () );
    }
}

inline epicsEvent::~epicsEvent ()
{
    epicsEventDestroy ( this->id );
}

inline void epicsEvent::signal ()
{
    epicsEventSignal ( this->id );
}

inline void epicsEvent::wait ()
{
    epicsEventWaitStatus status;
    status = epicsEventWait (this->id);
    if (status!=epicsEventWaitOK) {
        throwWithLocation ( invalidSemaphore () );
    }
}

inline bool epicsEvent::wait (double timeOut)
{
    epicsEventWaitStatus status;
    status = epicsEventWaitWithTimeout (this->id, timeOut);
    if (status==epicsEventWaitOK) {
        return true;
    } else if (status==epicsEventWaitTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false;
}

inline bool epicsEvent::tryWait ()
{
    epicsEventWaitStatus status;
    status = epicsEventTryWait (this->id);
    if (status==epicsEventWaitOK) {
        return true;
    } else if (status==epicsEventWaitTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false;
}

inline void epicsEvent::show ( unsigned level ) const
{
    epicsEventShow ( this->id, level );
}

#endif /*__cplusplus */

#endif /* epicsEventh */
