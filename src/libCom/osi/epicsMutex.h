#ifndef epicsMutexh
#define epicsMutexh

#include <stdarg.h>
#include "epicsAssert.h"
#include "shareLib.h"

typedef struct epicsMutexOSD *epicsMutexId;
typedef enum {
    epicsMutexLockOK,epicsMutexLockTimeout,epicsMutexLockError
} epicsMutexLockStatus;

#ifdef __cplusplus

class epicsShareClass epicsMutex {
public:
    epicsMutex ();
    ~epicsMutex ();
    void lock (); /* blocks until success */
    bool lock ( double timeOut ); /* true if successful */
    bool tryLock (); /* true if successful */
    void unlock ();
    void show ( unsigned level ) const;

    class invalidSemaphore {}; // exception
private:
    epicsMutexId id;
    epicsMutex ( const epicsMutex & );
    epicsMutex & operator = ( const epicsMutex & );
};

#endif /*__cplusplus*/

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
/* The following should NOT be called by user code*/
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsdCreate(void);
epicsShareFunc void epicsShareAPI epicsMutexOsdDestroy(epicsMutexId id);
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsiCreate(
    const char *pFileName,int lineno);
epicsShareFunc epicsMutexId epicsShareAPI epicsMutexOsiMustCreate(
    const char *pFileName,int lineno);

#define epicsMutexCreate() epicsMutexOsiCreate(__FILE__,__LINE__)
#define epicsMutexMustCreate() epicsMutexOsiMustCreate(__FILE__,__LINE__)
epicsShareFunc void epicsShareAPI epicsMutexDestroy(epicsMutexId id);
epicsShareFunc void epicsShareAPI epicsMutexUnlock(epicsMutexId id);
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLock(
    epicsMutexId id);
#define epicsMutexMustLock(ID) assert((epicsMutexLock((ID))==epicsMutexLockOK))
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexLockWithTimeout(
    epicsMutexId id, double timeOut);
epicsShareFunc epicsMutexLockStatus epicsShareAPI epicsMutexTryLock(
    epicsMutexId id);
epicsShareFunc void epicsShareAPI epicsMutexShow(
    epicsMutexId id,unsigned  int level);
epicsShareFunc void epicsShareAPI epicsMutexShowAll(
    int onlyLocked,unsigned  int level);

/*NOTES:
    epicsMutex MUST implement recursive locking
    epicsMutex should implement priority inheritance and deletion safe
*/

#ifdef __cplusplus
}
#endif

#include "osdMutex.h"

#endif /* epicsMutexh */
