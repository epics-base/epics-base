/*
 * $Id$
 */
#ifdef DECTHREAD
#include <pthread.h>
#include <timers.h> 
#endif
#ifdef FSUTHREAD
#include "pthread/pthread.h"
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <ctype.h>

#define GLOBALLOCK \
	{\
	pthread_lock_global_np(); \
	}

#define GLOBALUNLOCK \
	{\
	pthread_unlock_global_np(); \
	}


#define FASTLOCKINIT(PFAST_LOCK) \
	{\
	mutex_init(PFAST_LOCK,0,NULL); \
	}

#define FASTLOCKFREE(PFAST_LOCK) \
	{\
	pthread_mutex_destroy(PFAST_LOCK); \
	}

#define FASTLOCK(PFAST_LOCK) \
	{\
	mutex_enter( (PFAST_LOCK)); \
	}

#define FASTUNLOCK(PFAST_LOCK) \
	{\
	mutex_exit( (PFAST_LOCK)); \
	}

#define FASTLOCKNOWAIT(PFAST_LOCK) \
	{\
	mutex_tryenter( (PFAST_LOCK)); \
	}

/* #define DEBUG */
 
#define THDNUM		100	

#define ST_INIT         0
#define ST_LOCKED       1
#define ST_RUNNING      2
#define ST_SUSPEND      3
#define ST_CANCELED     4

/*char *st_status[] = {
	"INIT",
	"LOCKED",
	"RUNNING",
	"SUSPENDED",
	"CANCELED"};
*/ 

/* DEC/POSIX  type to FSU POSIX type*/
#ifdef FSUTHREAD
#define pthread_startroutine_t  pthread_func_t
#define pthread_addr_t		any_t 
#endif

/* Sun Solaris 5.0 using DEC/ POSIX */
#define thread_id_t	pthread_t
#define mutex_t		pthread_mutex_t
#define condvar_t	pthread_cond_t

#define STATUS int

#define PROMPT          "Seq> "         /* prompt string */
#define MAXLINSIZE 	81		/* command line width */

#define check(status,string) if(status == -1)perror(string)

#define FAST_LOCK pthread_mutex_t	/* fastlock for pthread_mutex_t */



