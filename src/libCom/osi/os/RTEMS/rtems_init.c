/*
 * RTEMS startup task for EPICS
 *  $Id$
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
#include <syslog.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <rtems.h>
#include <rtems/error.h>
#include <rtems/rtems_bsdnet.h>
#include <rtems/tftp.h>

#include <osiThread.h>
#include <logClient.h>
#include <ioccrf.h>
#include <dbStaticLib.h>

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

static void 
rtems_semstat (int level)
{
    Semaphore_Control *sem;
    int i;
    int n;

    n = 0;
    for (i = 0 ; i < _Semaphore_Information.maximum ; i++) {
        sem =  (Semaphore_Control *)_Semaphore_Information.local_table[i];
        if (sem) {
            if (level == 0) {
                n++;
            }
            else {
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
    }
    if (level && ((n % 3) != 0))
        printf ("\n");
    printf ("%d of %d semaphores used.\n", n, _Semaphore_Information.maximum);
}

static const ioccrfArg netStatArg0 = { "level",ioccrfArgInt};
static const ioccrfArg *netStatArgs[1] = {&netStatArg0};
static const ioccrfFuncDef netStatFuncDef = {"netstat",1,netStatArgs};
static void netStatCallFunc(const ioccrfArgBuf *args)
{
    rtems_netstat(args[0].ival);
}
static const ioccrfArg semStatArg0 = { "level",ioccrfArgInt};
static const ioccrfArg *semStatArgs[1] = {&semStatArg0};
static const ioccrfFuncDef semStatFuncDef = {"semstat",1,semStatArgs};
static void semStatCallFunc(const ioccrfArgBuf *args)
{
    rtems_semstat(args[0].ival);
}
static const ioccrfFuncDef stackCheckFuncDef = {"stackCheck",0,NULL};
static void stackCheckCallFunc(const ioccrfArgBuf *args)
{
    Stack_check_Dump_usage ();
}
static void ioccrfRegisterRTEMS (void)
{
    ioccrfRegister(&netStatFuncDef, netStatCallFunc);
    ioccrfRegister(&semStatFuncDef, semStatCallFunc);
    ioccrfRegister(&stackCheckFuncDef, stackCheckCallFunc);
}

/*
 * Since RTEMS doesn't have NFS we fake it by making sure that
 * all paths refer to files in the TFTP area.
 */
#ifdef fopen
# undef fopen
  extern FILE *fopen (const char *, const char *);
#endif
FILE *rtems_fopen (const char *name, const char *mode)
{
    FILE *fp;
    char *cp = rtems_tftp_path (name);

    fp = fopen (cp, mode);
    free (cp);
    return fp;
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
    /*
     * Create a reasonable environment
     */
    initConsole ();
    putenv ("TERM=xterm");
    putenv ("IOCSH_PS1=rtems> ");
    putenv ("IOCSH_HISTSIZE=20");

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
     * Do EPICS initialization
     */
    threadInit ();

    /*
     * Run the EPICS startup script
     */
    printf ("***** Executing EPICS startup script *****\n");
    ioccrfRegisterRTEMS ();
    ioccrf ("st.cmd");

    /*
     * Everything's running!
     */
    threadSleep (2.0);
    ioccrf (NULL);
    LogFatal ("Console command interpreter terminated");
}
