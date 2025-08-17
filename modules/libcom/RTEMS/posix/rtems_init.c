/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * RTEMS startup task for EPICS
 *      Author: W. Eric Norum
 *              eric.norum@usask.ca
 *              (306) 966-5394
 */

/*
 * This is the first implementation with posix and rtems5.
 *  Currently only dhcp is supported with libbsd. The reading of the
 *  environment variables from e.g. u-boot environment is still missing.
 * Extensive tests so far only with qemu.
 */

/* trigger sys/syslog.h to emit prioritynames[] */
#define SYSLOG_NAMES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <rtems.h>
#include <rtems/malloc.h>
#include <rtems/error.h>
#include <rtems/stackchk.h>
#include <rtems/imfs.h>
#include <rtems/shell.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <rtems/libio.h>
#include <rtems/rtc.h>
#include <time.h>
#include <sys/unistd.h>

#if __RTEMS_MAJOR__ > 4
#include <rtems/tod.h>
#else
#include <rtems/score/tod.h>
#endif

#ifdef RTEMS_LEGACY_STACK
#include <rtems/rtems_bsdnet.h>
#include <librtemsNfs.h>
#else // libbsd stack
#include <rtems/bsd/bsd.h>
#include <rtems/bsd/modules.h>
#include <rtems/dhcpcd.h>
#include <machine/rtems-bsd-commands.h>
#endif

#include <rtems/telnetd.h>
#if __RTEMS_MAJOR__ > 4
#include <rtems/printer.h>
#endif
#include "epicsVersion.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "epicsExit.h"
#include "envDefs.h"
#include "errlog.h"
#include "logClient.h"
#include "osiUnistd.h"
#include "iocsh.h"
#include "osdTime.h"
#include "epicsMemFs.h"
#include "epicsEvent.h"
#include "errlog.h"

#include "epicsRtemsInitHooks.h"

#ifndef RTEMS_LEGACY_STACK
// setup own ntp-client to get time
#include "epicsNtp.h"
epicsEventId 	dhcpDone;
#endif

/* these settings are needed by the rtems startup
 * may provide by dhcp/bootp
 * or environments from the "BIOS" like u-boot, motboot etc.
 */
char rtemsInit_NTP_server_ip[16] = "";
char bootp_server_name_init[128] = "1001.1001@10.0.5.1:/epics";
char bootp_boot_file_name_init[128] = "/epics/myExample/bin/RTEMS-beatnik/myExample.boot";
char bootp_cmdline_init[128] = "/epics/myExample/iocBoot/iocmyExample/st.cmd";

/* TODO check rtems_bsdnet_bootp_cmdline */
#ifndef RTEMS_LEGACY_STACK
struct in_addr rtems_bsdnet_bootp_server_address;
char *rtems_bsdnet_bootp_server_name = bootp_server_name_init;
char *rtems_bsdnet_bootp_boot_file_name = bootp_boot_file_name_init;
char *rtems_bsdnet_bootp_cmdline = bootp_cmdline_init;
#endif // not LEGACY Stack

/*
 * Prototypes for some functions not in header files
 */
//void tzset(void);
int fileno(FILE *);
int main(int argc, char **argv);

static void
logReset (void)
{
    void rtems_bsp_reset_cause(char *buf, size_t capacity) __attribute__((weak));
    void (*fp)(char *buf, size_t capacity) = rtems_bsp_reset_cause;

    if (fp) {
        char buf[80];
        fp(buf, sizeof buf);
        errlogPrintf ("Startup after %s.\n", buf);
    }
    else {
        errlogPrintf ("Startup.\n");
    }
}

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
    rtems_task_wake_after (rtems_clock_get_ticks_per_second());
    rtems_task_wake_after (rtems_clock_get_ticks_per_second());
    rtems_panic ("%s", msg);
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

void *
mustMalloc(int size, const char *msg)
{
    void *p;

    if ((p = malloc (size)) == NULL)
        LogFatal ("Can't allocate space for %s.\n", msg);
    return p;
}

/*
 ***********************************************************************
 *                         REMOTE FILE ACCESS                          *
 ***********************************************************************
 */
#ifdef OMIT_NFS_SUPPORT
# include <rtems/tftp.h>
#endif

const epicsMemFS *epicsRtemsFSImage __attribute__((weak));
const epicsMemFS *epicsRtemsFSImage = (void*)&epicsRtemsFSImage;

/* hook to allow app specific FS setup */
int
epicsRtemsMountLocalFilesystem(char **argv) __attribute__((weak));
int
epicsRtemsMountLocalFilesystem(char **argv)
{
    if(epicsRtemsFSImage==(void*)&epicsRtemsFSImage)
        return -1; /* no FS image provided. */
    else if(epicsRtemsFSImage==NULL)
        return 0; /* no FS image provided, but none is needed. */
    else {
        printf("***** Using compiled in file data *****\n");
        if (epicsMemFsLoad(epicsRtemsFSImage) != 0) {
            printf("Can't unpack tar filesystem\n");
            return -1;
        } else {
            argv[1] = "/";
            return 0;
        }
    }
}

static int
initialize_local_filesystem(char **argv)
{
    extern char _DownloadLocation[] __attribute__((weak));
    extern char _FlashBase[] __attribute__((weak));
    extern char _FlashSize[]  __attribute__((weak));

    argv[0] = rtems_bsdnet_bootp_boot_file_name;
    if (epicsRtemsMountLocalFilesystem(argv)==0) {
        return 1; /* FS setup successful */
#ifdef RTEMS_LEGACY_STACK
    } else if (_FlashSize && (_DownloadLocation || _FlashBase)) {
        extern char _edata[];
        size_t flashIndex = _edata - _DownloadLocation;
        char *header = _FlashBase + flashIndex;

        if (memcmp(header + 257, "ustar  ", 8) == 0) {
            int fd;
            printf ("***** Unpack in-memory file system (IMFS) *****\n");
            if (rtems_tarfs_load("/", (unsigned char *)header, (size_t)_FlashSize - flashIndex) != 0) {
                printf("Can't unpack tar filesystem\n");
                return 0;
            }
            if ((fd = open(rtems_bsdnet_bootp_cmdline, 0)) >= 0) {
                close(fd);
                printf ("***** Found startup script (%s) in IMFS *****\n", rtems_bsdnet_bootp_cmdline);
                argv[1] = rtems_bsdnet_bootp_cmdline;
                return 1;
            }
            printf ("***** Startup script (%s) not in IMFS *****\n", rtems_bsdnet_bootp_cmdline);
        }
#endif /* only with old stack, check check libbsd dependency! */
    }
    return 0;
}

#ifndef OMIT_NFS_SUPPORT
int
nfsMount(char *uidhost, char *path, char *mntpoint)
{
    int   devl = strlen(uidhost) + strlen(path) + 2;
    char *dev;
    int   rval = -1;

    if ((dev = malloc(devl)) == NULL) {
        fprintf(stderr,"nfsMount: out of memory\n");
        return -1;
    }
    sprintf(dev, "%s:%s", uidhost, path);
    printf("Mount %s on %s\n", dev, mntpoint);
    rval = mount_and_make_target_path (
        dev, mntpoint, RTEMS_FILESYSTEM_TYPE_NFS,
        RTEMS_FILESYSTEM_READ_WRITE, NULL );
   if(rval)
      perror("mount failed");
    free(dev);
    return rval;
}
#define NFS_INIT
#endif


static void
initialize_remote_filesystem(char **argv, int hasLocalFilesystem)
{
#ifdef OMIT_NFS_SUPPORT
    printf ("***** Initializing TFTP *****\n");
    mount_and_make_target_path(NULL,
                               "/TFTP",
                               RTEMS_FILESYSTEM_TYPE_TFTPFS,
                               RTEMS_FILESYSTEM_READ_WRITE,
                               NULL);
    if (!hasLocalFilesystem) {
        char *path;
        int pathsize = 200;
        int l;

        path = mustMalloc(pathsize, "Command path name ");
        strcpy (path, "/TFTP/BOOTP_HOST/epics/");
        l = strlen (path);
        if (gethostname (&path[l], pathsize - l - 10) || (path[l] == '\0'))
        {
            LogFatal ("Can't get host name");
        }
        strcat (path, "/st.cmd");
        argv[1] = path;
    }
#else
    char *server_name;
    char *server_path;
    char *mount_point;
    char *cp;
    int l = 0;

    printf ("***** Initializing NFS *****\n");
    NFS_INIT
    if (env_nfsServer && env_nfsPath && env_nfsMountPoint) {
        server_name = env_nfsServer;
        server_path = env_nfsPath;
        mount_point = env_nfsMountPoint;
        cp = mount_point;
        while ((cp = strchr(cp+1, '/')) != NULL) {
            *cp = '\0';
            if ((mkdir (mount_point, 0755) != 0)
             && (errno != EEXIST))
                LogFatal("Can't create directory \"%s\": %s.\n",
                                                mount_point, strerror(errno));
            *cp = '/';
        }
        argv[1] = rtems_bsdnet_bootp_cmdline;
    }
    else if (hasLocalFilesystem) {
        return;
    }
    else {
        /*
         * Use first component of nvram/bootp command line pathname
         * to set up initial NFS mount.  A "/tftpboot/" is prepended
         * if the pathname does not begin with a '/'.  This allows
         * NFS and TFTP to have a similar view of the remote system.
         */
        if (rtems_bsdnet_bootp_cmdline[0] == '/')
            cp = rtems_bsdnet_bootp_cmdline + 1;
        else
            cp = rtems_bsdnet_bootp_cmdline;
        cp = strchr(cp, '/');
        if ((cp == NULL)
         || ((l = cp - rtems_bsdnet_bootp_cmdline) == 0))
            LogFatal("\"%s\" is not a valid command pathname.\n", rtems_bsdnet_bootp_cmdline);
        cp = mustMalloc(l + 20, "NFS mount paths");
        server_path = cp;
        server_name = rtems_bsdnet_bootp_server_name;
        if (rtems_bsdnet_bootp_cmdline[0] == '/') {
            mount_point = server_path;
            strncpy(mount_point, rtems_bsdnet_bootp_cmdline, l);
            mount_point[l] = '\0';
            argv[1] = rtems_bsdnet_bootp_cmdline;
            /*
             * Its probably common to embed the mount point in the server
             * name so, when this is occurring, don't clobber the mount point
             * by appending the first node from the command path. This allows
             * the mount point to be a different path then the server's mount
             * path.
             *
             * This allows for example a line similar to as follows the DHCP
             * configuration file.
             *
             * server-name "159.233@192.168.0.123:/vol/vol0/bootRTEMS";
             */
            if ( server_name ) {
                const size_t allocSize = strlen ( server_name ) + 2;
                char * const pServerName = mustMalloc( allocSize,
                                                        "NFS mount paths");
                char * const pServerPath = mustMalloc ( allocSize,
                                                        "NFS mount paths");
                const int scanfStatus = sscanf (
                                          server_name,
                                          "%[^:] : / %s",
                                          pServerName,
                                          pServerPath + 1u );
                if ( scanfStatus ==  2 ) {
                    pServerPath[0u]= '/';
                    server_name = pServerName;
                    // is this a general solution?
                    server_path = mount_point = pServerPath;
                }
                else {
                    free ( pServerName );
                    free ( pServerPath );
                }
            }
        }
        else {
            char *abspath = mustMalloc(strlen(rtems_bsdnet_bootp_cmdline)+2,"Absolute command path");
            strcpy(server_path, "/tftpboot/");
            mount_point = server_path + strlen(server_path);
            strncpy(mount_point, rtems_bsdnet_bootp_cmdline, l);
            mount_point[l] = '\0';
            mount_point--;
            strcpy(abspath, "/");
            strcat(abspath, rtems_bsdnet_bootp_cmdline);
            argv[1] = abspath;
        }
    }
    errlogPrintf("nfsMount(\"%s\", \"%s\", \"%s\")\n",
                 server_name, server_path, mount_point);
    nfsMount(server_name, server_path, mount_point);
#endif
}

static
char rtems_etc_hosts[] = "127.0.0.1       localhost\n";

/* If it doesn't already exist, create /etc/hosts with an entry for 'localhost' */
static
void fixup_hosts(void)
{
    FILE *fp;
    int ret;
    struct stat STAT;

    ret=stat("/etc/hosts", &STAT);
    if(ret==0)
    {
        return; /* already exists, assume file */
    } else if(errno!=ENOENT) {
        perror("error: fixup_hosts stat /etc/hosts");
        return;
    }

    ret = mkdir("/etc", 0775);
    if(ret!=0 && errno!=EEXIST)
    {
        perror("error: fixup_hosts create /etc");
        return;
    }

    if((fp=fopen("/etc/hosts", "w"))==NULL)
    {
        perror("error: fixup_hosts create /etc/hosts");
    }

    if(fwrite(rtems_etc_hosts, 1, sizeof(rtems_etc_hosts)-1, fp)!=sizeof(rtems_etc_hosts)-1)
    {
        perror("error: failed to write /etc/hosts");
    }

    fclose(fp);
}

/*
 * Get to the startup script directory
 * The TFTP filesystem requires a trailing '/' on chdir arguments.
 */
static void
set_directory (const char *commandline)
{
    const char *cp;
    char *directoryPath;
    int l;

    cp = strrchr(commandline, '/');
    if (cp == NULL) {
        l = 0;
        cp = "/";
    }
    else {
        l = cp - commandline;
        cp = commandline;
    }
    directoryPath = mustMalloc(l + 2, "Command path directory ");
    strncpy(directoryPath, cp, l);
    directoryPath[l] = '/';
    directoryPath[l+1] = '\0';
    if (chdir (directoryPath) < 0)
        LogFatal ("Can't set initial directory(%s): %s\n", directoryPath, strerror(errno));
    else
        errlogPrintf("chdir(\"%s\")\n", directoryPath);
    free(directoryPath);
}

/*
 ***********************************************************************
 *                         RTEMS/EPICS COMMANDS                        *
 ***********************************************************************
 */

static const iocshArg rtshellArg0 = { "cmd", iocshArgString};
static const iocshArg rtshellArg1 = { "args", iocshArgArgv};
static const iocshArg * rtshellArgs[2] = { &rtshellArg0, &rtshellArg1};
static const iocshFuncDef rtshellFuncDef = { "rt",2, rtshellArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                            , "run rtems shell command"
#endif
                                           };

static void rtshellCallFunc(const iocshArgBuf *args)
{
    rtems_shell_cmd_t *cmd = rtems_shell_lookup_cmd(args[0].sval);
    int ret;
    
    if (!cmd) {
        fprintf(stderr, "ERR: No such command\n");
        iocshSetError(-1);
    
    } else {
        fflush(stdout);
        fflush(stderr);
        ret = (*cmd->command)(args[1].aval.ac,args[1].aval.av);
        fflush(stdout);
        fflush(stderr);
        iocshSetError(ret);
        if(ret)
            fprintf(stderr, "ERR: %d\n",ret);
    }
}

/*
 * RTEMS status
 */
static void
rtems_netstat (unsigned int level)
{
#ifndef RTEMS_LEGACY_STACK
    printf ("***** Sorry not implemented yet with the new network stack (bsdlib)\n");
#else
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
#endif
}

static const iocshArg netStatArg0 = { "level",iocshArgInt};
static const iocshArg * const netStatArgs[1] = {&netStatArg0};
static const iocshFuncDef netStatFuncDef = {"netstat",1,netStatArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                            , "show network status"
#endif
                                           };
static void netStatCallFunc(const iocshArgBuf *args)
{
    rtems_netstat(args[0].ival);
}

static const iocshFuncDef heapSpaceFuncDef = {"heapSpace",0,NULL
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                              , "show malloc statistic"
#endif
                                             };
static void heapSpaceCallFunc(const iocshArgBuf *args)
{
#if __RTEMS_MAJOR__ > 4
    Heap_Information_block info;
    double x;

    malloc_info (&info);
    x = info.Stats.size - (unsigned long)
        (info.Stats.lifetime_allocated - info.Stats.lifetime_freed);
    if (x >= 1024*1024)
        printf("Heap space: %.1f MB\n", x / (1024 * 1024));
    else
        printf("Heap space: %.1f kB\n", x / 1024);
#else
    rtems_malloc_statistics_t s;
    double x;

    malloc_get_statistics(&s);
    x = s.space_available - (unsigned long)(s.lifetime_allocated - s.lifetime_freed);
    if (x >= 1024*1024)
        printf("Heap space: %.1f MB\n", x / (1024 * 1024));
    else
        printf("Heap space: %.1f kB\n", x / 1024);
#endif // RTEMS < 5
}

#ifndef OMIT_NFS_SUPPORT
static const iocshArg nfsMountArg0 = { "[uid.gid@]host",iocshArgString};
static const iocshArg nfsMountArg1 = { "server path",iocshArgString};
static const iocshArg nfsMountArg2 = { "mount point",iocshArgString};
static const iocshArg * const nfsMountArgs[3] = {&nfsMountArg0,&nfsMountArg1,
                                                 &nfsMountArg2};
static const iocshFuncDef nfsMountFuncDef = {"nfsMount",3,nfsMountArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                             , "mount nfs drive"
#endif
                                            };
static void nfsMountCallFunc(const iocshArgBuf *args)
{
    char *cp = args[2].sval;
    while ((cp = strchr(cp+1, '/')) != NULL) {
        *cp = '\0';
        if ((mkdir (args[2].sval, 0755) != 0) && (errno != EEXIST)) {
            printf("Can't create directory \"%s\": %s.\n",
                                            args[2].sval, strerror(errno));
            return;
        }
        *cp = '/';
    }
    iocshSetError(nfsMount(args[0].sval, args[1].sval, args[2].sval));
}
#endif


int zoneset(const char *zone)
{
    int ret;
    if(zone) {
        if ((ret = setenv("TZ", zone, 1)) < 0)
            return ret;
    }
    #if defined( __NEWLIB_MINOR__ ) /* Added in newlib 2.2.0 */
        else if ((ret = unsetenv("TZ")) < 0)
            return ret;
    #else
        else
            unsetenv("TZ");
    #endif
    tzset();
    return 0;
}

static const iocshArg zonesetArg0 = {"zone string", iocshArgString};
static const iocshArg * const zonesetArgs[1] = {&zonesetArg0};
static const iocshFuncDef zonesetFuncDef = {"zoneset",1,zonesetArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                           , "set timezone (obsolete?)"
#endif
                                           };
static void zonesetCallFunc(const iocshArgBuf *args)
{
    iocshSetError(zoneset(args[0].sval));
}

#ifndef RTEMS_LEGACY_STACK
static int bsd_log_to_erl(int level, const char *fmt, va_list ap)
{
    return errlogVprintf(fmt, ap);
}
#endif // RTEMS_LEGACY_STACK

static void setlogmaskCallFunc(const iocshArgBuf *args)
{
    const char* name = args[0].sval;
    const CODE* cur;
    if(!name) {
        printf("Usage: setlogmask <level>\n"
               "\n"
               "  Level names:\n");
        for(cur = prioritynames; cur->c_name; cur++) {
            printf("    %s\n", cur->c_name);
        }
    } else {
        for(cur = prioritynames; cur->c_name; cur++) {
            if(strcmp(name, cur->c_name)!=0)
                continue;

            (void)setlogmask(LOG_MASK(cur->c_val));
#ifndef RTEMS_LEGACY_STACK
            rtems_bsd_setlogpriority(name);
#endif
            return;
        }
        printf("Error: unknown log level.\n");
        iocshSetError(-1);
    }
}
static const iocshArg setlogmaskArg0 = {"level name", iocshArgString};
static const iocshArg * const setlogmaskArgs[1] = {&setlogmaskArg0};
static const iocshFuncDef setlogmaskFuncDef = {"setlogmask",1,setlogmaskArgs,
                                            "Set syslog() threshold level"};

/*
 * Register RTEMS-specific commands
 */
static void iocshRegisterRTEMS (void)
{
    iocshRegister(&netStatFuncDef, netStatCallFunc);
    iocshRegister(&heapSpaceFuncDef, heapSpaceCallFunc);
#ifndef OMIT_NFS_SUPPORT
    iocshRegister(&nfsMountFuncDef, nfsMountCallFunc);
#endif
    iocshRegister(&zonesetFuncDef, &zonesetCallFunc);
    iocshRegister(&rtshellFuncDef, &rtshellCallFunc);
    iocshRegister(&setlogmaskFuncDef, &setlogmaskCallFunc);
#if __RTEMS_MAJOR__ > 4
    rtems_shell_init_environment();
#endif
}

/*
 * Set up the console serial line (no handshaking)
 */
static void
initConsole (void)
{
    struct termios t;

printf("\n initConsole --- Info ---\n");
printf("stdin: fileno: %d, ttyname: %s\n", fileno(stdin), ttyname(fileno(stdin)));
printf("stdout: fileno: %d, ttyname: %s\n", fileno(stdout), ttyname(fileno(stdout)));
printf("stderr: fileno: %d, ttyname: %s\n", fileno(stderr), ttyname(fileno(stderr)));

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
 * Hook to ensure that BSP cleanup code gets run on exit
 */
static void
exitHandler(void)
{
    rtems_shutdown_executive(0);
}

#ifndef RTEMS_LEGACY_STACK
static void
dhcpcd_hook_handler(rtems_dhcpcd_hook *hook, char *const *env)
{
    int bound = 0;
    char interface[16] = "?";
    char reason[16] = "?";
    char new_host_name[32] = "RTEMShost";
    struct dhcp_vars_t {
        const char * const name;
        char* var;
        size_t varsize;
    } dhcp_vars[] = {
#define DEFVAR(N) { #N, N, sizeof(N) }
        DEFVAR(interface),
        DEFVAR(reason),
        DEFVAR(new_host_name),
#undef DEFVAR
#define DEFVAR(N, V) { N, V, sizeof(V) }
        DEFVAR("new_ntp_servers", rtemsInit_NTP_server_ip),
        DEFVAR("new_tftp_server_name", bootp_server_name_init),
        DEFVAR("new_bootfile_name", bootp_boot_file_name_init),
        DEFVAR("new_rtems_cmdline", bootp_cmdline_init),
#undef DEFVAR
        {NULL}
    };
    (void)hook;

    printf("\n");
    for (;NULL != *env;++env) {
        printf("dhcpcd ---> '%s'\n", *env);
        
        for(const struct dhcp_vars_t* var = dhcp_vars; var->name; var++) {
            size_t namelen = strlen(var->name);

            // assume "KEY=value" with no whitespace around '='
            if(strncmp(*env, var->name, namelen)!=0 || (*env)[namelen]!='=')
                continue;

            const char* value = *env + namelen+1;
            size_t valuelen = strlen(value);

            // require ordering of keys:
            // interface=
            // reason=
            // all others...
            if(var->var!=reason && var->var!=interface && !bound) {
                // ignore others if !BOUND

            } else if(valuelen >= var->varsize) {
                printf("  Not enough space for value string.  Expand buffer and recompile.\n");

            } else {
                memcpy(var->var, value, valuelen);
                var->var[valuelen] = '\0';
                printf("  '%s' = '%s'\n", var->name, var->var);

                if(var->var==reason && strcmp(reason, "BOUND")==0) {
                    printf("  is BOUND\n");
                    bound = 1;
                }
            }
            break;
        }
    }

    if(bound) {
        sethostname (new_host_name, strlen (new_host_name));
        epicsEventSignal(dhcpDone);
        printf("dhcpd BOUND\n");
    }
}

static rtems_dhcpcd_hook dhcpcd_hook = {
	.name = "ioc boot",
	.handler = dhcpcd_hook_handler
};

static void
default_network_on_exit(int exit_code, void *arg)
{
        rtems_printer printer;

        (void)arg;

        rtems_print_printer_printf(&printer);
        rtems_stack_checker_report_usage_with_plugin(&printer);
}

static void
default_network_set_self_prio(rtems_task_priority prio)
{
        rtems_status_code sc;

        sc = rtems_task_set_priority(RTEMS_SELF, prio, &prio);
        assert(sc == RTEMS_SUCCESSFUL);
}

static void
default_network_dhcpcd(void)
{
    static const char default_cfg[] = "clientid test client\n";
    rtems_status_code sc;
    int fd;
    int rv;
    ssize_t n;
    struct stat statbuf;

    if (ENOENT == stat("/etc/dhcpcd.conf", &statbuf)) {
        fd = open("/etc/dhcpcd.conf", O_CREAT | O_WRONLY,
                  S_IRWXU | S_IRWXG | S_IRWXO);
        assert(fd >= 0);

        n = write(fd, default_cfg, sizeof(default_cfg) - 1);
        assert(n == (ssize_t) sizeof(default_cfg) - 1);

        static const char fhi_cfg[] =
            "nodhcp6\n"
            "ipv4only\n"
            "option ntp-servers\n"
            "option rtems_cmdline\n"
            "option tftp-server-name\n"
            "option bootfile-name\n"
            "define 129 string rtems_cmdline\n"
            "timeout 0";

        n = write(fd, fhi_cfg, sizeof(fhi_cfg) - 1);
        assert(n == (ssize_t) sizeof(fhi_cfg) - 1);

        rv = close(fd);
        assert(rv == 0);
    }
    
    sc = rtems_dhcpcd_start(NULL);
    assert(sc == RTEMS_SUCCESSFUL);
}
#endif // not RTEMS_LEGACY_STACK

#if __RTEMS_MAJOR__>4
/*
 ***********************************************************************
 *                         TELNET DAEMON                               *
 ***********************************************************************
 */
#define LINE_SIZE 256
static void
telnet_pseudoIocsh(char *name, __attribute__((unused))void *arg)
{
  char line[LINE_SIZE];
  int fid[3], save_fid[3];

  printf("info:  pty dev name = %s\n", name);

  save_fid[1] = dup2(1,1);
  fid[1] = dup2( fileno(stdout), 1);
  if (fid[1] == -1 ) printf("Can't dup stdout\n");
  save_fid[2] = dup2(2,2);
  fid[2] = dup2( fileno(stderr), 2);
  if (fid[2] == -1 ) printf("Can't dup stderr\n");

  const char *prompt = "tIocSh> ";

  while (1) {
    fputs(prompt, stdout);
    fflush(stdout);
    /* telnet close not detected ??? tbd */
    if (fgets(line, LINE_SIZE, stdin) == NULL) {
      dup2(save_fid[1],1);
      dup2(save_fid[2],2);
      return;
    }
    if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = 0;
    if (!strncmp( line, "bye",3)) {
      printf( "%s", "Will end session\n");
      dup2(save_fid[1],1);
      dup2(save_fid[2],2);
      return;
     }
     iocshCmd(line);
   }
}

#define SHELL_ENTRY telnet_pseudoIocsh

/*
 *  Telnet daemon configuration
 * 0 or NULL for most fields in this struct indicate default values to RTEMS.
 */
rtems_telnetd_config_table rtems_telnetd_config = {
  .command = SHELL_ENTRY,
  .arg = NULL,
  .priority = 0,
  .stack_size = 0,
  .client_maximum = 0,
  .login_check = NULL,
  .keep_stdio = false
};

#endif


/*
 * RTEMS Startup task
 */
void *
POSIX_Init ( void *argument __attribute__((unused)))
{
    int                	result;
    char               	*argv[3]         = { NULL, NULL, NULL };
    rtems_status_code  	sc;
    struct timespec  	now;
    char timeBuff[100];

    initConsole ();

    /*
     * Use BSP-supplied time of day if available otherwise supply default time.
     * It is very likely that other time synchronization facilities in EPICS
     * will soon override this value.
     */
    /* check for RTC ... unfortunately seems to be missing with libbsd and qemu ?
    if (checkRealtime() >= 0) {
      setRealTimeToRTEMS();
    } else
    */
    {
      // set time to 14.4.2014
      now.tv_sec = 1397460606;
      now.tv_nsec = 0;
      sc = clock_settime(CLOCK_REALTIME, &now);
      if (sc < 0)
        printf ("***** Can't set time: %s\n", rtems_status_text (sc));
    }
    sc = clock_gettime( CLOCK_REALTIME, &now);
    if ( sc < 0) {
      printf ("***** Can't get time: %s\n", rtems_status_text (sc));
    } else {
      strftime(timeBuff, sizeof timeBuff, "%D %T", gmtime(&now.tv_sec));
      printf("time set to : %s.%09ld UTC\n", timeBuff, now.tv_nsec);
    }

    /*
     * Explain why we're here
     */
    logReset();

    /* TBD ...
     * Architecture-specific hooks
     */
#ifdef RTEMS_LEGACY_STACK
    if (epicsRtemsInitPreSetBootConfigFromNVRAM(&rtems_bsdnet_config) != 0)
        delayedPanic("epicsRtemsInitPreSetBootConfigFromNVRAM");
    if (rtems_bsdnet_config.bootp == NULL) {
        extern void setBootConfigFromNVRAM(void);
        setBootConfigFromNVRAM();
    }
    if (epicsRtemsInitPostSetBootConfigFromNVRAM(&rtems_bsdnet_config) != 0)
        delayedPanic("epicsRtemsInitPostSetBootConfigFromNVRAM");
#endif
    /*
     * Override RTEMS Posix configuration, it gets started with posix prio 2
     */
    epicsThreadSetPriority(epicsThreadGetIdSelf(), epicsThreadPriorityIocsh);

    /*
     * Create a reasonable environment
     */
    
    putenv ("TERM=xterm");
    putenv ("IOCSH_HISTSIZE=20");

    /*
     * Display some OS information
     */
    printf("\n***** RTEMS Version: %s *****\n",
        rtems_get_version_string());

#ifndef RTEMS_LEGACY_STACK
#if defined(QEMU_FIXUPS) && defined(__i386__)
    // glorious hack to stub out useless EEPROM check
    // which takes sooooo longggg w/ QEMU
    // Writes a 'ret' instruction to immediatly return to the caller
    extern void _bsd_e1000_validate_nvm_checksum(void);
    *(char*)&_bsd_e1000_validate_nvm_checksum = 0xc3;
#endif
    /*
     * Start network (libbsd)
     *
     * start qemu like this
     * qemu-system-i386 -m 64 -no-reboot -serial stdio -display none \
     * -net nic,model=e1000 -net user,restrict=yes \
     * -append "--video=off --console=/dev/com1" -kernel libComTestHarness
     */
    printf("\n***** Initializing network (libbsd, dhcpcd) *****\n");
    rtems_bsd_setlogpriority("debug");
    on_exit(default_network_on_exit, NULL);

    /* Let other tasks run to complete background work */
    default_network_set_self_prio(RTEMS_MAXIMUM_PRIORITY - 1U);

    sc = rtems_bsd_initialize();
    assert(sc == RTEMS_SUCCESSFUL);

    /* Let the callout timer allocate its resources */
    sc = rtems_task_wake_after(2);
    assert(sc == RTEMS_SUCCESSFUL);

    printf("\n***** ifconfig lo0 *****\n");
    rtems_bsd_ifconfig_lo0();

    printf("\n***** add dhcpcd hook *****\n");
    dhcpDone = epicsEventMustCreate(epicsEventEmpty);
    rtems_dhcpcd_add_hook(&dhcpcd_hook);

    printf("\n***** Start default network dhcpcd *****\n");
    // if MY_BOOTP???
    default_network_dhcpcd();

    /* this seems to be hard coded in the BSP -> Sebastian Huber ? */
    printf("\n--Info (hpj)-- bsd task prio IRQS: %d  -----\n", rtems_bsd_get_task_priority("IRQS"));
    printf("\n--Info (hpj)-- bsd task prio TIME: %d  -----\n", rtems_bsd_get_task_priority("TIME"));


    // wait for dhcp done ... should be if SYNCDHCP is used
    epicsEventWaitStatus stat;
    printf("\n ---- Waiting for DHCP ...\n");
    stat = epicsEventWaitWithTimeout(dhcpDone, 600); 
    if (stat == epicsEventOK)
    	epicsEventDestroy(dhcpDone);
    else if (stat == epicsEventWaitTimeout)
        printf("\n ---- DHCP timed out!\n");
    else
	printf("\n ---- dhcpDone Event Unknown state %d\n", stat);

    const char* ifconfg_args[] = {
        "ifconfig", NULL
    };
    const char* netstat_args[] = {
        "netstat", "-rn", NULL
    };

    printf("-------------- IFCONFIG -----------------\n");
    rtems_bsd_command_ifconfig(1, (char**) ifconfg_args);
    printf("-------------- NETSTAT ------------------\n");
    rtems_bsd_command_netstat(2, (char**) netstat_args);

    /* until now there is no NTP support in libbsd -> Sebastian Huber ... */
    printf("\n***** Until now no NTP support in RTEMS 5 with rtems-libbsd *****\n");
    printf("\n***** Ask ntp server once... *****\n");
    if (rtemsInit_NTP_server_ip[0]=='\0') {
      printf ("***** No NTP server ...\n");
    } else if (epicsNtpGetTime(rtemsInit_NTP_server_ip, &now) < 0) {
      printf ("***** Can't get time from ntp ...\n");
    } else {
      if (clock_settime(CLOCK_REALTIME, &now) < 0){
        printf ("***** Can't set time: %s\n", rtems_status_text (sc));
      } else {
        strftime(timeBuff, sizeof timeBuff, "%D %T", gmtime(&now.tv_sec));
        printf("time from ntp : %s.%09ld UTC\n", timeBuff, now.tv_nsec);
      }
    }
#else // Legacy stack, old network initialization
    char *cp;
    if ((cp = getenv("EPICS_TS_NTP_INET")) != NULL)
        rtems_bsdnet_config.ntp_server[0] = cp;

    int rtems_bsdnet_ntpserver_count = 1;
    struct in_addr rtems_bsdnet_ntpserver[rtems_bsdnet_ntpserver_count];
    memcpy(rtems_bsdnet_ntpserver, rtems_bsdnet_config.ntp_server, sizeof(struct in_addr));

    if (rtems_bsdnet_config.network_task_priority == 0)
    {
        unsigned int p;
        if (epicsThreadHighestPriorityLevelBelow(epicsThreadPriorityScanLow, &p)
                                            == epicsThreadBooleanStatusSuccess)
        {
            rtems_bsdnet_config.network_task_priority = p;
        }
    }
    printf("\n***** Initializing network (Legacy Stack)  *****\n");
    rtems_bsdnet_initialize_network();
    printf("\n***** Network Status  *****\n");
    rtems_netstat(3);
    rtems_bsdnet_synchronize_ntp (0, 0);
#endif // not RTEMS_LEGACY_STACK

    printf("\n***** Setting up file system *****\n");
    initialize_remote_filesystem(argv, initialize_local_filesystem(argv));
    fixup_hosts();

    /*
     * More environment: iocsh prompt and hostname
     */
    {
        char hostname[1024];
        gethostname(hostname, 1023);
        char *cp = mustMalloc(strlen(hostname)+3, "iocsh prompt");
        sprintf(cp, "%s> ", hostname);
        epicsEnvSet ("IOCSH_PS1", cp);
        epicsEnvSet("IOC_NAME", hostname);
    }

    if (getenv("") == NULL) {
        const char *tzp = envGetConfigParamPtr(&EPICS_TZ);
        if (tzp == NULL) {
            printf("Warning -- no timezone information available -- times will be displayed as GMT.\n");
        }

    }
    tzset();
    printf(" check for time registered , C++ initialization ...\n");
    //osdTimeRegister();
    /*/Volumes/Epics/myExample/bin/RTEMS-xilinx_zynq_a9_qemu
     * Run the EPICS startup script
     */
#if __RTEMS_MAJOR__>4
    // if telnetd is requested ...
   // printf(" Will try to start telnetd with prio %d ...\n", rtems_telnetd_config.priority);
   // result = rtems_telnetd_initialize();
   // printf (" telnetd initialized with result %d\n", result);
#endif

    printf ("***** Preparing EPICS application *****\n");
    iocshRegisterRTEMS ();
    set_directory (argv[1]);
    epicsEnvSet ("IOC_STARTUP_SCRIPT", argv[1]);
    atexit(exitHandler);
    errlogFlush();
    printf ("***** Starting EPICS application *****\n");


#ifndef RTEMS_LEGACY_STACK
    // switch OS to async logging
    rtems_bsd_set_vprintf_handler(bsd_log_to_erl);
#endif

#if 0
// Start an rtems shell before main, for debugging RTEMS system issues
    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4,
                     100, "/dev/console",
                     false, true,
                     NULL);
#endif

    result = main ((sizeof argv / sizeof argv[0]) - 1, argv);
    printf ("***** IOC application terminating *****\n");
    epicsThreadSleep(1.0);
    epicsExit(result);

#if defined(__rtems__)
    delayedPanic("will reset rtems ... end of POSIX_Init");
#endif
    exit(0);
}

#if defined(QEMU_FIXUPS)
/* Override some hooks (weak symbols)
 * if BSP defaults aren't configured for running tests.
 */


/* Ensure that stdio goes to serial (so it can be captured) */
#if defined(__i386__) && !USE_COM1_AS_CONSOLE
#include <uart.h>
#if __RTEMS_MAJOR__ > 4
#include <libchip/serial.h>
#endif

extern int BSPPrintkPort;
void bsp_predriver_hook(void)
{
#if __RTEMS_MAJOR__ > 4
    Console_Port_Minor = BSP_CONSOLE_PORT_COM1;
#else
    BSPConsolePort = BSP_CONSOLE_PORT_COM1;

#endif
    BSPPrintkPort = BSP_CONSOLE_PORT_COM1;
}
#endif

/* reboot immediately when done. */
#if defined(__i386__) && BSP_PRESS_KEY_FOR_RESET
void bsp_cleanup(void)
{
    void bsp_reset();
    bsp_reset();
}
#endif

#endif /* QEMU_FIXUPS */

int cexpdebug __attribute__((weak));
