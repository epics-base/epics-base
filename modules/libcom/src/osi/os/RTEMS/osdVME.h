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
/*
 * ?? Is PowerPC the only EPICS arch to support VME?
 */
#if !defined(__i386__) && !defined(__mc68000) && \
    !defined(arm) && !defined(__aarch64__)
#include <bsp/VME.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void bcopyLongs(char *source, char *destination, int nlongs);

#ifdef __cplusplus
}
#endif
