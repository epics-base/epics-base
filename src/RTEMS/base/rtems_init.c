/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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
#include <errno.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rtems.h>
#include <rtems/error.h>
#include <rtems/stackchk.h>
#include <rtems/rtems_bsdnet.h>

#include <epicsThread.h>
#include <errlog.h>
#include <logClient.h>
#include <osiUnistd.h>
#include <iocsh.h>

#ifdef OMIT_NFS_SUPPORT
# include <rtems/tftp.h>
#else
# ifndef RTEMS_EPICS_NFS_SERVER_PATH
#  define RTEMS_EPICS_NFS_SERVER_PATH "/tftpboot/epics"
# endif
# ifndef RTEMS_EPICS_NFS_MOUNT_POINT
#  define RTEMS_EPICS_NFS_MOUNT_POINT "/epics"
# endif
#endif

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
    errlogPrintf ("Startup after %s.\n", cbuf);
    m360.rsr = ~0;
}

#else

static void
logReset (void)
{
    errlogPrintf ("Started.\n");
}
#endif

#ifdef __i386__
/*
 * Remote debugger support
 *
 * i386-rtems-gdb -b 38400 example(binary from EPICS build -- not netbootable image!)
 * (gdb) target remote /dev/ttyS0
 */
int enableRemoteDebugging = 0;  /* Global so gdb can set before download */
static void
initRemoteGdb(int ticksPerSecond)
{
    if (enableRemoteDebugging) {
        init_remote_gdb();
        rtems_task_wake_after(ticksPerSecond);
        breakpoint();
    }
}
#endif

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
    errlogVprintf (msg, ap);
    va_end (ap);
    delayedPanic (msg);
}

/*
 * Log RTEMS error and terminate
 */
void
LogRtemsFatal (const char *msg, rtems_status_code sc)
{
    errlogPrintf ("%s: %s\n", msg, rtems_status_text (sc));
    delayedPanic (msg);
}

/*
 * Log network error and terminate
 */
void
LogNetFatal (const char *msg, int err)
{
    errlogPrintf ("%s: %d\n", msg, err);
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
#ifdef OMIT_NFS_SUPPORT
    strcpy (path, "/TFTP/BOOTP_HOST/tftpboot/epics/");
    l = strlen (path);
    if (gethostname (&path[l], pathsize - l - 2) || (path[l] == '\0'))
        LogFatal ("Can't get host name");
    strcat (path, "/");
#else
    {
    char *cp = strrchr(rtems_bsdnet_bootp_cmdline, '/');
    if (cp == NULL)
        l = strlen(rtems_bsdnet_bootp_cmdline);
    else
        l = cp - rtems_bsdnet_bootp_cmdline;
    if (l >= pathsize)
        LogFatal ("Boot command path too long");
    strncpy(path, rtems_bsdnet_bootp_cmdline, l);
    path[l] = '\0';
    }
#endif
    if (chdir (path) < 0)
        LogFatal ("Can't set initial directory");
    free(path);
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

#ifndef OMIT_NFS_SUPPORT
static const iocshArg nfsMountArg0 = { "[uid.gid@]host",iocshArgString};
static const iocshArg nfsMountArg1 = { "server path",iocshArgString};
static const iocshArg nfsMountArg2 = { "mount point",iocshArgString};
static const iocshArg * const nfsMountArgs[3] = {&nfsMountArg0,&nfsMountArg1,
                                                 &nfsMountArg2};
static const iocshFuncDef nfsMountFuncDef = {"nfsMount",3,nfsMountArgs};
static void nfsMountCallFunc(const iocshArgBuf *args)
{
    nfsMount(args[0].sval, args[1].sval, args[2].sval);
}
#endif

/*
 * Register RTEMS-specific commands
 */
static void iocshRegisterRTEMS (void)
{
    iocshRegister(&netStatFuncDef, netStatCallFunc);
    iocshRegister(&stackCheckFuncDef, stackCheckCallFunc);
#ifndef OMIT_NFS_SUPPORT
    iocshRegister(&nfsMountFuncDef, nfsMountCallFunc);
#endif
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
    rtems_interval ticksPerSecond;
    rtems_task_priority newpri;
    rtems_status_code sc;
    rtems_time_of_day now;

    /*
     * Get configuration
     */
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &ticksPerSecond);

    /*
     * Architecture-specific hooks
     */
#if defined(__i386__)
    initRemoteGdb(ticksPerSecond);
#endif
#if defined(__PPC) && defined(HAVE_PPCBUG)
    {
    extern void setBootConfigFromPPCBUGNVRAM(void);
    setBootConfigFromPPCBUGNVRAM();
    argv[0] = rtems_bsdnet_bootp_boot_file_name;
    argv[1] = rtems_bsdnet_bootp_cmdline ;
    }
#endif

    /*
     * Override RTEMS configuration
     */
    rtems_task_set_priority (
                     RTEMS_SELF,
                     epicsThreadGetOssPriorityValue(epicsThreadPriorityIocsh),
                     &newpri);

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
    if (rtems_bsdnet_config.network_task_priority == 0) {
        unsigned int p;
        if (epicsThreadHighestPriorityLevelBelow(epicsThreadPriorityScanLow, &p)
                                            == epicsThreadBooleanStatusSuccess)
            rtems_bsdnet_config.network_task_priority = epicsThreadGetOssPriorityValue(p);
    }
    printf ("***** Initializing network *****\n");
    rtems_bsdnet_initialize_network ();
#ifdef OMIT_NFS_SUPPORT
    printf ("***** Initializing TFTP *****\n");
    rtems_bsdnet_initialize_tftp_filesystem ();
#else
    printf ("***** Initializing NFS *****\n");
    rpcUdpInit();
    nfsInit(0,0);
    nfsMount(rtems_bsdnet_bootp_server_name,
                                            RTEMS_EPICS_NFS_SERVER_PATH,
                                            RTEMS_EPICS_NFS_MOUNT_POINT);
#endif

    /*
     * Use BSP-supplied time of day if available
     */
    if (rtems_clock_get(RTEMS_CLOCK_GET_TOD,&now) != RTEMS_SUCCESSFUL) {
        for (i = 0 ; ; i++) {
            printf ("***** Initializing NTP *****\n");
            if (rtems_bsdnet_synchronize_ntp (0, 0) >= 0)
                break;
            rtems_task_wake_after (5*ticksPerSecond);
            if (i >= 12) {
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
    }

    /*
     * Run the EPICS startup script
     */
    printf ("***** Starting EPICS application *****\n");
    iocshRegisterRTEMS ();
    rtems_set_directory ();
    i = main ((sizeof argv / sizeof argv[0]) - 1, argv);
    printf ("***** IOC application terminating *****\n");
    exit (i);
}
