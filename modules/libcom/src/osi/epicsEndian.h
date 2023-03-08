/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsEndian.h
 * \brief Defining byte order and float word order macros.
 * 
 * This code defines macros to represent the byte order and float word order of the target platform.
 * It then includes the OS dependent file osdWireConfig.h, which should define these macros to
 * their appropriate values for the target platform.
 * If osdWireConfig.h does not define EPICS_BYTE_ORDER or 
 * EPICS_FLOAT_WORD_ORDER macros, the code raises a preprocessor error using the #error directive.
 * 
 * \warning This file must be usable from both C and C++ 
 */
 
#ifndef INC_epicsEndian_H
#define INC_epicsEndian_H

#define EPICS_ENDIAN_LITTLE   1234
#define EPICS_ENDIAN_BIG      4321


/**
 * \brief The following OS Dependent file defines the macros EPICS_BYTE_ORDER  and  EPICS_FLOAT_WORD_ORDER  to be one of the above  EPICS_ENDIAN_  values.
 */
#include "osdWireConfig.h"

#ifndef EPICS_BYTE_ORDER
#error osdWireConfig.h didnt define EPICS_BYTE_ORDER
#endif

#ifndef EPICS_FLOAT_WORD_ORDER
#error osdWireConfig.h didnt define EPICS_FLOAT_WORD_ORDER
#endif

#endif /* INC_epicsEndian_H */
