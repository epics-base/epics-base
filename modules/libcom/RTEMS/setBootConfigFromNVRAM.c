/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef RTEMS_LEGACY_STACK
#include <rtems/rtems_bsdnet.h>
#else
#include <stdio.h>
#include <net/if.h>
#include <sysexits.h>
#include <rtems/bsd/bsd.h>
#endif
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

#if defined(HAVE_MOTLOAD) || defined(HAVE_PPCBUG) || defined(__mcf528x__)

extern char* rtems_bsdnet_bootp_cmdline;
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

#if defined(HAVE_MOTLOAD) || defined(__mcf528x__)
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
#endif /* HAVE_MOTLOAD || __mcf528x__ */

struct boot_net_config {
    char *ip_address;           /* static IP address of this device */
    char *netmask;              /* subnet mask */
    char *gateway;              /* default gateway */
    char *server;               /* boot server; used as NTP/DNS fallback if not overridden */
    char *ntp_server;           /* NTP server (falls back to server if NULL) */
    char *hostname;             /* this device's hostname */
    char *dns_server;           /* DNS resolver (falls back to server if NULL) */
    char *domainname;           /* DNS domain name */
    char *bootp_boot_file;      /* boot filename reported via BOOTP */
    uint32_t bootp_server_addr; /* server IPv4 address as stored in PPCBUG NVRAM */
};

/* Configure network from boot_net_config.
 * RTEMS_LEGACY_STACK: populate rtems_bsdnet_config fields.
 * libbsd: configure the first hardware interface (index 1) with a static IP.
 * Returns 0 on success, -1 on failure. */
static int
applyNetConfig(const struct boot_net_config *cfg,
               char *ntp_server_ip, size_t ntp_server_ip_size)
{
#ifdef RTEMS_LEGACY_STACK
    (void)ntp_server_ip;
    (void)ntp_server_ip_size;
    rtems_bsdnet_bootp_server_name           = cfg->server;
    rtems_bsdnet_config.name_server[0]       = cfg->dns_server ? cfg->dns_server : cfg->server;
    rtems_bsdnet_config.ntp_server[0]        = cfg->ntp_server ? cfg->ntp_server : cfg->server;
    rtems_bsdnet_config.gateway              = cfg->gateway;
    rtems_bsdnet_config.ifconfig->ip_netmask = cfg->netmask;
    rtems_bsdnet_config.ifconfig->ip_address = cfg->ip_address;
    rtems_bsdnet_config.hostname             = cfg->hostname ? cfg->hostname : cfg->ip_address;
    if (cfg->domainname)
        rtems_bsdnet_config.domainname = cfg->domainname;
    if (cfg->bootp_boot_file)
        rtems_bsdnet_bootp_boot_file_name = cfg->bootp_boot_file;
    if (cfg->bootp_server_addr)
        rtems_bsdnet_bootp_server_address.s_addr = cfg->bootp_server_addr;
#else
    char ifnamebuf[IF_NAMESIZE];
    char *ifname = if_indextoname(1, ifnamebuf);
    if (ifname == NULL) {
        printf("No network interface found\n");
        return -1;
    }
    printf("Configuring ifconfig with ip=%s, netmask=%s, gateway=%s\n",
           cfg->ip_address ? cfg->ip_address : "NULL",
           cfg->netmask    ? cfg->netmask    : "NULL",
           cfg->gateway    ? cfg->gateway    : "NULL");
    if (cfg->ip_address && cfg->netmask) {
        int exit_code = rtems_bsd_ifconfig(ifname, cfg->ip_address,
                                           cfg->netmask, cfg->gateway);
        if (exit_code != EX_OK) {
            printf("rtems_bsd_ifconfig failed (exit code %d)\n", exit_code);
            return -1;
        }
    } else {
        printf("Skipping static IP address and netmask from NVRAM\n");
        return -1;
    }
    if (ntp_server_ip != NULL && ntp_server_ip_size > 0 && cfg->ntp_server != NULL)
        snprintf(ntp_server_ip, ntp_server_ip_size, "%s", cfg->ntp_server);
#endif
    return 0;
}

#endif /* HAVE_MOTLOAD || HAVE_PPCBUG || __mcf528x__ */

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

int
setBootConfigFromNVRAM(char *ntp_server_ip, size_t ntp_server_ip_size)
{
    const char *mot_script_boot;
    volatile char *nvp;
    struct boot_net_config cfg = {0};

# if defined(BSP_NVRAM_BASE_ADDR)
    nvp = (volatile char *)(BSP_NVRAM_BASE_ADDR+0x70f8);
# elif defined(BSP_I2C_VPD_EEPROM_DEV_NAME)
    char gev_buf[3592];
    int fd;
    if ((fd = open(BSP_I2C_VPD_EEPROM_DEV_NAME, 0)) < 0) {
        printf("Can't open %s: %s\n", BSP_I2C_VPD_EEPROM_DEV_NAME, strerror(errno));
        return -1;
    }
    lseek(fd, 0x10f8, SEEK_SET);
    if (read(fd, gev_buf, sizeof gev_buf) != sizeof gev_buf) {
        printf("Can't read %s: %s\n", BSP_I2C_VPD_EEPROM_DEV_NAME, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    nvp = gev_buf;
# else
#  error "No way to read GEV!"
# endif

    mot_script_boot = gev("mot-script-boot", nvp);

    /*
     * Read network parameters from NVRAM
     */
    if ((cfg.ip_address = gev("mot-/dev/enet0-cipa", nvp)) == NULL) {
        cfg.ip_address = motScriptParm(mot_script_boot, 'c');
    }

    if ((cfg.netmask = gev("mot-/dev/enet0-snma", nvp)) == NULL) {
        cfg.netmask = motScriptParm(mot_script_boot, 'm');
    }
    if ((cfg.gateway = gev("mot-/dev/enet0-gipa", nvp)) == NULL) {
        cfg.gateway = motScriptParm(mot_script_boot, 'g');
    }

    if ((cfg.server = gev("mot-/dev/enet0-sipa", nvp)) == NULL) {
        cfg.server = motScriptParm(mot_script_boot, 's');
    }


    {
        char *ntp_gev = gev("epics-ntpserver", nvp);
        cfg.ntp_server = ntp_gev ? ntp_gev : cfg.server;
    }
    cfg.hostname   = gev("rtems-client-name", nvp);
    cfg.dns_server = gev("rtems-dns-server", nvp);
    cfg.domainname = gev("rtems-dns-domainname", nvp);
    cfg.bootp_boot_file  = gev("mot-/dev/enet0-file", nvp);
    if (cfg.bootp_boot_file == NULL)
        cfg.bootp_boot_file = motScriptParm(mot_script_boot, 'f');

    /*
     * Apply network configuration
     */
#ifdef RTEMS_LEGACY_STACK
    if (rtems_bsdnet_config.bootp != NULL)
        return 0;
#endif
    if (applyNetConfig(&cfg, ntp_server_ip, ntp_server_ip_size) != 0)
        return -1;

    rtems_bsdnet_bootp_cmdline = gev("epics-script", nvp);
    splitRtemsBsdnetBootpCmdline();
    splitNfsMountPath(gev("epics-nfsmount", nvp));
    {
        char *tz = gev("epics-tz", nvp);
        if (tz != NULL)
            epicsEnvSet("TZ", tz);
    }
    return 0;
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

int
setBootConfigFromNVRAM(char *ntp_server_ip, size_t ntp_server_ip_size)
{
    static struct ppcbug_nvram nvram;
    static char ip_address[INET_ADDRSTRLEN];
    static char ip_netmask[INET_ADDRSTRLEN];
    static char server[INET_ADDRSTRLEN];
    static char gateway[INET_ADDRSTRLEN];

    /*
     * Get network configuration from PPCBUG.
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

    char * serverp = addr(server, nvram.ServerIPAddress);
    struct boot_net_config cfg = {
        addr(ip_address, nvram.ClientIPAddress),
        addr(ip_netmask, nvram.SubnetIPAddressMask),
        addr(gateway,    nvram.GatewayIPAddress),
        serverp,
        serverp,               /* ntp_server defaults to server */
        NULL,                  /* hostname */
        NULL,                  /* dns_server */
        NULL,                  /* domainname */
        nvram.BootFilenameString, /* bootp_boot_file */
        nvram.ServerIPAddress,    /* bootp_server_addr */
    };

#ifdef RTEMS_LEGACY_STACK
    if (rtems_bsdnet_config.bootp != NULL)
        return 0;
#endif
    if (applyNetConfig(&cfg, ntp_server_ip, ntp_server_ip_size) != 0)
        return -1;
    rtems_bsdnet_bootp_cmdline = nvram.ArgumentFilenameString;
    splitRtemsBsdnetBootpCmdline();
    return 0;
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

int
setBootConfigFromNVRAM(char *ntp_server_ip, size_t ntp_server_ip_size)
{
    char *server_str = env("SERVER", "192.168.0.1");
    struct boot_net_config cfg = {
        env("IPADDR0",   "192.168.0.2"),
        env("NETMASK",   "255.255.252.0"),
        env("GATEWAY",   NULL),
        server_str,
        env("NTPSERVER", server_str),
        env("HOSTNAME",  "iocNobody"),
        env("NAMESERVER", server_str),
        env("DOMAIN",    NULL),
        env("BOOTFILE",  "uC5282App.boot"),
        0,  /* bootp_server_addr */
    };

#ifdef RTEMS_LEGACY_STACK
    if (rtems_bsdnet_config.bootp != NULL)
        return 0;
#endif
    if (applyNetConfig(&cfg, ntp_server_ip, ntp_server_ip_size) != 0)
        return -1;
    rtems_bsdnet_bootp_cmdline = env("CMDLINE", "epics/iocBoot/iocNobody/st.cmd");
    splitRtemsBsdnetBootpCmdline();
    splitNfsMountPath(env("NFSMOUNT", NULL));
    {
        char *tz = env("TZ", NULL);
        if (tz != NULL)
            epicsEnvSet("TZ", tz);
    }
    return 0;
}

#else
/*
 * Placeholder for systems without NVRAM
 */
int
setBootConfigFromNVRAM(char *ntp_server_ip, size_t ntp_server_ip_size)
{
    printf("SYSTEM HAS NO NON-VOLATILE RAM!\n");
    printf("YOU MUST USE SOME OTHER METHOD TO OBTAIN NETWORK CONFIGURATION\n");
    return -1;
}
#endif
