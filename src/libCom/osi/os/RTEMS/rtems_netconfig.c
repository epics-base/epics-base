/*
 * RTEMS network configuration for EPICS
 *  $Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
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
static struct rtems_bsdnet_ifconfig netdriver_config = {
    RTEMS_BSP_NETWORK_DRIVER_NAME,      /* name */
    RTEMS_BSP_NETWORK_DRIVER_ATTACH,    /* attach function */
    &loopback_config,                   /* link to next interface */
};
struct rtems_bsdnet_config rtems_bsdnet_config = {
    &netdriver_config,        /* Network interface */
    rtems_bsdnet_do_bootp,    /* Use BOOTP to get network configuration */
    NETWORK_TASK_PRIORITY,    /* Network task priority */
    180*1024,                 /* MBUF space */
    350*1024,                 /* MBUF cluster space */
};

