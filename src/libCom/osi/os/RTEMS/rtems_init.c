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
#include <ioccrf.h>
#include <ioccrfRegisterCommon.h>
#include <registerRecordDeviceDriverRegister.h>
#include <dbStaticLib.h>

/*
 ***********************************************************************
 *                         RTEMS CONFIGURATION                         *
 ***********************************************************************
 */
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_EXECUTIVE_RAM_SIZE        (2000*1024)
#define CONFIGURE_MAXIMUM_TASKS             rtems_resource_unlimited(30)
#define CONFIGURE_MAXIMUM_SEMAPHORES        rtems_resource_unlimited(500)
#define CONFIGURE_MAXIMUM_TIMERS            rtems_resource_unlimited(20)
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES    rtems_resource_unlimited(5)

#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 30
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#define CONFIGURE_MICROSECONDS_PER_TICK     20000

#define CONFIGURE_INIT_TASK_PRIORITY    80
#define NETWORK_TASK_PRIORITY           90

#define CONFIGURE_INIT
#define CONFIGURE_INIT_TASK_INITIAL_MODES (RTEMS_PREEMPT | \
                    RTEMS_NO_TIMESLICE | \
                    RTEMS_NO_ASR | \
                    RTEMS_INTERRUPT_LEVEL(0))
#define CONFIGURE_INIT_TASK_ATTRIBUTES (RTEMS_FLOATING_POINT | RTEMS_LOCAL)
#define CONFIGURE_INIT_TASK_STACK_SIZE  (16*1024)
rtems_task Init (rtems_task_argument argument);

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#include <confdefs.h>

/*
 * Network configuration
 */
extern void rtems_bsdnet_loopattach();
static struct rtems_bsdnet_ifconfig loopback_config = {
    "lo0",                          /* name */
    (int (*)(struct rtems_bsdnet_ifconfig *))rtems_bsdnet_loopattach, /* attach function */
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
    &netdriver_config,        /* Network interface */
    rtems_bsdnet_do_bootp,    /* Use BOOTP to get network configuration */
    NETWORK_TASK_PRIORITY,    /* Network task priority */
    180*1024,                 /* MBUF space */
    350*1024,                 /* MBUF cluster space */
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

static ioccrfArg netStatArg0 = { "level",ioccrfArgInt,0};
static ioccrfArg *netStatArgs[1] = {&netStatArg0};
static ioccrfFuncDef netStatFuncDef = {"netstat",1,netStatArgs};
static void netStatCallFunc(ioccrfArg **args)
{
    rtems_netstat(*(int *)args[0]->value);
}
static ioccrfArg semStatArg0 = { "level",ioccrfArgInt,0};
static ioccrfArg *semStatArgs[1] = {&semStatArg0};
static ioccrfFuncDef semStatFuncDef = {"semstat",1,semStatArgs};
static void semStatCallFunc(ioccrfArg **args)
{
    rtems_semstat(*(int *)args[0]->value);
}
static void ioccrfRegisterRTEMS (void)
{
    ioccrfRegister(&netStatFuncDef, netStatCallFunc);
    ioccrfRegister(&semStatFuncDef, semStatCallFunc);
}

/*
 * Wrappers for EPICS routines which refer to file names.
 * Since RTEMS doesn't have NFS we fake it by making sure that
 * all paths refer to files in the TFTP area.
 */
long
dbLoadDatabaseRTEMS (char *name)
{
    char *cp = rtems_tftp_path (name);
    int dbLoadDatabase (char *filename, char *path, char *substitutions);

    dbLoadDatabase (cp, "/", NULL);
    free (cp);
    return 0;
}

long
dbLoadRecordsRTEMS (char *name, char *substitutions)
{
    char *cp = rtems_tftp_path (name);
    int dbLoadRecords (char* pfilename, char* substitutions);

    dbLoadRecords (cp, substitutions);
    free (cp);
    return 0;
}

void
runScriptRTEMS (const char *name)
{
    char *cp;

    cp = rtems_tftp_path (name);
    ioccrf (cp);
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
    ioccrfRegisterCommon ();
    ioccrfRegisterRTEMS ();
    registerRecordDeviceDriverRegister ();
    runScriptRTEMS ("st.cmd");

    /*
     * Everything's running!
     */
    threadSleep (2.0);
    ioccrf (NULL);
    LogFatal ("Console command interpreter terminated");
}
