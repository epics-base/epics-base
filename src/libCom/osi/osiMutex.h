
#include "osiSem.h"
#include "shareLib.h"

class epicsShareClass osiMutex {
public:
    osiMutex ();
    ~osiMutex ();
    void lock () const; /* blocks until success */
    bool lock (double timeOut) const; /* true if successful */
    bool tryLock () const; /* true if successful */
    void unlock () const;
    void show (unsigned level) const;

    class invalidSemaphore {}; /* exception */
    class noMemory {}; /* exception */
private:
    mutable semMutexId id;
};

inline osiMutex::osiMutex ()
{
    this->id = semMutexCreate ();
    if (this->id==0) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw noMemory ();
#       endif
    }
}

inline osiMutex::~osiMutex ()
{
    semMutexDestroy (this->id);
}

inline void osiMutex::lock () const
{
    semTakeStatus status;
    status = semMutexTake (this->id);
    if (status!=semTakeOK) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw invalidSemaphore ();
#       endif
    }
}

inline bool osiMutex::lock (double timeOut) const
{
    semTakeStatus status;
    status = semMutexTakeTimeout (this->id, timeOut);
    if (status==semTakeOK) {
        return true;
    }
    else if (status==semTakeTimeout) {
        return false;
    }
    else {
#       ifdef noExceptionsFromCXX
            assert (0);
            return false;
#       else            
            throw invalidSemaphore ();
#       endif
    }
}

inline bool osiMutex::tryLock () const
{
    semTakeStatus status;
    status = semMutexTakeNoWait (this->id);
    if (status==semTakeOK) {
        return true;
    }
    else if (status==semTakeTimeout) {
        return false;
    }
    else {
#       ifdef noExceptionsFromCXX
            assert (0);
            return false;
#       else            
            throw invalidSemaphore ();
#       endif
    }
}

inline void osiMutex::unlock () const
{
    semMutexGive (this->id);
}

inline void osiMutex::show (unsigned level) const
{
    semMutexShow (this->id, level);
}

