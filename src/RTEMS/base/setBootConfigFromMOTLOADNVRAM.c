#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rtems/rtems_bsdnet.h>
#include <bsp.h>
#include <string.h>
#include <stdlib.h>

static char *
gev(const char *parm)
{
    volatile char *nvp = (volatile unsigned char *)(GT64260_DEV1_BASE + 0x10000 + 0x70F8);
    char *val;
    char *ret;
    const char *name;
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
                nvp = val;
                val = ret;
                while ((*val++ = *nvp++) != '\0')
                    continue;
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

void
setBootConfigFromMOTLOADNVRAM(void)
{
    char *cp;

    if (rtems_bsdnet_config.bootp != NULL)
        return;
    rtems_bsdnet_bootp_server_name = gev("mot-/dev/enet0-sipa");
    rtems_bsdnet_config.gateway = gev("mot-/dev/enet0-gipa");
    rtems_bsdnet_config.ifconfig->ip_netmask = gev("mot-/dev/enet0-snma");

    rtems_bsdnet_config.name_server[0] = gev("rtems-dns-server");
    if (rtems_bsdnet_config.name_server[0] == NULL)
        rtems_bsdnet_config.name_server[0] = rtems_bsdnet_bootp_server_name;
    rtems_bsdnet_config.log_host = rtems_bsdnet_bootp_server_name;
    cp = gev("rtems-dns-domainname");
    if (cp)
        rtems_bsdnet_config.domainname = cp;

    rtems_bsdnet_config.ifconfig->ip_address = gev("mot-/dev/enet0-cipa");
    rtems_bsdnet_config.hostname = gev("rtems-client-name");
    if (rtems_bsdnet_config.hostname == NULL)
        rtems_bsdnet_config.hostname = rtems_bsdnet_config.ifconfig->ip_address;

    rtems_bsdnet_bootp_boot_file_name = gev("mot-/dev/enet0-file");
    rtems_bsdnet_bootp_cmdline = gev("epics-script");
}
