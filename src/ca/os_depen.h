/*
 *	O S _ D E P E N . H
 *
 *
 *	OS dependent stuff for channel access
 *	Author Jeffrey O. Hill
 *
 *	History
 *	.01 joh	110190	Moved to this file
 *	.02 joh 082891	on UNIX copy in a new delay into the timeval 
 *			arg to select each call in TCPDELAY in case 
 *			the system modifies my timeval structure.
 *			Also droped the TCPDELAY to 50 mS
 *	.03 joh	090391	Updated for V5 vxWorks
 *	.04 joh 092491	changed order of ops on LOCALTICKS
 *	.05 joh	092691	added NELEMENTS()
 *	.06 joh 111991	added EVENTLOCKTEST
 *	.07 joh 120291	added declaration of taskIdCurrent for
 *			compiling with V4 vxWorks 
 *	.08 joh	062692	took out printf to logMsg MACRO
 *	.09 joh 101692	dont quit if select() is interrupted in
 *			the UNIX version of TCPDELAY
 *	.10 joh	112092	removed the requirement that VMS locking
 *			pairs reside at the same C bracket level
 *      .11 GeG 120992	support VMS/UCX
 *      .12 CJM 130794  define MYERRNO properly for UCX
 *	.13 CJM 311094  mods to support DEC C compiler
 *	.14 joh 100695	removed UNIX include of filio.h and sockio.h
 *			because they are include by ioctl.h under
 *			BSD and SVR4 (and they dont exist under linux)
 */

#ifndef INCos_depenh
#define INCos_depenh

static char *os_depenhSccsId = "$Id$";

#ifdef UNIX
#	define CA_OS_CONFIGURED
#endif

#ifdef iocCore
#	include "osiSem.h"
#	include "osiThread.h"
#	include "taskwd.h"

#	define CA_OS_CONFIGURED
#endif

#ifdef VMS
#	define CA_OS_CONFIGURED
#endif /*VMS*/

#ifdef _WIN32
#	define CA_OS_CONFIGURED
#endif /*_WIN32*/

#ifndef CA_OS_CONFIGURED
#error Please define one of iocCore, UNIX, VMS, or _WIN32 
#endif

#if defined(iocCore)
#  	define POST_IO_EV semBinaryGive(io_done_sem)
#	define VXTASKIDNONE 0
#  	define LOCK semMutexTakeAssert(client_lock); 
#  	define UNLOCK semMutexGive(client_lock);
#	define EVENTLOCKTEST (lock_tid==threadGetIdSelf())
#	define VXTHISTASKID threadGetIdSelf();
#	define abort() threadSuspend(threadGetIdSelf())
#else
#  	define POST_IO_EV 
#  	define LOCK
#  	define UNLOCK  
#	define EVENTLOCKTEST	(post_msg_active)
#endif /*defined(iocCore) */

#endif /* INCos_depenh */

