#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rtems/rtems_bsdnet.h>
#include <bsp.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
setBootConfigFromMOTLOADNVRAM(void)
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
