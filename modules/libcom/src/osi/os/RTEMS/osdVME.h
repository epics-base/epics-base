/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * OS-dependent VME support
 */
#ifndef __i386__
#ifndef __mc68000
#ifndef __arm__
#include <bsp/VME.h>
#endif
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

void bcopyLongs(char *source, char *destination, int nlongs);

#ifdef __cplusplus
}
#endif
