/*
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * SPDX-License-Identifier: EPICS
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author: Jeremy Lorelli <lorelli@slac.stanford.edu>, 2024
 */
#ifndef _EPICS_LIMITS_H
#define _EPICS_LIMITS_H

#include <limits.h>
#include <stdlib.h>

#ifndef PATH_MAX
#   define PATH_MAX _MAX_PATH
#endif

#ifndef NAME_MAX
#   define NAME_MAX _MAX_FNAME
#endif

#endif /* _EPICS_LIMITS_H */
