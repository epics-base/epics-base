/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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
 * On RTEMS-pc386 targets all network drivers which support run-time
 * probing are linked. 
 * On other targets the network interface specified by the board-support
 * package is used.
 * To use a different NIC for a particular application, copy this file to the
 * application directory and make the appropriate changes.
 */
#if defined(__i386__)

extern int rtems_fxp_attach (struct rtems_bsdnet_ifconfig *, int);
static struct rtems_bsdnet_ifconfig fxp_driver_config = {
    "fxp1",                             /* name */
    rtems_fxp_attach,                   /* attach function */
    &loopback_config,                   /* link to next interface */
};
extern int rtems_3c509_driver_attach (struct rtems_bsdnet_ifconfig *, int);
static struct rtems_bsdnet_ifconfig e3c509_driver_config = {
    "ep0",                              /* name */
    rtems_3c509_driver_attach,          /* attach function */
    &fxp_driver_config,                 /* link to next interface */
};
#define FIRST_DRIVER_CONFIG &e3c509_driver_config

#else

/*
 * FIXME: This really belongs in the mcp750 BSP
 */
#if defined(__PPC) && defined(mpc750) && !defined(RTEMS_BSP_NETWORK_DRIVER_NAME) && !defined(RTEMS_BSP_NETWORK_DRIVER_ATTACH)
# define RTEMS_BSP_NETWORK_DRIVER_NAME "dc1"
# define RTEMS_BSP_NETWORK_DRIVER_ATTACH rtems_dec21140_driver_attach
  extern int rtems_dec21140_driver_attach();
#endif

static struct rtems_bsdnet_ifconfig bsp_driver_config = {
    RTEMS_BSP_NETWORK_DRIVER_NAME,      /* name */
    RTEMS_BSP_NETWORK_DRIVER_ATTACH,    /* attach function */
    &loopback_config,                   /* link to next interface */
};
#define FIRST_DRIVER_CONFIG &bsp_driver_config

#endif

struct rtems_bsdnet_config rtems_bsdnet_config = {
    FIRST_DRIVER_CONFIG,      /* Link to next interface */
    rtems_bsdnet_do_bootp,    /* Use BOOTP to get network configuration */
    NETWORK_TASK_PRIORITY,    /* Network task priority */
    180*1024,                 /* MBUF space */
    350*1024,                 /* MBUF cluster space */
};
