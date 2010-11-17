/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <rtems/rtems_bsdnet.h>
#include <bsp.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <epicsStdlib.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <envDefs.h>

char *env_nfsServer;
char *env_nfsPath;
char *env_nfsMountPoint;

/*
 * Split argument string of form nfs_server:nfs_export:<path>
 * The nfs_export component will be used as:
 *      - the path to the directory exported from the NFS server
 *      - the local mount point
 *      - a prefix of <path>
 * For example, the argument string:
 *       romeo:/export/users:smith/ioc/iocexample/st.cmd
 * would:
 *       - mount /export/users from NFS server romeo on /export/users
 *       - chdir to /export/users/smith/ioc/iocexample
 *       - read commands from st.cmd
 */
static void
splitRtemsBsdnetBootpCmdline(void)
{
    char *cp1, *cp2, *cp3;

    if ((cp1 = rtems_bsdnet_bootp_cmdline) == NULL)
        return;
    if (((cp2 = strchr(cp1, ':')) != NULL)
     && (((cp3 = strchr(cp2+1, ' ')) != NULL)
      || ((cp3 = strchr(cp2+1, ':')) != NULL))) {
        int l1 = cp2 - cp1;
        int l2 = cp3 - cp2 - 1;
        int l3 = strlen(cp3) - 1;
        if (l1 && l2 && l3) {
            *cp2++ = '\0';
            *cp3 = '\0';
            env_nfsServer = cp1;
            env_nfsMountPoint = env_nfsPath = epicsStrDup(cp2);
            *cp3 = '/';
            rtems_bsdnet_bootp_cmdline = cp2;
        }
    }
}

/*
 * Split NFS mount information of the form nfs_server:host_path:local_path
 */
static void
splitNfsMountPath(char *nfsString)
{
    char *cp2, *cp3;

    if (nfsString == NULL)
        return;
    if (((cp2 = strchr(nfsString, ':')) != NULL)
     && (((cp3 = strchr(cp2+1, ' ')) != NULL)
      || ((cp3 = strchr(cp2+1, ':')) != NULL))) {
        int l1 = cp2 - nfsString;
        int l2 = cp3 - cp2 - 1;
        int l3 = strlen(cp3) - 1;
        if (l1 && l2 && l3) {
            *cp2++ = '\0';
            *cp3++ = '\0';
            env_nfsServer = nfsString;
            env_nfsPath = cp2;
            env_nfsMountPoint = cp3;
        }
    }
}

#if defined(HAVE_MOTLOAD)

/*
 * Motorola MOTLOAD NVRAM Access
 */
static char *
gev(const char *parm, volatile char *nvp)
{
    const char *val;
    const char *name;
    char *ret;
    char c;

    for (;;) {
        if (*nvp == '\0')
            return NULL;
        name = parm;
        while ((c = *nvp++) != '\0') {
            if ((c == '=') && (*name == '\0')) {
                val = (char *)nvp;
                while (*nvp++ != '\0')
                    continue;
                ret = malloc(nvp - val);
                if (ret == NULL)
                    return NULL;
                strcpy(ret, val);
                return ret;
            }
            if (c != *name++) {
                while (*nvp++ != '\0')
                    continue;
                break;
            }
        }
    }
}

static char *
motScriptParm(const char *mot_script_boot, char parm)
{
    const char *cp;
    char *ret;
    int l;

    while (*mot_script_boot != '\0') {
        if (isspace(*(unsigned char *)mot_script_boot)
         && (*(mot_script_boot+1) == '-')
         && (*(mot_script_boot+2) == parm)) {
            mot_script_boot += 3;
            cp = mot_script_boot;
            while ((*mot_script_boot != '\0') &&
                   !isspace(*(unsigned char *)mot_script_boot))
                mot_script_boot++;
            l = mot_script_boot - cp;
            ret = malloc(l+1);
            if (ret == NULL)
                return NULL;
            strncpy(ret, cp, l);
            *(ret+l) = '\0';
            return ret;
         }
        mot_script_boot++;
    }
    return NULL;
}

void
setBootConfigFromNVRAM(void)
{
    char *cp;
    const char *mot_script_boot;
    char *nvp;

# if defined(BSP_NVRAM_BASE_ADDR)
    nvp = (volatile unsigned char *)(BSP_NVRAM_BASE_ADDR+0x70f8);
# elif defined(BSP_I2C_VPD_EEPROM_DEV_NAME)
    char gev_buf[3592];
    int fd;
    if ((fd = open(BSP_I2C_VPD_EEPROM_DEV_NAME, 0)) < 0) {
        printf("Can't open %s: %s\n", BSP_I2C_VPD_EEPROM_DEV_NAME, strerror(errno));
        return;
    }
    lseek(fd, 0x10f8, SEEK_SET);
    if (read(fd, gev_buf, sizeof gev_buf) != sizeof gev_buf) {
        printf("Can't read %s: %s\n", BSP_I2C_VPD_EEPROM_DEV_NAME, strerror(errno));
        return;
    }
    close(fd);
    nvp = gev_buf;
# else
#  error "No way to read GEV!"
# endif

    if (rtems_bsdnet_config.bootp != NULL)
        return;
    mot_script_boot = gev("mot-script-boot", nvp);
    if ((rtems_bsdnet_bootp_server_name = gev("mot-/dev/enet0-sipa", nvp)) == NULL)
        rtems_bsdnet_bootp_server_name = motScriptParm(mot_script_boot, 's');
    if ((rtems_bsdnet_config.gateway = gev("mot-/dev/enet0-gipa", nvp)) == NULL)
        rtems_bsdnet_config.gateway = motScriptParm(mot_script_boot, 'g');
    if  ((rtems_bsdnet_config.ifconfig->ip_netmask = gev("mot-/dev/enet0-snma", nvp)) == NULL)
        rtems_bsdnet_config.ifconfig->ip_netmask = motScriptParm(mot_script_boot, 'm');

    rtems_bsdnet_config.name_server[0] = gev("rtems-dns-server", nvp);
    if (rtems_bsdnet_config.name_server[0] == NULL)
        rtems_bsdnet_config.name_server[0] = rtems_bsdnet_bootp_server_name;
    cp = gev("rtems-dns-domainname", nvp);
    if (cp)
        rtems_bsdnet_config.domainname = cp;

    if ((rtems_bsdnet_config.ifconfig->ip_address = gev("mot-/dev/enet0-cipa", nvp)) == NULL)
        rtems_bsdnet_config.ifconfig->ip_address = motScriptParm(mot_script_boot, 'c');
    rtems_bsdnet_config.hostname = gev("rtems-client-name", nvp);
    if (rtems_bsdnet_config.hostname == NULL)
        rtems_bsdnet_config.hostname = rtems_bsdnet_config.ifconfig->ip_address;

    if ((rtems_bsdnet_bootp_boot_file_name = gev("mot-/dev/enet0-file", nvp)) == NULL)
        rtems_bsdnet_bootp_boot_file_name = motScriptParm(mot_script_boot, 'f');
    rtems_bsdnet_bootp_cmdline = gev("epics-script", nvp);
    splitRtemsBsdnetBootpCmdline();
    splitNfsMountPath(gev("epics-nfsmount", nvp));
    rtems_bsdnet_config.ntp_server[0] = gev("epics-ntpserver", nvp);
    if (rtems_bsdnet_config.ntp_server[0] == NULL)
        rtems_bsdnet_config.ntp_server[0] = rtems_bsdnet_bootp_server_name;
    if ((cp = gev("epics-tz", nvp)) != NULL)
        epicsEnvSet("TZ", cp);
}

#elif defined(HAVE_PPCBUG)
/*
 * Motorola PPCBUG NVRAM Access
 */
struct ppcbug_nvram {
    uint32_t    PacketVersionIdentifier;
    uint32_t    NodeControlMemoryAddress;
    uint32_t    BootFileLoadAddress;
    uint32_t    BootFileExecutionAddress;
    uint32_t    BootFileExecutionDelay;
    uint32_t    BootFileLength;
    uint32_t    BootFileByteOffset;
    uint32_t    TraceBufferAddress;
    uint32_t    ClientIPAddress;
    uint32_t    ServerIPAddress;
    uint32_t    SubnetIPAddressMask;
    uint32_t    BroadcastIPAddressMask;
    uint32_t    GatewayIPAddress;
    uint8_t     BootpRarpRetry;
    uint8_t     TftpRarpRetry;
    uint8_t     BootpRarpControl;
    uint8_t     UpdateControl;
    char                BootFilenameString[64];
    char                ArgumentFilenameString[64];
};

static char *addr(char *cbuf, uint32_t addr)
{
    struct in_addr a;
    if ((a.s_addr = addr) == 0)
        return NULL;
    return (char *)inet_ntop(AF_INET, &a, cbuf, INET_ADDRSTRLEN);
}

void
setBootConfigFromNVRAM(void)
{
    static struct ppcbug_nvram nvram;
    static char ip_address[INET_ADDRSTRLEN];
    static char ip_netmask[INET_ADDRSTRLEN];
    static char server[INET_ADDRSTRLEN];
    static char gateway[INET_ADDRSTRLEN];

    if (rtems_bsdnet_config.bootp != NULL)
        return;

    /*
     * Get network configuation from PPCBUG.
     * The 'correct' way to do this would be to issue a .NETCFIG PPCBUG
     * system call.  Unfortunately it is very difficult to issue such a
     * call once RTEMS is up and running so we just copy from the 'known'
     * location of the network configuration parameters.
     * Care must be taken to access the NVRAM a byte at a time.
     */

#if defined(NVRAM_INDIRECT)
   {
      volatile char *addrLo = (volatile char *)0x80000074;
      volatile char *addrHi = (volatile char *)0x80000075;
      volatile char *data = (volatile char *)0x80000077;
      int addr =  0x1000;
      char *d = (char *)&nvram;

      while (d < ((char *)&nvram + sizeof nvram)) {
         *addrLo = addr & 0xFF;
         *addrHi = (addr >> 8) & 0xFF;
         *d++ = *data;
         addr++;
      }
   }
#else
    {
    volatile char *s = (volatile char *)0xFFE81000;
    char *d = (char *)&nvram;

    while (d < ((char *)&nvram + sizeof nvram))
        *d++ = *s++;
    }
#endif
    /*
     * Assume that the boot server is also the name, log and ntp server!
     */
    rtems_bsdnet_config.name_server[0] =
    rtems_bsdnet_config.ntp_server[0]  =
      rtems_bsdnet_bootp_server_name   = addr(server, nvram.ServerIPAddress);
    rtems_bsdnet_bootp_server_address.s_addr = nvram.ServerIPAddress;
    /*
     * Nothing better to use as host name!
     */
    rtems_bsdnet_config.ifconfig->ip_address =
      rtems_bsdnet_config.hostname = addr(ip_address, nvram.ClientIPAddress);

    rtems_bsdnet_config.gateway = addr(gateway, nvram.GatewayIPAddress);
    rtems_bsdnet_config.ifconfig->ip_netmask = addr(ip_netmask, nvram.SubnetIPAddressMask);

    rtems_bsdnet_bootp_boot_file_name = nvram.BootFilenameString;
    rtems_bsdnet_bootp_cmdline = nvram.ArgumentFilenameString;
    splitRtemsBsdnetBootpCmdline();
}

#elif defined(__mcf528x__)

static char *
env(const char *parm, const char *defaultValue)
{
    const char *cp = bsp_getbenv(parm);

    if (!cp) {
        if (!defaultValue)
            return NULL;
        cp = defaultValue;
        printf ("%s environment variable missing -- using %s.\n", parm, cp);
    }
    return epicsStrDup(cp);
}

void
setBootConfigFromNVRAM(void)
{
    const char *cp1;

    if (rtems_bsdnet_config.bootp != NULL)
        return;
    rtems_bsdnet_config.gateway = env("GATEWAY", NULL);
    rtems_bsdnet_config.ifconfig->ip_netmask = env("NETMASK", "255.255.252.0");

    rtems_bsdnet_bootp_server_name = env("SERVER", "192.168.0.1");
    rtems_bsdnet_config.name_server[0] = env("NAMESERVER", rtems_bsdnet_bootp_server_name);
    rtems_bsdnet_config.ntp_server[0] = env("NTPSERVER", rtems_bsdnet_bootp_server_name);
    cp1 = env("DOMAIN", NULL);
    if (cp1 != NULL)
        rtems_bsdnet_config.domainname = cp1;
    rtems_bsdnet_config.hostname = env("HOSTNAME", "iocNobody");
    rtems_bsdnet_config.ifconfig->ip_address = env("IPADDR0", "192.168.0.2");
    rtems_bsdnet_bootp_boot_file_name = env("BOOTFILE", "uC5282App.boot");
    rtems_bsdnet_bootp_cmdline = env("CMDLINE", "epics/iocBoot/iocNobody/st.cmd");
    splitNfsMountPath(env("NFSMOUNT", NULL));
    if ((cp1 = env("TZ", NULL)) != NULL)
        epicsEnvSet("TZ", cp1);
}

#else
/*
 * Placeholder for systems without NVRAM
 */
void
setBootConfigFromNVRAM(void)
{
    printf("SYSTEM HAS NO NON-VOLATILE RAM!\n");
    printf("YOU MUST USE SOME OTHER METHOD TO OBTAIN NETWORK CONFIGURATION\n");
}
#endif
