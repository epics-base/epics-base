/*************************************************************************\
* Copyright (c) 2006 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Hooks into RTEMS startup code
 */
#include <bsp.h>
#ifdef RTEMS_LEGACY_STACK
#include <rtems/rtems_bsdnet.h>
#endif

extern char *env_nfsServer;
extern char *env_nfsPath;
extern char *env_nfsMountPoint;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return 0 for success, non-zero for failure (will cause panic)
 */
#ifdef RTEMS_LEGACY_STACK
int epicsRtemsInitPreSetBootConfigFromNVRAM(struct rtems_bsdnet_config *config);
int epicsRtemsInitPostSetBootConfigFromNVRAM(struct rtems_bsdnet_config *config);
#endif
/* Return 0 if local file system was setup, or non-zero (will fall back to network */
int epicsRtemsMountLocalFilesystem(char **argv);

#ifdef __cplusplus
}
#endif

