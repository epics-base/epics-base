/*
 * RTEMS network configuration for EPICS
 *  $Id$
 *      Author: W. Eric Norum
 *              eric.norum@usask.ca
 *              (306) 966-5394
 *
 * This file can be copied to an application source dirctory
 * and modified to override the values shown below.
 */
#include <stdio.h>
#include <bsp.h>
#include <rtems/rtems_bsdnet.h>

#define NETWORK_TASK_PRIORITY           90

extern void rtems_bsdnet_loopattach();
static struct rtems_bsdnet_ifconfig loopback_config = {
    "lo0",                          /* name */
    (int (*)(struct rtems_bsdnet_ifconfig *, int))rtems_bsdnet_loopattach, /* attach function */
    NULL,                           /* link to next interface */
    "127.0.0.1",                    /* IP address */
    "255.0.0.0",                    /* IP net mask */
};

/*
 * The following conditionals select the network interface card.
 *
 * By default the network interface specified by the board-support
 * package is used.
 * To use a different NIC for a particular application, copy this file to the
 * application directory and add the appropriate -Dxxxx to the compiler flag.
 * To specify a different NIC on a site-wide basis, add the appropriate
 * flags to the site configuration file for the target.  For example, to
 * specify a 3COM 3C509 for all RTEMS-pc386 targets at your site, add
 *      TARGET_CFLAGS += -DEPICS_RTEMS_NIC_3C509
 * to configure/os/CONFIG_SITE.Common.RTEMS-pc386.
 */
#if defined(EPICS_RTEMS_NIC_3C509)       /* 3COM 3C509 */
  extern int rtems_3c509_driver_attach (struct rtems_bsdnet_ifconfig *, int);
# define NIC_NAME   "en0"
# define NIC_ATTACH rtems_3c509_driver_attach

#else                                    /* Use NIC provided by BSP */
# define NIC_NAME   RTEMS_BSP_NETWORK_DRIVER_NAME
# define NIC_ATTACH RTEMS_BSP_NETWORK_DRIVER_ATTACH
#endif

static struct rtems_bsdnet_ifconfig netdriver_config = {
    NIC_NAME,                           /* name */
    NIC_ATTACH,                         /* attach function */
    &loopback_config,                   /* link to next interface */
};
struct rtems_bsdnet_config rtems_bsdnet_config = {
    &netdriver_config,        /* Network interface */
    rtems_bsdnet_do_bootp,    /* Use BOOTP to get network configuration */
    NETWORK_TASK_PRIORITY,    /* Network task priority */
    180*1024,                 /* MBUF space */
    350*1024,                 /* MBUF cluster space */
};
