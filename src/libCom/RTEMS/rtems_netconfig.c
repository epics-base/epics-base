/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * RTEMS network configuration for EPICS
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

# if defined(__PPC)
  /*
   * FIXME: This really belongs in the BSP
   */
#  ifndef RTEMS_BSP_NETWORK_DRIVER_NAME
#   define RTEMS_BSP_NETWORK_DRIVER_NAME "dc1"
#  endif
#  ifndef RTEMS_BSP_NETWORK_DRIVER_ATTACH
#   define RTEMS_BSP_NETWORK_DRIVER_ATTACH rtems_dec21140_driver_attach
    extern int rtems_dec21140_driver_attach();
#  endif
# endif

static struct rtems_bsdnet_ifconfig bsp_driver_config = {
    RTEMS_BSP_NETWORK_DRIVER_NAME,      /* name */
    RTEMS_BSP_NETWORK_DRIVER_ATTACH,    /* attach function */
    &loopback_config,                   /* link to next interface */
};
#define FIRST_DRIVER_CONFIG &bsp_driver_config

#endif

/*
 * Allow configure/os/CONFIG_SITE.Common.RTEMS to provide domain name
 */
#ifdef RTEMS_NETWORK_CONFIG_DNS_DOMAINNAME
# define XSTR(x) STR(x)
# define STR(x) #x
# define MY_DOMAINNAME XSTR(RTEMS_NETWORK_CONFIG_DNS_DOMAINNAME)
#else
# define MY_DOMAINNAME NULL
#endif

/*
 * Allow non-BOOTP network configuration
 */
#ifndef MY_DO_BOOTP
# define MY_DO_BOOTP rtems_bsdnet_do_bootp
#endif

/*
 * Allow site- and BSP-specific network buffer space configuration.
 * The macro values are specified in KBytes.
 */
#ifndef RTEMS_NETWORK_CONFIG_MBUF_SPACE
# define RTEMS_NETWORK_CONFIG_MBUF_SPACE 180
#endif
#ifndef RTEMS_NETWORK_CONFIG_CLUSTER_SPACE
# define RTEMS_NETWORK_CONFIG_CLUSTER_SPACE 350
#endif

/*
 * Network configuration
 */
struct rtems_bsdnet_config rtems_bsdnet_config = {
    FIRST_DRIVER_CONFIG,   /* Link to next interface */
    MY_DO_BOOTP,           /* How to find network config */
    10,                    /* If 0 then the network daemons will run at a */
                           /*   priority just less than the lowest-priority */
                           /*   EPICS scan thread. */
                           /* If non-zero then the network daemons will run */
                           /*   at this *RTEMS* priority */
    RTEMS_NETWORK_CONFIG_MBUF_SPACE*1024,
    RTEMS_NETWORK_CONFIG_CLUSTER_SPACE*1024,
    NULL,                  /* Host name */
    MY_DOMAINNAME,         /* Domain name */
};
