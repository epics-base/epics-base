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
 * \file alarm.h
 * \brief Alarm severity and status/condition values
 * \author Bob Dalesio and Marty Kraimer
 *
 * These alarm definitions must match the related
 * menuAlarmSevr.dbd and menuAlarmStat.dbd files
 * found in the IOC database module.
 */

#ifndef INC_alarm_H
#define INC_alarm_H

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The NO_ALARM value can be used as both a severity and a status.
 */
#define NO_ALARM            0

/**
 * \brief Alarm severity values
 * \note These must match the choices in menuAlarmSevr.dbd
 */
typedef enum {
    epicsSevNone = NO_ALARM, /**< No alarm */
    epicsSevMinor,           /**< Minor alarm severity */
    epicsSevMajor,           /**< Major alarm severity */
    epicsSevInvalid,         /**< Invalid alarm severity */
    ALARM_NSEV               /**< Number of alarm severities */
} epicsAlarmSeverity;

/**
 * \name Original macros for alarm severity values
 * @{
 */
#define firstEpicsAlarmSev  epicsSevNone
#define MINOR_ALARM         epicsSevMinor
#define MAJOR_ALARM         epicsSevMajor
#define INVALID_ALARM       epicsSevInvalid
#define lastEpicsAlarmSev   epicsSevInvalid
/** @} */

/**
 * \brief Alarm status/condition values
 * \note These must match the choices in menuAlarmStat.dbd
 */
typedef enum {
    epicsAlarmNone = NO_ALARM, /**< No alarm */
    epicsAlarmRead,            /**< Read alarm (read error) */
    epicsAlarmWrite,           /**< Write alarm (write error) */
    epicsAlarmHiHi,            /**< High high limit alarm */
    epicsAlarmHigh,            /**< High limit alarm */
    epicsAlarmLoLo,            /**< Low low limit alarm */
    epicsAlarmLow,             /**< Low limit alarm */
    epicsAlarmState,           /**< State alarm (e.g. off/on) */
    epicsAlarmCos,             /**< Change of state alarm */
    epicsAlarmComm,            /**< Communication alarm */
    epicsAlarmTimeout,         /**< Timeout alarm */
    epicsAlarmHwLimit,         /**< Hardware limit alarm */
    epicsAlarmCalc,            /**< Calculation expression error */
    epicsAlarmScan,            /**< Scan alarm, e.g. record not processed (10 times) or not in desired scan list */
    epicsAlarmLink,            /**< Link alarm */
    epicsAlarmSoft,            /**< Soft alarm, e.g. in sub record if subroutine gives error */
    epicsAlarmBadSub,          /**< Bad subroutine alarm, e.g. in sub record subroutine not defined */
    epicsAlarmUDF,             /**< Undefined value alarm, e.g. record never processed */
    epicsAlarmDisable,         /**< Record disabled using DISV/DISA fields */
    epicsAlarmSimm,            /**< Record is in simulation mode */
    epicsAlarmReadAccess,      /**< Read access permission problem */
    epicsAlarmWriteAccess,     /**< Write access permission problem */
    ALARM_NSTATUS              /**< Number of alarm conditions */
} epicsAlarmCondition;

/**
 * \name Original macros for alarm status/condition values
 * @{
 */
#define firstEpicsAlarmCond epicsAlarmNone
#define READ_ALARM          epicsAlarmRead
#define WRITE_ALARM         epicsAlarmWrite
#define HIHI_ALARM          epicsAlarmHiHi
#define HIGH_ALARM          epicsAlarmHigh
#define LOLO_ALARM          epicsAlarmLoLo
#define LOW_ALARM           epicsAlarmLow
#define STATE_ALARM         epicsAlarmState
#define COS_ALARM           epicsAlarmCos
#define COMM_ALARM          epicsAlarmComm
#define TIMEOUT_ALARM       epicsAlarmTimeout
#define HW_LIMIT_ALARM      epicsAlarmHwLimit
#define CALC_ALARM          epicsAlarmCalc
#define SCAN_ALARM          epicsAlarmScan
#define LINK_ALARM          epicsAlarmLink
#define SOFT_ALARM          epicsAlarmSoft
#define BAD_SUB_ALARM       epicsAlarmBadSub
#define UDF_ALARM           epicsAlarmUDF
#define DISABLE_ALARM       epicsAlarmDisable
#define SIMM_ALARM          epicsAlarmSimm
#define READ_ACCESS_ALARM   epicsAlarmReadAccess
#define WRITE_ACCESS_ALARM  epicsAlarmWriteAccess
#define lastEpicsAlarmCond  epicsAlarmWriteAccess
/** @} */

/**
 * \brief How to convert an alarm severity into a string
 */
LIBCOM_API extern const char *epicsAlarmSeverityStrings [ALARM_NSEV];
/**
 * \brief How to convert an alarm condition/status into a string
 */
LIBCOM_API extern const char *epicsAlarmConditionStrings [ALARM_NSTATUS];


#ifdef __cplusplus
}
#endif

#endif /* INC_alarm_H */
