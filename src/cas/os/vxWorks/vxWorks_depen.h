

#include <vxWorks.h>
#include <taskLib.h>
#include <errnoLib.h>
#include <logLib.h>
#include <tickLib.h>
#include <sysLib.h>
#include <vxLib.h>
#include <ioLib.h>
#include <usrLib.h>
#include <semLib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sockLib.h> /* for sendto() setsockopt() etc */
#include <netinet/in.h>
#include <netinet/tcp.h>

/*
 * EPICS on vxWorks
 */
#include <task_params.h>
#include <taskwd.h>
#include <fdmgr.h>

extern size_t caMaxFreeBlock;
#define MAXFREEBLOCKPOLL        5 /* sec */
#define MAXFREEBLOCKTASKNAME    "caMemPoll"
#define MAXFREEBLOCKTASKPRI     255
#define MAXFREEBLOCKTASKOPT     (VX_FP_TASK)
#define MAXFREEBLOCKTASKSTACK   4095

/*
 * per server locks
 * ================
 */
#define LOCK_CLIENTQ(PRSRV) \
{ \
        int status; \
        status = semTake((PRSRV)->osSpecific.clientQLock, WAIT_FOREVER); \
        assert(status==OK); \
}

#define UNLOCK_CLIENTQ(PRSRV) \
{ \
        int status; \
        status = semGive((PRSRV)->osSpecific.clientQLock); \
        assert(status==OK); \
}

#define LOCK_FREE_CHAN_Q(PRSRV) \
{\
        int status;\
        status = semTake((PRSRV)->osSpecific.freeChanQLock, WAIT_FOREVER);\
        assert(status==OK);\
}

#define UNLOCK_FREE_CHAN_Q(PRSRV) \
{ \
        int status;\
        status = semGive((PRSRV)->osSpecific.freeChanQLock);\
        assert(status==OK);\
}

#define LOCK_FREE_EVENT_Q(PRSRV) \
{\
        int status;\
        status = semTake((PRSRV)->osSpecific.freeEventQLock, WAIT_FOREVER);\
        assert(status==OK);\
}

#define UNLOCK_FREE_EVENT_Q(PRSRV) \
{ \
        int status;\
        status = semGive((PRSRV)->osSpecific.freeEventQLock);\
        assert(status==OK);\
}

#define LOCK_PVQ(PRSRV) \
{ \
        int status; \
        status = semTake((PRSRV)->osSpecific.pvQLock, WAIT_FOREVER); \
        assert(status==OK); \
}

#define UNLOCK_PVQ(PRSRV) \
{ \
        int status;\
        status = semGive((PRSRV)->osSpecific.pvQLock);\
        assert(status==OK);\
}

/*
 * per client locks
 * ================
 */
#define SEND_LOCK(CLIENT) \
{\
        int status;\
        status = semTake((CLIENT)->cc.osSpecific.lock, WAIT_FOREVER);\
        assert(status==OK);\
}

#define SEND_UNLOCK(CLIENT) \
{ \
        int status;\
        status = semGive((CLIENT)->cc.osSpecific.lock);\
        assert(status==OK);\
}

#define LOCK_CHAN_Q(PCLIENT) \
{\
        int status;\
        status = semTake((PCLIENT)->cc.osSpecific.chanQLock, WAIT_FOREVER);\
        assert(status==OK);\
}

#define UNLOCK_CHAN_Q(PCLIENT) \
{ \
        int status;\
        status = semGive((PCLIENT)->cc.osSpecific.chanQLock);\
        assert(status==OK);\
}

#define LOCK_EVENT_Q(PCLIENT) \
{\
        int status;\
        status = semTake((PCLIENT)->cc.osSpecific.eventQLock, WAIT_FOREVER);\
        assert(status==OK);\
}

#define UNLOCK_EVENT_Q(PCLIENT) \
{ \
        int status;\
        status = semGive((PCLIENT)->cc.osSpecific.eventQLock);\
        assert(status==OK);\
}

#define LOCK_PUT_NOTIFY(PCLIENT) \
{\
        int status;\
        status = semTake((PCLIENT)->cc.osSpecific.putNotifyLock, WAIT_FOREVER);\
        assert(status==OK);\
}

#define UNLOCK_PUT_NOTIFY(PCLIENT) \
{ \
        int status;\
        status = semGive((PCLIENT)->cc.osSpecific.putNotifyLock);\
        assert(status==OK);\
}

#define LOCK_ACCESS_RIGHTS_Q(PCLIENT) \
{\
        int status;\
        status = semTake((PCLIENT)->cc.osSpecific.accessRightsQLock, \
		WAIT_FOREVER);\
        assert(status==OK);\
}

#define UNLOCK_ACCESS_RIGHTS_Q(PCLIENT) \
{ \
        int status;\
        status = semGive((PCLIENT)->cc.osSpecific.accessRightsQLock);\
        assert(status==OK);\
}


/*
 * event system locks
 * ==================
 */
#define CA_EVENT_LOCK(PEVUSER) \
{ \
	int status;\
	status = semTake((PEVUSER)->osSpecific.lock, WAIT_FOREVER);\
	assert(status==OK);\
}
#define CA_EVENT_UNLOCK(PEVUSER) \
{ \
	int status;\
	status = semGive((PEVUSER)->osSpecific.lock);\
	assert(status==OK);\
}

#define LOCK_PV(PPV) \
{ \
	int status;\
	status = semTake((PPV)->osSpecific.lock, WAIT_FOREVER);\
	assert(status==OK);\
}

#define UNLOCK_PV(PPV) \
{ \
	int status;\
	status = semGive((PPV)->osSpecific.lock);\
	assert(status==OK);\
}

/*
 * vxWorks specific addition to event_user struct
 */
typedef struct {
	SEM_ID		lock;
	SEM_ID		ppendsem;
	int		taskid;
	char		pendlck;
}caeqOsSpecific;

typedef struct {
	SEM_ID		lock;
}caPVOsSpecific;


/*
 * vxWorks specific addition to rsrv struct
 */
typedef struct {
	fdctx		*pfdctx;
	SEM_ID		clientQLock;
	SEM_ID		pvQLock;
	SEM_ID		freeChanQLock;
	SEM_ID		freeEventQLock;
	int		selectTask;
}rsrvOsSpecific;

/*
 * vxWorks specific addition to client struct
 */
typedef struct {
	SEM_ID		lock;
	SEM_ID		putNotifyLock;
	SEM_ID		chanQLock;
	SEM_ID		eventQLock;
	SEM_ID		accessRightsQLock;
	fdmgrAlarm	*pPutCBAlarm;
	unsigned	sockSendIOCallbackEnabled:1;
	unsigned	sockRecvIOCallbackEnabled:1;
}clientOsSpecific;

typedef int	SOCKET;

#define SOCKERRNO errno

#define socket_close(S) close(S)
/* vxWorks still has brain dead func proto for ioctl() */
#define socket_ioctl(A,B,C) ioctl(A,B,(int)C)

/* leave task around for debugging if we fail */
#define abort(A) taskSuspend(0)




