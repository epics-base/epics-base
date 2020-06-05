/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file alarmString.h
 * \brief Deprecated, use alarm.h instead
 *
 * How to convert alarm status and severity values into a string for printing.
 *
 * \note This file is deprecated, use alarm.h instead.
 */

#ifndef INC_alarmString_H
#define INC_alarmString_H

#include "alarm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief An alias for epicsAlarmSeverityStrings
 */
#define alarmSeverityString epicsAlarmSeverityStrings
/**
 * \brief An alias for epicsAlarmConditionStrings
 */
#define alarmStatusString epicsAlarmConditionStrings


#ifdef __cplusplus
}
#endif

#endif
