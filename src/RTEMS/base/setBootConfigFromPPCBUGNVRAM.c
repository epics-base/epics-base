#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rtems/rtems_bsdnet.h>

#if defined(__PPC)

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
#define PPCBUG_NVRAM ((void *)0xFFE81000)

static char *addr(char *cbuf, rtems_unsigned32 addr)
{
    struct in_addr a;
    if ((a.s_addr = addr) == 0)
        return NULL;
    return (char *)inet_ntop(AF_INET, &a, cbuf, INET_ADDRSTRLEN);
}

void
setBootConfigFromPPCBUGNVRAM(void)
{
    char *d;
    volatile char *s;
    static struct ppcbug_nvram nvram;
    static char ip_address[INET_ADDRSTRLEN];
    static char ip_netmask[INET_ADDRSTRLEN];
    static char server[INET_ADDRSTRLEN];
    static char gateway[INET_ADDRSTRLEN];

    /*
     * NVRAM must be accessed a byte at a time
     */
    s = (volatile char *)PPCBUG_NVRAM;
    d = (char *)&nvram;
    while (d < ((char *)&nvram + sizeof(nvram)))
        *d++ = *s++;

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

#endif /* defined(__PPC) */
