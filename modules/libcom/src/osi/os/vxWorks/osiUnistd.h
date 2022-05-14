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
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 *  505 665 1831
 */

#include <unistd.h>

/* VxWorks has some functions in ioLib.h instead of unistd.h,
   for example close() and read()
 */
#include <ioLib.h>
