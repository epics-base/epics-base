/*
 * RTEMS startup task for EPICS
 *	$Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <bsp.h>
#include <rtems/rtems_bsdnet.h>
#include <rtems/tftp.h>
#include <rtems/monitor.h>

#include <osiThread.h>
#include <logClient.h>
#include <CommandInterpreter.h>
#include <dbStaticLib.h>
#include <cantProceed.h>

/*
 ***********************************************************************
 *                         RTEMS CONFIGURATION                         *
 ***********************************************************************
 */
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_EXECUTIVE_RAM_SIZE		(700*1024)
#define CONFIGURE_MAXIMUM_TASKS			60
#define CONFIGURE_MAXIMUM_SEMAPHORES		180
#define CONFIGURE_MAXIMUM_TIMERS		50
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES	30

#define CONFIGURE_MICROSECONDS_PER_TICK		20000

#define CONFIGURE_INIT_TASK_PRIORITY    220
#define NETWORK_TASK_PRIORITY    	120

#define CONFIGURE_INIT
#define CONFIGURE_INIT_TASK_INITIAL_MODES (RTEMS_PREEMPT | \
					RTEMS_NO_TIMESLICE | \
					RTEMS_NO_ASR | \
					RTEMS_INTERRUPT_LEVEL(0))
#define CONFIGURE_INIT_TASK_INITIAL_ATTRIBUTES (RTEMS_FLOATING_POINT | \
					RTEMS_LOCAL)
#define CONFIGURE_INIT_TASK_STACK_SIZE  (12*1024)
rtems_task Init (rtems_task_argument argument);

#define CONFIGURE_HAS_OWN_DEVICE_DRIVER_TABLE
rtems_driver_address_table Device_drivers[] = {
  CONSOLE_DRIVER_TABLE_ENTRY,
  CLOCK_DRIVER_TABLE_ENTRY,
};

#include <confdefs.h>

/*
 * Network configuration
 */
extern void rtems_bsdnet_loopattach();
static struct rtems_bsdnet_ifconfig loopback_config = {
	"lo0",                          /* name */
	rtems_bsdnet_loopattach,        /* attach function */
	NULL,                           /* link to next interface */
	"127.0.0.1",                    /* IP address */
	"255.0.0.0",                    /* IP net mask */
};
static struct rtems_bsdnet_ifconfig netdriver_config = {
	RTEMS_BSP_NETWORK_DRIVER_NAME,      /* name */
	RTEMS_BSP_NETWORK_DRIVER_ATTACH,    /* attach function */
    &loopback_config,                   /* link to next interface */
};
struct rtems_bsdnet_config rtems_bsdnet_config = {
	&netdriver_config,	    /* Network interface */
	rtems_bsdnet_do_bootp,	/* Use BOOTP to get network configuration */
	NETWORK_TASK_PRIORITY,	/* Network task priority */
	150*1024,	        	/* MBUF space */
	300*1024,	        	/* MBUF cluster space */
};

/*
 ***********************************************************************
 *                         FATAL ERROR REPORTING                       *
 ***********************************************************************
 */
/*
 * Delay for a while, then terminate
 */
static void
delayedPanic (const char *msg)
{
	rtems_interval ticksPerSecond;

	rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &ticksPerSecond);
	rtems_task_wake_after (ticksPerSecond);
	rtems_panic (msg);
}

/*
 * Log error and terminate
 */
void
LogFatal (const char *msg, ...)
{
	va_list ap;

	va_start (ap, msg);
	vsyslog (LOG_ALERT,  msg, ap);
	va_end (ap);
	delayedPanic (msg);
}

/*
 * Log RTEMS error and terminate
 */
void
LogRtemsFatal (const char *msg, rtems_status_code sc)
{
	syslog (LOG_ALERT, "%s: %s", msg, rtems_status_text (sc));
	delayedPanic (msg);
}

/*
 * Log network error and terminate
 */
void
LogNetFatal (const char *msg, int err)
{
	syslog (LOG_ALERT, "%s: %d", msg, err);
	delayedPanic (msg);
}

/*
 ***********************************************************************
 *                         REMOTE FILE ACCESS                          *
 ***********************************************************************
 */
/*
 * Add TFTP server and target prefix to pathname
 */
static char *
rtems_tftp_path (const char *name)
{
	char *path;
	int pathsize = 200;
	int l;

	if ((path = malloc (pathsize)) == NULL)
		LogFatal ("Can't create TFTP path name -- no memory.\n");
	strcpy (path, "/TFTP/");
	l = strlen (path);
	if (inet_ntop (AF_INET, &rtems_bsdnet_bootp_server_address, &path[l], pathsize - l) == NULL)
		LogFatal ("Can't convert BOOTP server name");
	l = strlen (path);
	strcpy (&path[l], "/epics/");
	l = strlen (path);
	if (gethostname (&path[l], pathsize - l) || (path[l] == '\0'))
		LogFatal ("Can't get host name");
	l = strlen (path);
	path[l++] = '/';
	for (;;) {
		if (name[0] == '.') {
			if (name[1] == '/') {
				name += 2;
				continue;
			}
			if ((name[1] == '.') && (name[2] == '/')) {
				name += 3;
				continue;
			}
		}
		break;
	}
	path = realloc (path, l + 1 + strlen (name));
	strcpy (&path[l], name);
	return path;
}

/*
 ***********************************************************************
 *                         RTEMS/EPICS COMMANDS                        *
 ***********************************************************************
 */
/*
 * RTEMS status
 */
long
rtems_showStats (unsigned int level)
{
        rtems_bsdnet_show_if_stats ();
        rtems_bsdnet_show_mbuf_stats ();
	if (level >= 1) {
		rtems_bsdnet_show_inet_routes ();
	}
	if (level >= 2) {
		rtems_bsdnet_show_ip_stats ();
		rtems_bsdnet_show_icmp_stats ();
		rtems_bsdnet_show_udp_stats ();
		rtems_bsdnet_show_tcp_stats ();
	}
	return 0;
}

long 
rtems_showSem (void)
{
	Semaphore_Control *sem;
	int i;
	int n;

	n = 0;
	for (i = 0 ; i < _Semaphore_Information.maximum ; i++) {
		sem =  (Semaphore_Control *)_Semaphore_Information.local_table[i];
		if (sem) {
			char *cp = sem->Object.name;
			char cbuf[4];
			int j;

			for (j = 0 ; j < 4 ; j++) {
				unsigned char c = cp[j];
				if (isprint (c))
					cbuf[j] = c;
				else
					cbuf[j] = ' ';
			}
			printf ("%4.4s%9x%5x%5d", cbuf, sem->Object.id,
				sem->attribute_set,
				sem->attribute_set & RTEMS_BINARY_SEMAPHORE ?
					sem->Core_control.mutex.lock :
					sem->Core_control.semaphore.count);
			n++;
			if ((n % 3) == 0)
				printf ("\n");
			else
				printf ("   ");
		}
	}
	if ((n % 3) != 0)
		printf ("\n");
	printf ("%d/%d\n", n, _Semaphore_Information.maximum);
	return 0;
}

/*
 * Dummy commands
 *	RTEMS does not yet support full NFS.
 *	RTEMS does not support dynamic loading.
 */
long
rtems_cdCommand (char *name)
{
	printf ("`cd' command ignored.\n");
	return 0;
}

long
rtems_ldCommand (char *name)
{
	printf ("`ld' command ignored.\n");
	return 0;
}

/*
 * Wrappers for EPICS routines which refer to file names.
 * Since RTEMS doesn't have NFS we fake it by making sure that
 * all paths refer to files in the TFTP area.
 */
long
rtems_registerRecordDeviceDriver (char *name)
{
	extern DBBASE *pdbbase;
	int registerRecordDeviceDriver(DBBASE *pdbbase);

	if (strcmp (name, "pdbbase") == 0)
		return registerRecordDeviceDriver (pdbbase);
	errlogPrintf ("Don't know how to registerRecordDeviceDriver(%s)\n", name);
	cantProceed ("registerRecordDeviceDriver");
	return -1;
}

long
rtems_dbLoadDatabase (char *name)
{
	char *cp = rtems_tftp_path (name);
	int dbLoadDatabase (char *filename, char *path, char *substitutions);

	dbLoadDatabase (cp, "/", NULL);
	free (cp);
	return 0;
}

long
rtems_dbLoadRecords (char *name, char *substitutions)
{
	char *cp = rtems_tftp_path (name);
	int dbLoadRecords (char* pfilename, char* substitutions);

	dbLoadRecords (cp, substitutions);
	free (cp);
	return 0;
}

void
rtems_runScript (const char *name)
{
	char *cp;
	FILE *fp;

	cp = rtems_tftp_path (name);
	fp = fopen (cp, "r");
	if (fp == NULL) {
		printf ("Can't open script (%s)\n", name);
	}
	else {
		CommandInterpreter (NULL, fp, name);
		fclose (fp);
	}
	free (cp);
}

void
rtems_reboot (const char *name)
{
	int c;

	printf ("Are you sure you want to reboot the IOC? ");
	fflush (stdout);
	if ((c = getchar ()) == 'Y')
		LogFatal ("Reboot");
	while ((c != '\n') && (c != EOF))
		c = getchar ();
}

/*
 * RTEMS-specific commands
 */
typedef long (*cmd)();
static const struct CommandTableEntry CommandTable[] = {
	{ "<",
	  "Redirect command",
	  "*",		(cmd)rtems_runScript,	2, 2
	},
	{ "Reboot",
	  "Rebot IOC",
	  NULL,		(cmd)rtems_reboot,	1, 1
	},
	{ "cd",
	  "Change directories",
	  NULL,		rtems_cdCommand,	2, 2
	},
	{ "ld",
	  "Load object module",
	  NULL,		rtems_ldCommand,	3, 3
	},
	{ "dbLoadDatabase",
	  "Load database",
	  "*",		rtems_dbLoadDatabase,	2, 2
	},
	{ "dbLoadRecords",
	  "Load database records",
	  "**",		rtems_dbLoadRecords,	2, 3
	},
	{ "registerRecordDeviceDriver",
	  "Register device driver",
	  "*",		rtems_registerRecordDeviceDriver,	2, 2
	},
	{ "network",
	  "Show network statistics",
	  "i",			rtems_showStats,	1, 2
	},
	{ "semaphore",
	  "Show semaphores",
	  "",			rtems_showSem,	1, 1
	},
	{ NULL }
};

/*
 * RTEMS Startup task
 */
rtems_task
Init (rtems_task_argument ignored)
{
	void registerBaseCommands (void);

	/*
	 * Create a reasonable environment
	 */
	putenv ("TERM=xterm");
	putenv ("PS1=rtems> ");
	putenv ("HISTSIZE=10");
	putenv ("IFS= \t,()");

	/*
	 * Start network
	 */
	printf ("***** Initializing network *****\n");
	rtems_bsdnet_initialize_network ();
	printf ("***** Initializing TFTP *****\n");
	rtems_bsdnet_initialize_tftp_filesystem ();
	printf ("***** Initializing NTP *****\n");
	rtems_bsdnet_synchronize_ntp (0, 0);
	printf ("***** Initializing syslog *****\n");
	openlog ("IOC", LOG_CONS, LOG_DAEMON);
	syslog (LOG_NOTICE, "IOC started.");

	/*
	 * Do some RTEMS initializations
	 */
	clockInit ();
	threadInit ();

	/*
	 * Run the EPICS startup script
	 */
	printf ("***** Executing EPICS startup script *****\n");
	registerBaseCommands ();
	CommandInterpreterRegisterCommands (CommandTable);
	rtems_runScript ("st.cmd");

	/*
	 * Everything's running!
	 */
	threadSleep (2.0);
	CommandInterpreter (NULL, NULL, NULL);
	rtems_task_suspend (RTEMS_SELF);
}
