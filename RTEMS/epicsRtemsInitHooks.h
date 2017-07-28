/*************************************************************************\
* Copyright (c) 2006 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Hooks into RTEMS startup code
 */
#include <bsp.h>
#include <rtems/rtems_bsdnet.h>

extern char *env_nfsServer;
extern char *env_nfsPath;
extern char *env_nfsMountPoint;

/*
 * Return 0 for success, non-zero for failure (will cause panic)
 */
int epicsRtemsInitPreSetBootConfigFromNVRAM(struct rtems_bsdnet_config *config);
int epicsRtemsInitPostSetBootConfigFromNVRAM(struct rtems_bsdnet_config *config);
