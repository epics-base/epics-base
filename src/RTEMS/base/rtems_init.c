/*
 * RTEMS startup task for EPICS
 *  $Id$
 *      Author: W. Eric Norum
 *              eric.norum@usask.ca
 *              (306) 966-5394
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <rtems.h>
#include <rtems/error.h>
#include <rtems/stackchk.h>
#include <rtems/rtems_bsdnet.h>
#include <rtems/tftp.h>

#include <epicsThread.h>
#include <logClient.h>
#include <iocsh.h>

static void logReset (void);

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
 * Add TFTP server and target prefix to pathnames
 */
static void
rtems_set_directory (void)
{
    char *path;
    int pathsize = 200;
    int l;

    if ((path = malloc (pathsize)) == NULL)
        LogFatal ("Can't create TFTP path name -- no memory.\n");
    strcpy (path, "/TFTP/BOOTP_HOST/epics/");
    l = strlen (path);
    if (gethostname (&path[l], pathsize - l - 2) || (path[l] == '\0'))
        LogFatal ("Can't get host name");
    strcat (path, "/");
    if (chdir (path) < 0)
        LogFatal ("Can't set initial TFTP directory");
}

/*
 ***********************************************************************
 *                         RTEMS/EPICS COMMANDS                        *
 ***********************************************************************
 */
/*
 * RTEMS status
 */
static void
rtems_netstat (unsigned int level)
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
}

static const iocshArg netStatArg0 = { "level",iocshArgInt};
static const iocshArg * const netStatArgs[1] = {&netStatArg0};
static const iocshFuncDef netStatFuncDef = {"netstat",1,netStatArgs};
static void netStatCallFunc(const iocshArgBuf *args)
{
    rtems_netstat(args[0].ival);
}
static const iocshFuncDef stackCheckFuncDef = {"stackCheck",0,NULL};
static void stackCheckCallFunc(const iocshArgBuf *args)
{
    Stack_check_Dump_usage ();
}
static void iocshRegisterRTEMS (void)
{
    iocshRegister(&netStatFuncDef, netStatCallFunc);
    iocshRegister(&stackCheckFuncDef, stackCheckCallFunc);
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
 * Set up the console serial line (no handshaking)
 */
static void
initConsole (void)
{
    struct termios t;

    if (tcgetattr (fileno (stdin), &t) < 0) {
        printf ("tcgetattr failed: %s\n", strerror (errno));
        return;
    }
    t.c_iflag &= ~(IXOFF | IXON | IXANY);
    if (tcsetattr (fileno (stdin), TCSANOW, &t) < 0) {
        printf ("tcsetattr failed: %s\n", strerror (errno));
        return;
    }
}

/*
 * Ensure that the configuration object files
 * get pulled in from the library
 */
extern rtems_configuration_table Configuration;
extern struct rtems_bsdnet_config rtems_bsdnet_config;
const void *rtemsConfigArray[] = {
    &Configuration,
    &rtems_bsdnet_config
};

/*
 * RTEMS Startup task
 */
rtems_task
Init (rtems_task_argument ignored)
{
    int i;
    char arg0[] = "RTEMS_IOC";
    char arg1[] = "st.cmd";
	char *argv[3] = { arg0, arg1, NULL };

    /*
     * Create a reasonable environment
     */
    initConsole ();
    putenv ("TERM=xterm");
    putenv ("IOCSH_PS1=epics> ");
    putenv ("IOCSH_HISTSIZE=20");

    /*
     * Start network
     */
    printf ("***** Initializing network *****\n");
    rtems_bsdnet_initialize_network ();
    printf ("***** Initializing TFTP *****\n");
    rtems_bsdnet_initialize_tftp_filesystem ();
    for (i = 0 ; ; i++) {
        printf ("***** Initializing NTP *****\n");
        if (rtems_bsdnet_synchronize_ntp (0, 0) >= 0)
            break;
        epicsThreadSleep (5.0);
        if (i >= 12) {
            rtems_status_code sc;
            rtems_time_of_day now;
            printf ("    *************** WARNING ***************\n");
            printf ("    ***** NO RESPONSE FROM NTP SERVER *****\n");
            printf ("    *****  TIME SET TO DEFAULT VALUE  *****\n");
            printf ("    ***************************************\n");
            now.year = 2001;
            now.month = 1;
            now.day = 1;
            now.hour = 0;
            now.minute = 0;
            now.second = 0;
            now.ticks = 0;
            if ((sc = rtems_clock_set (&now)) != RTEMS_SUCCESSFUL)
                printf ("***** Can't set time: %s\n", rtems_status_text (sc));
            break;
        }
    }
    printf ("***** Initializing syslog *****\n");
    openlog ("IOC", LOG_CONS, LOG_DAEMON);
    logReset ();

    /*
     * Run the EPICS startup script
     */
    printf ("***** Starting EPICS application *****\n");
    iocshRegisterRTEMS ();
    rtems_set_directory ();
    main ((sizeof argv / sizeof argv[0]) - 1, argv);
    LogFatal ("Console command interpreter terminated");
}

/*
 * Architecture-dependent routines
 */
#ifdef __mcpu32__

#include <m68360.h>
static void
logReset (void)
{
    int bit, rsr;
    int i;
    const char *cp;
    char cbuf[80];

    rsr = m360.rsr;
    for (i = 0, bit = 0x80 ; bit != 0 ; bit >>= 1) {
        if (rsr & bit) {
            switch (bit) {
            case 0x80:  cp = "RESETH*";         break;
            case 0x40:  cp = "POWER-UP";        break;
            case 0x20:  cp = "WATCHDOG";        break;
            case 0x10:  cp = "DOUBLE FAULT";    break;
            case 0x04:  cp = "LOST CLOCK";      break;
            case 0x02:  cp = "RESET";           break;
            case 0x01:  cp = "RESETS*";         break;
            default:    cp = "??";              break;
            }
            i += sprintf (cbuf+i, cp); 
            rsr &= ~bit;
            if (rsr)
                i += sprintf (cbuf+i, ", "); 
            else
                break;
        }
    }
    syslog (LOG_NOTICE, "Startup after %s.", cbuf);
    printf ("Startup after %s.\n", cbuf);
    m360.rsr = ~0;
}

#else

static void
logReset (void)
{
    syslog (LOG_NOTICE, "Started.");
}

#endif
