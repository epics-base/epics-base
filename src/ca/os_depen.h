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

#ifdef vxWorks
#	include <vxWorks.h>
#	include <ioLib.h>
#	include <tickLib.h>
#	include <taskHookLib.h>
#	include <selectLib.h>
#       include <errnoLib.h>
#       include <sysLib.h>
#       include <taskVarLib.h>
#       include <hostLib.h>
#       include <logLib.h>
#       include <usrLib.h>
#       include <dbgLib.h>
#	include <inetLib.h>
#	include <taskLib.h>
#	include <vxLib.h>

#	include "task_params.h"
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
#error Please define one of vxWorks, UNIX, VMS, or _WIN32 
#endif

#if defined(vxWorks)
#  	define POST_IO_EV semGive(io_done_sem)
#	define VXTASKIDNONE 0
#  	define LOCK {assert(semTake(client_lock, WAIT_FOREVER)==OK); lock_count++; lock_tid=(int)taskIdCurrent;}
#  	define UNLOCK {if(--lock_count==0u) lock_tid=VXTASKIDNONE; assert(semGive(client_lock)==OK);}
#	define EVENTLOCKTEST (lock_tid==(int)taskIdCurrent)
#	define VXTHISTASKID taskIdSelf()
#	define abort() taskSuspend(VXTHISTASKID)
#endif

#if defined(UNIX)
#  	define POST_IO_EV 
#  	define LOCK
#  	define UNLOCK  
#	define EVENTLOCKTEST	(post_msg_active)
#endif

#if defined(VMS)
#  	define POST_IO_EV 
#	define LOCK
#	define UNLOCK
#	define EVENTLOCKTEST	(post_msg_active)
#endif

#ifdef _WIN32 
#	define POST_IO_EV
#  	define LOCK
#  	define UNLOCK  
#	define EVENTLOCKTEST	(post_msg_active)
#endif /*_WIN32*/

#endif /* INCos_depenh */

