/*
 * RTEMS utilitiy routines for EPICS
 *  $Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 *
 * Supplies routines that are present in vxWorks but missing in RTEMS.
 */

#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

/*
 * Like connect(), but with an explicit timeout
 */
int connectWithTimeout (int sfd,
                        struct sockaddr *addr, 
                        int addrlen,
                        struct timeval *timeout)
{
    struct timeval sv;
    int svlen = sizeof sv;
    int ret;

    if (!timeout)
        return connect (sfd, addr, addrlen);
    if (getsockopt (sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&sv, &svlen) < 0)
        return -1;
    if (setsockopt (sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)timeout, sizeof *timeout) < 0)
        return -1;
    ret = connect (sfd, addr, addrlen);
    setsockopt (sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&sv, sizeof sv);
    return ret;
}

/*
 * RTEMS does not have the getprotobyXXX routines.
 * Provide these simple-minded versions.
 */
static char *ip_alias[]   = { "IP",   NULL };
static char *icmp_alias[] = { "ICMP", NULL };
static char *tcp_alias[]  = { "TCP",  NULL };
static char *udp_alias[]  = { "UDP",  NULL };
static const struct protoent prototab[] = {
            { "IP",    ip_alias,   IPPROTO_IP   },
            { "ICMP",  icmp_alias, IPPROTO_ICMP },
            { "TCP",   tcp_alias,  IPPROTO_TCP  },
            { "UDP",   udp_alias,  IPPROTO_UDP  },
            };
struct protoent *
getprotobyname (const char *name)
{
    int i;

    for (i = 0 ; i < (sizeof prototab / sizeof prototab[0]) ; i++) {
        if (strcasecmp (name, prototab[i].p_name) == 0)
            return (struct protoent *) &prototab[i];
    }
    return NULL;
}
struct protoent *
getprotobynumber (int proto)
{
    int i;

    for (i = 0 ; i < (sizeof prototab / sizeof prototab[0]) ; i++) {
        if (proto == prototab[i].p_proto)
            return (struct protoent *) &prototab[i];
    }
    return NULL;
}
