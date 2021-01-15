/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * RTEMS network configuration header for EPICS
 *
 * This file can be copied to an application source dirctory
 * and modified to override the values shown below.
 */
#include <stdio.h>
#include <bsp.h>

#define NET_CFG_INTERFACE_0 "cgem0"
#define DEFAULT_NETWORK_DHCPCD_ENABLE
#define DEFAULT_NETWORK_NO_INTERFACE_0

// Tests
#define NET_CFG_SELF_IP "141.14.128.89"
#define NET_CFG_NETMASK "255.255.240.0"
#define NET_CFG_GATEWAY_IP "141.14.128.128"

int epicsRtemsPrepareAndRunRCConfFile(void);
