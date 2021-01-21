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
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include <bsp.h>

#include <machine/rtems-bsd-commands.h>
#include <machine/rtems-bsd-rc-conf.h>
#include <machine/rtems-bsd-rc-conf-services.h>

#include "libbsd_netconfig.h" 

#include <rtems/console.h>
#include <rtems/shell.h>

#define IFACE_IPV4_DHCP(iface) \
  "ifconfig_" # iface "=\"SYNCDHCP\"\n"

#define IFACE_IPV4(iface, addr) \
  "ifconfig_" # iface "=\"inet " # addr " netmask 255.255.255.0\"\n"

static const char* rc_conf_text =                          \
  "\n"                                                     \
  "syslog_priority=\"debug\"\n"                            \
  "\n"                                                     \
  "#create_args_myvlan=\"vlan 102\"\n"                     \
  "#create_args_yourvlan=\"vlan 202\"\n"                   \
  IFACE_IPV4_DHCP(cgem0)				                   \
  "\n"                                                     \
  "dhcpcd_priority=\"100\"\n"                              \
  "\n"                                                     \
  "#defaultrouter=\"" NET_CFG_GATEWAY_IP "\"\n"             \
  "defaultroute_delay=\"5\"\n"                             \
  "\n"                                                     \
  "#telnetd_enable=\"YES\"\n"                               \
  "#telnetd_options=\"-C 2 -P 100 \"\n"                \
  "\n"                                                     \
  "#ftpd_enable=\"YES\"\n"                                  \
  "#ftpd_options=\"-v -p 21 -C 2 -P 150 -L -I 10 -R /\"\n" \
  "\n"                                                     \
  "#pf_enable=\"YES\"\n"                                    \
  "#pf_rules=\"/etc/mypf.conf\"\n"                          \
  "#pf_flags=\"-q -z\"\n"                                   \
  "\n";

static const char* dhcpcd_conf_text =                    \
  "\n"                                                  \
"clientid RTEMS test client\n"                           \
"\n"                                                      \
"nodhcp6\n" \
"ipv4only\n"                             \
"option ntp_servers\n" \
"option rtems_cmdline\n";

int
epicsRtemsPrepareAndRunRCConfFile(void)
{
  size_t len;
  size_t written;
  int fd;
  int rv;
  struct stat sb;

  mkdir("/etc", S_IRWXU | S_IRWXG | S_IRWXO); /* ignore errors, check the dir after. */
  assert(stat("/etc", &sb) == 0);
  assert(S_ISDIR(sb.st_mode));

  printf("--------------- dhcpcd.conf -----------------\n");
  printf(dhcpcd_conf_text);
  printf("-----------------------------------------\n");

  len = strlen(dhcpcd_conf_text);
  fd = open("/etc/dhcpcd.conf", O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
  assert(fd != -1);
  written = write(fd, dhcpcd_conf_text, len);
  assert(written == len);
  rv = close(fd);
  assert(rv == 0);

  printf("--------------- rc.conf -----------------\n");
  printf(rc_conf_text);
  printf("-----------------------------------------\n");

  len = strlen(rc_conf_text);
  fd = open("/etc/rc.conf", O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
  assert(fd != -1);
  written = write(fd, rc_conf_text, len);
  assert(written == len);
  rv = close(fd);
  assert(rv == 0);

  return (rtems_bsd_run_etc_rc_conf(20, true));
}
