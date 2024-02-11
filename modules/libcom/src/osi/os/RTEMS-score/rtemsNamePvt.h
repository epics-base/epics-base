/*************************************************************************\
* Copyright (c) 2023 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef RTEMSNAMEPVT_H
#define RTEMSNAMEPVT_H

#include <stdint.h>

/* Compute rtems_build_name(prefix, A, B, C) where A, B, C are the letters a-z.
 * eg. "Qaaa"
 *
 * 'counter' is incremented atomically during each call.
 */
uint32_t next_rtems_name(char prefix, uint32_t* counter);

#endif // RTEMSNAMEPVT_H
