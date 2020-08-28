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
#include <rtems/tod.h>

#ifdef RTEMS_LEGACY_STACK
#include <rtems/rtems_bsdnet.h>
#include <librtemsNfs.h>
#else // libbsd stack
#include <rtems/bsd/bsd.h>
#include <rtems/bsd/modules.h>
#include <rtems/dhcpcd.h>
#include "libbsd_netconfig.h"
#include <machine/rtems-bsd-commands.h>
#endif

#include <rtems/telnetd.h>
#include <rtems/printer.h>

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

#include "epicsRtemsInitHooks.h"

#ifdef RTEMS_LEGACY_STACK
#pragma message "RTEMS uses legacy stack"
#else
#pragma message "RTEMS uses libbsd stack"
// setup own ntp-client to get time
#include "epicsNtp.h"
epicsEventId 	dhcpDone;
#endif


/* these settings are needed by the rtems startup
 * may provide by dhcp/bootp
 * or environments from the "BIOS" like u-boot, motboot etc.
 */
struct in_addr rtems_bsdnet_bootp_server_address;
char rtemsInit_NTP_server_ip[16];
char bootp_server_name_init[128] = "1001.1001@141.14.128.9:/Volumes/Epics";
char *rtems_bsdnet_bootp_server_name = bootp_server_name_init;
char bootp_boot_file_name_init[128] = "/Volumes/Epics/myExample/bin/RTEMS-qoriq_e500/myExample.boot";
char *rtems_bsdnet_bootp_boot_file_name = bootp_boot_file_name_init;
char bootp_cmdline_init[128] = "/Volumes/Epics/myExample/iocBoot/iocmyExample/st.cmd";
char *rtems_bsdnet_bootp_cmdline = bootp_cmdline_init;

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
    }
/* else if (_FlashSize && (_DownloadLocation || _FlashBase)) {
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
    } */
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
printf( " rtems_bootp_server_name: %s\n",rtems_bsdnet_bootp_server_name);
        server_name = rtems_bsdnet_bootp_server_name;
        if (rtems_bsdnet_bootp_cmdline[0] == '/') {
            mount_point = server_path;
            strncpy(mount_point, rtems_bsdnet_bootp_cmdline, l);
            mount_point[l] = '\0';
            argv[1] = rtems_bsdnet_bootp_cmdline;
            /*
             * Its probably common to embed the mount point in the server
             * name so, when this is occurring, dont clobber the mount point
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
static const iocshFuncDef rtshellFuncDef = { "rt",2, rtshellArgs};
static void rtshellCallFunc(const iocshArgBuf *args)
{
    rtems_shell_cmd_t *cmd = rtems_shell_lookup_cmd(args[0].sval);
    int ret;
    
    if (!cmd) {
        fprintf(stderr, "ERR: No such command\n");
    
    } else {
        fflush(stdout);
        fflush(stderr);
        ret = (*cmd->command)(args[1].aval.ac,args[1].aval.av);
        fflush(stdout);
        fflush(stderr);
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
/*
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
*/
}

static const iocshArg netStatArg0 = { "level",iocshArgInt};
static const iocshArg * const netStatArgs[1] = {&netStatArg0};
static const iocshFuncDef netStatFuncDef = {"netstat",1,netStatArgs};
static void netStatCallFunc(const iocshArgBuf *args)
{
    rtems_netstat(args[0].ival);
}

static const iocshFuncDef heapSpaceFuncDef = {"heapSpace",0,NULL};
static void heapSpaceCallFunc(const iocshArgBuf *args)
{
    Heap_Information_block info;
    double x;

    malloc_info (&info);
    x = info.Stats.size - (unsigned long)
        (info.Stats.lifetime_allocated - info.Stats.lifetime_freed);
    if (x >= 1024*1024)
        printf("Heap space: %.1f MB\n", x / (1024 * 1024));
    else
        printf("Heap space: %.1f kB\n", x / 1024);
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
    nfsMount(args[0].sval, args[1].sval, args[2].sval);
}
#endif


void zoneset(const char *zone)
{
    if(zone)
        setenv("TZ", zone, 1);
    else
        unsetenv("TZ");
    tzset();
}

static const iocshArg zonesetArg0 = {"zone string", iocshArgString};
static const iocshArg * const zonesetArgs[1] = {&zonesetArg0};
static const iocshFuncDef zonesetFuncDef = {"zoneset",1,zonesetArgs};
static void zonesetCallFunc(const iocshArgBuf *args)
{
    zoneset(args[0].sval);
}


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
    rtems_shell_init_environment();
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

static char* getPrimaryNetworkInterface(void)
{
    // lookup available network interfaces
    char ifnamebuf[IF_NAMESIZE];
    char *ifname;
    // get primary network interface
    ifname = if_indextoname(1, &ifnamebuf[0]);
    if (ifname == NULL) return (NULL);
    printf("\n***** Primary Network interface : %s *****\n", ifname);
    return (ifname);
}

static void
dhcpcd_hook_handler(rtems_dhcpcd_hook *hook, char *const *env)
{
    int bound = 0;
    char iName[16];
    char *name;
    char *value;

	(void)hook;

    char ifnamebuf[IF_NAMESIZE];
    *ifnamebuf = getPrimaryNetworkInterface();

    while (*env != NULL) {
        name = strtok(*env,"=");
        value = strtok(NULL,"=");
        printf("all out ---> %s = %s\n", name, value);
        if (!strncmp(name, "interface", 9) && !strcmp(value,  ifnamebuf))
			 strncpy(iName, value, 16);
		if (!strncmp(name, "reason", 6) && !strncmp(value, "BOUND", 5)){
            printf ("Interface %s bounded\n", iName);
            bound = 1;
		}
		if (bound) {
            // as there is no ntp-support in rtems-libbsd, we call our own client
            if(!strncmp(name, "new_ntp_servers", 15))
                strcpy(rtemsInit_NTP_server_ip,value);
            if(!strncmp(name, "new_host_name", 13))
                sethostname (value, strlen (value));
            if(!strncmp(name, "new_tftp_server_name", 20)){
                //printf(" new_tftp_server_name : %s\n", value);
                strncpy(rtems_bsdnet_bootp_server_name,value, sizeof(bootp_server_name_init));
                printf(" rtems_bsdnet_bootp_server_name : %s\n", rtems_bsdnet_bootp_server_name);
            }
            if(!strncmp(name, "new_bootfile_name", 20)){
                //printf(" new_bootfile_name : %s\n", value);
                strncpy(rtems_bsdnet_bootp_boot_file_name,value, sizeof(bootp_boot_file_name_init));
                printf(" rtems_bsdnet_bootp_boot_file_name : %s\n", rtems_bsdnet_bootp_boot_file_name);
            }
            if(!strncmp(name, "new_rtems_cmdline", 20)){
                //printf(" new_rtems_cmdline : %s\n", value);
                strncpy(rtems_bsdnet_bootp_cmdline,value, sizeof(bootp_cmdline_init));
                printf(" rtems_bsdnet_bootp_cmdline : %s\n", rtems_bsdnet_bootp_cmdline);
            }
            // printf("---> %s = %s\n", name, value);
        }
		++env;
	}
    if (bound)
        epicsEventSignal(dhcpDone);
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

        if (exit_code == 0) {
                puts("*** END OF TEST ***");
        }
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
    static const char default_cfg[] = "clientid FHI test client\n";
    rtems_status_code sc;
    int fd;
    int rv;
    ssize_t n;

    fd = open("/etc/dhcpcd.conf", O_CREAT | O_WRONLY,
        S_IRWXU | S_IRWXG | S_IRWXO);
    assert(fd >= 0);

    n = write(fd, default_cfg, sizeof(default_cfg) - 1);
    assert(n == (ssize_t) sizeof(default_cfg) - 1);

    static const char fhi_cfg[] =
        "nodhcp6\n" \
        "ipv4only\n" \
        "option ntp_servers\n" \
        "option rtems_cmdline\n" \
        "option tftp_server_name\n" \
        "option bootfile_name";

    n = write(fd, fhi_cfg, sizeof(fhi_cfg) - 1);
    assert(n == (ssize_t) sizeof(fhi_cfg) - 1);

    rv = close(fd);
    assert(rv == 0);

    sc = rtems_dhcpcd_start(NULL);
    assert(sc == RTEMS_SUCCESSFUL);
}

/*
 * RTEMS Startup task
 */
void *
POSIX_Init (void *argument)
{
    int                	result;
    char               	*argv[3]         = { NULL, NULL, NULL };
 //   char               	*cp;
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
      if (clock_settime(CLOCK_REALTIME, &now) < 0)
        printf ("***** Can't set time: %s\n", rtems_status_text (sc));
    }
    if ( clock_gettime( CLOCK_REALTIME, &now) < 0) {
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
    if (epicsRtemsInitPreSetBootConfigFromNVRAM(&rtems_bsdnet_config) != 0)
        delayedPanic("epicsRtemsInitPreSetBootConfigFromNVRAM");
    if (rtems_bsdnet_config.bootp == NULL) {
        extern void setBootConfigFromNVRAM(void);
        setBootConfigFromNVRAM();
    }
    if (epicsRtemsInitPostSetBootConfigFromNVRAM(&rtems_bsdnet_config) != 0)
        delayedPanic("epicsRtemsInitPostSetBootConfigFromNVRAM");

     */

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
      

    printf("\n***** Initializing network (dhcp) *****\n");
    rtems_bsd_setlogpriority("debug");
    on_exit(default_network_on_exit, NULL);
    /* Let other tasks run to complete background work */
    default_network_set_self_prio(RTEMS_MAXIMUM_PRIORITY - 1U);

    /* supress all output from bsd network initialization
    rtems_bsd_vprintf_handler bsd_vprintf_handler_old;
    bsd_vprintf_handler_old = rtems_bsd_set_vprintf_handler(rtems_bsd_vprintf_handler_mute);
    */

    sc = rtems_bsd_initialize();
    assert(sc == RTEMS_SUCCESSFUL);

    /* Let the callout timer allocate its resources */
    sc = rtems_task_wake_after(2);
    assert(sc == RTEMS_SUCCESSFUL);

    rtems_bsd_ifconfig_lo0();

    // if MY_BOOTP???
    default_network_dhcpcd();

    /* this seems to be hard coded in the BSP -> Sebastian Huber ?
    printf("\n--Info (hpj)-- bsd task prio IRQS: %d  -----\n", rtems_bsd_get_task_priority("IRQS"));
    printf("\n--Info (hpj)-- bsd task prio TIME: %d  -----\n", rtems_bsd_get_task_priority("TIME"));
    */

    // implement DHCP hook  ... and wait for acknowledge
    dhcpDone = epicsEventMustCreate(epicsEventEmpty);
    rtems_dhcpcd_add_hook(&dhcpcd_hook);

    /* use /etc/rc.conf, /etc/dhclient.conf ... */
    //epicsRtemsPrepareAndRunRCConfFile();

    // wait for dhcp done ... should be if SYNCDHCP is used
    epicsEventWaitStatus stat;
    int counter = 10;
    do {
	printf("\n ---- Wait for DHCP done ...\n");
        stat = epicsEventWaitWithTimeout(dhcpDone, 5.0); 
    } while ((stat == epicsEventWaitTimeout) && (--counter > 0)); 
    if (stat == epicsEventOK)
    	epicsEventDestroy(dhcpDone);
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
    if (epicsNtpGetTime(rtemsInit_NTP_server_ip, &now) < 0) {
      printf ("***** Can't get time from ntp ...\n");
    } else {
      if (clock_settime(CLOCK_REALTIME, &now) < 0){
        printf ("***** Can't set time: %s\n", rtems_status_text (sc));
      } else {
        strftime(timeBuff, sizeof timeBuff, "%D %T", gmtime(&now.tv_sec));
        printf("time from ntp : %s.%09ld UTC\n", timeBuff, now.tv_nsec);
      }
    }

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
    printf ("***** Preparing EPICS application *****\n");
    iocshRegisterRTEMS ();
    set_directory (argv[1]);
    epicsEnvSet ("IOC_STARTUP_SCRIPT", argv[1]);
    atexit(exitHandler);
    errlogFlush();
    printf ("***** Starting EPICS application *****\n");
    result = main ((sizeof argv / sizeof argv[0]) - 1, argv);
    printf ("***** IOC application terminating *****\n");
    epicsThreadSleep(1.0);
    epicsExit(result);
    return NULL;
}

#if defined(QEMU_FIXUPS)
/* Override some hooks (weak symbols)
 * if BSP defaults aren't configured for running tests.
 */


/* Ensure that stdio goes to serial (so it can be captured) */
#if defined(__i386__) && !USE_COM1_AS_CONSOLE
#include <uart.h>
#include <libchip/serial.h>

extern int BSPPrintkPort;
void bsp_predriver_hook(void)
{
    Console_Port_Minor = BSP_CONSOLE_PORT_COM1;
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

/* defines in rtems_config.c
#define CONFIGURE_SHELL_COMMANDS_INIT
#define CONFIGURE_SHELL_COMMANDS_ALL
// exclude commands which won't work right with our configuration
#define CONFIGURE_SHELL_NO_COMMAND_EXIT
#define CONFIGURE_SHELL_NO_COMMAND_LOGOFF
// exclude commands which unnecessarily pull in block driver
#define CONFIGURE_SHELL_NO_COMMAND_MSDOSFMT
#define CONFIGURE_SHELL_NO_COMMAND_BLKSYNC
#define CONFIGURE_SHELL_NO_COMMAND_MKRFS
#define CONFIGURE_SHELL_NO_COMMAND_DEBUGRFS
#define CONFIGURE_SHELL_NO_COMMAND_FDISK

#include <rtems/shellconfig.h>
*/
