#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rtems/rtems_bsdnet.h>
#include <bsp.h>
#include <string.h>
#include <ctype.h>
#include <epicsStdlib.h>
#include <epicsStdio.h>
#include <epicsString.h>

#if defined(HAVE_MOTLOAD)
/*
 * Motorola MOTLOAD NVRAM Access
 */
static char *
gev(const char *parm)
{
    volatile char *nvp = (volatile unsigned char *)(GT64260_DEV1_BASE + 0x10000 + 0x70F8);
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
        if (isspace(*mot_script_boot)
         && (*(mot_script_boot+1) == '-')
         && (*(mot_script_boot+2) == parm)) {
            mot_script_boot += 3;
            cp = mot_script_boot;
            while ((*mot_script_boot != '\0') && !isspace(*mot_script_boot))
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

    if (rtems_bsdnet_config.bootp != NULL)
        return;
    mot_script_boot = gev("mot-script-boot");
    if ((rtems_bsdnet_bootp_server_name = gev("mot-/dev/enet0-sipa")) == NULL)
        rtems_bsdnet_bootp_server_name = motScriptParm(mot_script_boot, 's');
    if ((rtems_bsdnet_config.gateway = gev("mot-/dev/enet0-gipa")) == NULL)
        rtems_bsdnet_config.gateway = motScriptParm(mot_script_boot, 'g');
    if  ((rtems_bsdnet_config.ifconfig->ip_netmask = gev("mot-/dev/enet0-snma")) == NULL)
        rtems_bsdnet_config.ifconfig->ip_netmask = motScriptParm(mot_script_boot, 'm');

    rtems_bsdnet_config.name_server[0] = gev("rtems-dns-server");
    if (rtems_bsdnet_config.name_server[0] == NULL)
        rtems_bsdnet_config.name_server[0] = rtems_bsdnet_bootp_server_name;
    rtems_bsdnet_config.log_host = rtems_bsdnet_bootp_server_name;
    cp = gev("rtems-dns-domainname");
    if (cp)
        rtems_bsdnet_config.domainname = cp;

    if ((rtems_bsdnet_config.ifconfig->ip_address = gev("mot-/dev/enet0-cipa")) == NULL)
        rtems_bsdnet_config.ifconfig->ip_address = motScriptParm(mot_script_boot, 'c');
    rtems_bsdnet_config.hostname = gev("rtems-client-name");
    if (rtems_bsdnet_config.hostname == NULL)
        rtems_bsdnet_config.hostname = rtems_bsdnet_config.ifconfig->ip_address;

    if ((rtems_bsdnet_bootp_boot_file_name = gev("mot-/dev/enet0-file")) == NULL)
        rtems_bsdnet_bootp_boot_file_name = motScriptParm(mot_script_boot, 'f');
    rtems_bsdnet_bootp_cmdline = gev("epics-script");
}

#elif defined(HAVE_PPCBUG)
/*
 * Motorola PPCBUG NVRAM Access
 */
struct ppcbug_nvram {
    rtems_unsigned32    PacketVersionIdentifier;
    rtems_unsigned32    NodeControlMemoryAddress;
    rtems_unsigned32    BootFileLoadAddress;
    rtems_unsigned32    BootFileExecutionAddress;
    rtems_unsigned32    BootFileExecutionDelay;
    rtems_unsigned32    BootFileLength;
    rtems_unsigned32    BootFileByteOffset;
    rtems_unsigned32    TraceBufferAddress;
    rtems_unsigned32    ClientIPAddress;
    rtems_unsigned32    ServerIPAddress;
    rtems_unsigned32    SubnetIPAddressMask;
    rtems_unsigned32    BroadcastIPAddressMask;
    rtems_unsigned32    GatewayIPAddress;
    rtems_unsigned8     BootpRarpRetry;
    rtems_unsigned8     TftpRarpRetry;
    rtems_unsigned8     BootpRarpControl;
    rtems_unsigned8     UpdateControl;
    char                BootFilenameString[64];
    char                ArgumentFilenameString[64];
};

static char *addr(char *cbuf, rtems_unsigned32 addr)
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
    {
    volatile char *s = (volatile char *)0xFFE81000;
    char *d = (char *)&nvram;

    while (d < ((char *)&nvram + sizeof nvram))
        *d++ = *s++;
    }

    /*
     * Assume that the boot server is also the name server and log server!
     */
    rtems_bsdnet_config.name_server[0] =
      rtems_bsdnet_bootp_server_name   =
      rtems_bsdnet_config.log_host     = addr(server, nvram.ServerIPAddress);
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
    if (rtems_bsdnet_config.bootp != NULL)
        return;
    rtems_bsdnet_config.gateway = env("GATEWAY", NULL);
    rtems_bsdnet_config.ifconfig->ip_netmask = env("NETMASK", "255.255.252.0");

    rtems_bsdnet_bootp_server_name = env("SERVER", "192.168.0.1");
    rtems_bsdnet_config.name_server[0] = env("NAMESERVER", rtems_bsdnet_bootp_server_name);
    rtems_bsdnet_config.ntp_server[0] = env("NTPSERVER", rtems_bsdnet_bootp_server_name);
    rtems_bsdnet_config.log_host = env("LOGSERVER", rtems_bsdnet_bootp_server_name);
    if (rtems_bsdnet_config.domainname == NULL)
        rtems_bsdnet_config.domainname = env("DOMAIN", "local");
    rtems_bsdnet_config.hostname = env("HOSTNAME", "iocNobody");
    rtems_bsdnet_config.ifconfig->ip_address = env("IPADDR0", "192.168.0.2");
    rtems_bsdnet_bootp_boot_file_name = env("BOOTFILE", "epics/iocNobody/bin/RTEMS-uC5282/myApp.boot");
    rtems_bsdnet_bootp_cmdline = env("CMDLINE", "epics/iocBoot/iocNobody/st.cmd");
}

#else
/*
 * Placeholder for systems without NVRAM
 */
void
setBootConfigFromNVRAM(void)
{
    printf("SYSTEM HAS NO NON-VOLATILE RAM!\n");
    printf("YOU MUST USE BOOTP/DHCP TO OBTAIN NETWORK CONFIGURATION\n");
}
#endif
