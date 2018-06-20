/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Interface between old database access and new
 *
 *      Author:          Marty Kraimer
 *                       Andrew Johnson <anj@aps.anl.gov>
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "alarm.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsConvert.h"
#include "epicsTime.h"
#include "errlog.h"
#include "errMdef.h"

#define db_accessHFORdb_accessC
#include "db_access.h"
#undef db_accessHFORdb_accessC

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "db_access_routines.h"
#include "dbBase.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbEvent.h"
#include "dbLock.h"
#include "dbNotify.h"
#include "dbStaticLib.h"
#include "recSup.h"


#define oldDBF_STRING      0
#define oldDBF_INT         1
#define oldDBF_SHORT       1
#define oldDBF_FLOAT       2
#define oldDBF_ENUM        3
#define oldDBF_CHAR        4
#define oldDBF_LONG        5
#define oldDBF_DOUBLE      6

/* data request buffer types */
#define oldDBR_STRING           oldDBF_STRING
#define oldDBR_INT              oldDBF_INT
#define oldDBR_SHORT            oldDBF_INT
#define oldDBR_FLOAT            oldDBF_FLOAT
#define oldDBR_ENUM             oldDBF_ENUM
#define oldDBR_CHAR             oldDBF_CHAR
#define oldDBR_LONG             oldDBF_LONG
#define oldDBR_DOUBLE           oldDBF_DOUBLE
#define oldDBR_STS_STRING       7
#define oldDBR_STS_INT          8
#define oldDBR_STS_SHORT        8
#define oldDBR_STS_FLOAT        9
#define oldDBR_STS_ENUM         10
#define oldDBR_STS_CHAR         11
#define oldDBR_STS_LONG         12
#define oldDBR_STS_DOUBLE       13
#define oldDBR_TIME_STRING      14
#define oldDBR_TIME_INT         15
#define oldDBR_TIME_SHORT       15
#define oldDBR_TIME_FLOAT       16
#define oldDBR_TIME_ENUM        17
#define oldDBR_TIME_CHAR        18
#define oldDBR_TIME_LONG        19
#define oldDBR_TIME_DOUBLE      20
#define oldDBR_GR_STRING        21
#define oldDBR_GR_INT           22
#define oldDBR_GR_SHORT         22
#define oldDBR_GR_FLOAT         23
#define oldDBR_GR_ENUM          24
#define oldDBR_GR_CHAR          25
#define oldDBR_GR_LONG          26
#define oldDBR_GR_DOUBLE        27
#define oldDBR_CTRL_STRING      28
#define oldDBR_CTRL_INT         29
#define oldDBR_CTRL_SHORT       29
#define oldDBR_CTRL_FLOAT       30
#define oldDBR_CTRL_ENUM        31
#define oldDBR_CTRL_CHAR        32
#define oldDBR_CTRL_LONG        33
#define oldDBR_CTRL_DOUBLE      34
#define oldDBR_PUT_ACKT         oldDBR_CTRL_DOUBLE + 1
#define oldDBR_PUT_ACKS         oldDBR_PUT_ACKT + 1
#define oldDBR_STSACK_STRING    oldDBR_PUT_ACKS + 1
#define oldDBR_CLASS_NAME       oldDBR_STSACK_STRING + 1

typedef char DBSTRING[MAX_STRING_SIZE];

struct dbChannel * dbChannel_create(const char *pname)
{
    dbChannel *chan = dbChannelCreate(pname);

    if (!chan)
        return NULL;

    if (INVALID_DB_REQ(dbChannelExportType(chan)) ||
        dbChannelOpen(chan)) {
        dbChannelDelete(chan);
        return NULL;
    }

    return chan;
}

int dbChannel_get(struct dbChannel *chan,
    int buffer_type, void *pbuffer, long no_elements, void *pfl)
{
    long nRequest = no_elements;
    int result = dbChannel_get_count(
        chan, buffer_type, pbuffer, &nRequest, pfl);
    if (nRequest < no_elements) {
        /* The database request returned fewer elements than requested, so
         * fill the remainder of the buffer with zeros.
         */
        int val_size = dbr_value_size[buffer_type];
        int offset = dbr_size[buffer_type] + (nRequest - 1) * val_size;
        int nbytes = (no_elements - nRequest) * val_size;

        memset((char *)pbuffer + offset, 0, nbytes);
    }
    return result;
}

/* Performs the work of the public db_get_field API, but also returns the number
 * of elements actually copied to the buffer.  The caller is responsible for
 * zeroing the remaining part of the buffer. */
int dbChannel_get_count(
    struct dbChannel *chan, int buffer_type,
    void *pbuffer, long *nRequest, void *pfl)
{
    long status;
    long options;
    long i;
    long zero = 0;

   /* The order of the DBR* elements in the "newSt" structures below is
    * very important and must correspond to the order of processing
    * in the dbAccess.c dbGet() and getOptions() routines.
    */

    dbScanLock(dbChannelRecord(chan));

    switch(buffer_type) {
    case(oldDBR_STRING):
        status = dbChannelGet(chan, DBR_STRING, pbuffer, &zero, nRequest, pfl);
        break;
/*  case(oldDBR_INT): */
    case(oldDBR_SHORT):
        status = dbChannelGet(chan, DBR_SHORT, pbuffer, &zero, nRequest, pfl);
        break;
    case(oldDBR_FLOAT):
        status = dbChannelGet(chan, DBR_FLOAT, pbuffer, &zero, nRequest, pfl);
        break;
    case(oldDBR_ENUM):
        status = dbChannelGet(chan, DBR_ENUM, pbuffer, &zero, nRequest, pfl);
        break;
    case(oldDBR_CHAR):
        status = dbChannelGet(chan, DBR_CHAR, pbuffer, &zero, nRequest, pfl);
        break;
    case(oldDBR_LONG):
        status = dbChannelGet(chan, DBR_LONG, pbuffer, &zero, nRequest, pfl);
        break;
    case(oldDBR_DOUBLE):
        status = dbChannelGet(chan, DBR_DOUBLE, pbuffer, &zero, nRequest, pfl);
        break;

    case(oldDBR_STS_STRING):
    case(oldDBR_GR_STRING):
    case(oldDBR_CTRL_STRING):
        {
            struct dbr_sts_string *pold = (struct dbr_sts_string *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_STRING, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            status = dbChannelGet(chan, DBR_STRING, pold->value, &zero,
                nRequest, pfl);
        }
        break;
/*  case(oldDBR_STS_INT): */
    case(oldDBR_STS_SHORT):
        {
            struct dbr_sts_int *pold = (struct dbr_sts_int *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_SHORT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            status = dbChannelGet(chan, DBR_SHORT, &pold->value, &zero,
                nRequest, pfl);
        }
        break;
    case(oldDBR_STS_FLOAT):
        {
            struct dbr_sts_float *pold = (struct dbr_sts_float *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_FLOAT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            status = dbChannelGet(chan, DBR_FLOAT, &pold->value, &zero,
                nRequest, pfl);
        }
        break;
    case(oldDBR_STS_ENUM):
        {
            struct dbr_sts_enum *pold = (struct dbr_sts_enum *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_ENUM, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            status = dbChannelGet(chan, DBR_ENUM, &pold->value, &zero,
                nRequest, pfl);
        }
        break;
    case(oldDBR_STS_CHAR):
        {
            struct dbr_sts_char *pold = (struct dbr_sts_char *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_UCHAR, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            status = dbChannelGet(chan, DBR_UCHAR, &pold->value, &zero,
                nRequest, pfl);
        }
        break;
    case(oldDBR_STS_LONG):
        {
            struct dbr_sts_long *pold = (struct dbr_sts_long *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_LONG, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            status = dbChannelGet(chan, DBR_LONG, &pold->value, &zero,
                nRequest, pfl);
        }
        break;
    case(oldDBR_STS_DOUBLE):
        {
            struct dbr_sts_double *pold = (struct dbr_sts_double *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_DOUBLE, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            options = 0;
            status = dbChannelGet(chan, DBR_DOUBLE, &pold->value, &options,
                nRequest, pfl);
        }
        break;

    case(oldDBR_TIME_STRING):
        {
            struct dbr_time_string *pold = (struct dbr_time_string *)pbuffer;
            struct {
                DBRstatus
                DBRtime
            } newSt;

            options = DBR_STATUS | DBR_TIME;
            status = dbChannelGet(chan, DBR_STRING, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->stamp = newSt.time;         /* structure copy */
            options = 0;
            status = dbChannelGet(chan, DBR_STRING, pold->value, &options,
                    nRequest, pfl);
        }
        break;
/*  case(oldDBR_TIME_INT): */
    case(oldDBR_TIME_SHORT):
        {
            struct dbr_time_short *pold = (struct dbr_time_short *)pbuffer;
            struct {
                DBRstatus
                DBRtime
            } newSt;

            options = DBR_STATUS | DBR_TIME;
            status = dbChannelGet(chan, DBR_SHORT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->stamp = newSt.time;         /* structure copy */
            options = 0;
            status = dbChannelGet(chan, DBR_SHORT, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_TIME_FLOAT):
        {
            struct dbr_time_float *pold = (struct dbr_time_float *)pbuffer;
            struct {
                DBRstatus
                DBRtime
            } newSt;

            options = DBR_STATUS | DBR_TIME;
            status = dbChannelGet(chan, DBR_FLOAT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->stamp = newSt.time;         /* structure copy */
            options = 0;
            status = dbChannelGet(chan, DBR_FLOAT, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_TIME_ENUM):
        {
            struct dbr_time_enum *pold = (struct dbr_time_enum *)pbuffer;
            struct {
                DBRstatus
                DBRtime
            } newSt;

            options = DBR_STATUS | DBR_TIME;
            status = dbChannelGet(chan, DBR_ENUM, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->stamp = newSt.time;         /* structure copy */
            options = 0;
            status = dbChannelGet(chan, DBR_ENUM, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_TIME_CHAR):
        {
            struct dbr_time_char *pold = (struct dbr_time_char *)pbuffer;
            struct {
                DBRstatus
                DBRtime
            } newSt;

            options = DBR_STATUS | DBR_TIME;
            status = dbChannelGet(chan, DBR_CHAR, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->stamp = newSt.time;         /* structure copy */
            options = 0;
            status = dbChannelGet(chan, DBR_CHAR, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_TIME_LONG):
        {
            struct dbr_time_long *pold = (struct dbr_time_long *)pbuffer;
            struct {
                DBRstatus
                DBRtime
            } newSt;

            options = DBR_STATUS | DBR_TIME;
            status = dbChannelGet(chan, DBR_LONG, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->stamp = newSt.time;         /* structure copy */
            options = 0;
            status = dbChannelGet(chan, DBR_LONG, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_TIME_DOUBLE):
        {
            struct dbr_time_double *pold = (struct dbr_time_double *)pbuffer;
            struct {
                DBRstatus
                DBRtime
            } newSt;

            options = DBR_STATUS | DBR_TIME;
            status = dbChannelGet(chan, DBR_DOUBLE, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->stamp = newSt.time;         /* structure copy */
            options = 0;
            status = dbChannelGet(chan, DBR_DOUBLE, &pold->value, &options,
                nRequest, pfl);
        }
        break;

/*  case(oldDBR_GR_STRING): NOT IMPLEMENTED - use DBR_STS_STRING */
/*  case(oldDBR_GR_INT): */
    case(oldDBR_GR_SHORT):
        {
            struct dbr_gr_int *pold = (struct dbr_gr_int *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRgrLong
                DBRalLong
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_GR_LONG | DBR_AL_LONG;
            status = dbChannelGet(chan, DBR_SHORT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_SHORT, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_GR_FLOAT):
        {
            struct dbr_gr_float *pold = (struct dbr_gr_float *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRprecision
                DBRgrDouble
                DBRalDouble
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_PRECISION | DBR_GR_DOUBLE |
                DBR_AL_DOUBLE;
            status = dbChannelGet(chan, DBR_FLOAT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->precision = (dbr_short_t) newSt.precision.dp;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = epicsConvertDoubleToFloat(newSt.upper_disp_limit);
            pold->lower_disp_limit = epicsConvertDoubleToFloat(newSt.lower_disp_limit);
            pold->upper_alarm_limit = epicsConvertDoubleToFloat(newSt.upper_alarm_limit);
            pold->lower_alarm_limit = epicsConvertDoubleToFloat(newSt.lower_alarm_limit);
            pold->upper_warning_limit = epicsConvertDoubleToFloat(newSt.upper_warning_limit);
            pold->lower_warning_limit = epicsConvertDoubleToFloat(newSt.lower_warning_limit);
            options = 0;
            status = dbChannelGet(chan, DBR_FLOAT, &pold->value, &options,
                nRequest, pfl);
        }
        break;
/*  case(oldDBR_GR_ENUM): see oldDBR_CTRL_ENUM */
    case(oldDBR_GR_CHAR):
        {
            struct dbr_gr_char *pold = (struct dbr_gr_char *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRgrLong
                DBRalLong
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_GR_LONG | DBR_AL_LONG;
            status = dbChannelGet(chan, DBR_UCHAR, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_UCHAR, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_GR_LONG):
        {
            struct dbr_gr_long *pold = (struct dbr_gr_long *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRgrLong
                DBRalLong
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_GR_LONG | DBR_AL_LONG;
            status = dbChannelGet(chan, DBR_LONG, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_LONG, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_GR_DOUBLE):
        {
            struct dbr_gr_double *pold = (struct dbr_gr_double *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRprecision
                DBRgrDouble
                DBRalDouble
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_PRECISION | DBR_GR_DOUBLE |
                DBR_AL_DOUBLE;
            status = dbChannelGet(chan, DBR_DOUBLE, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->precision = (dbr_short_t) newSt.precision.dp;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_DOUBLE, &pold->value, &options,
                nRequest, pfl);
        }
        break;

/*  case(oldDBR_CTRL_INT): */
    case(oldDBR_CTRL_SHORT):
        {
            struct dbr_ctrl_int *pold = (struct dbr_ctrl_int *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRgrLong
                DBRctrlLong
                DBRalLong
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_GR_LONG | DBR_CTRL_LONG |
                DBR_AL_LONG;
            status = dbChannelGet(chan, DBR_SHORT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            pold->upper_ctrl_limit = newSt.upper_ctrl_limit;
            pold->lower_ctrl_limit = newSt.lower_ctrl_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_SHORT, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_CTRL_FLOAT):
        {
            struct dbr_ctrl_float *pold = (struct dbr_ctrl_float *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRprecision
                DBRgrDouble
                DBRctrlDouble
                DBRalDouble
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_PRECISION | DBR_GR_DOUBLE |
                DBR_CTRL_DOUBLE | DBR_AL_DOUBLE;
            status = dbChannelGet(chan, DBR_FLOAT, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->precision = (dbr_short_t) newSt.precision.dp;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = epicsConvertDoubleToFloat(newSt.upper_disp_limit);
            pold->lower_disp_limit = epicsConvertDoubleToFloat(newSt.lower_disp_limit);
            pold->upper_alarm_limit = epicsConvertDoubleToFloat(newSt.upper_alarm_limit);
            pold->lower_alarm_limit = epicsConvertDoubleToFloat(newSt.lower_alarm_limit);
            pold->upper_warning_limit = epicsConvertDoubleToFloat(newSt.upper_warning_limit);
            pold->lower_warning_limit = epicsConvertDoubleToFloat(newSt.lower_warning_limit);
            pold->upper_ctrl_limit = epicsConvertDoubleToFloat(newSt.upper_ctrl_limit);
            pold->lower_ctrl_limit = epicsConvertDoubleToFloat(newSt.lower_ctrl_limit);
            options = 0;
            status = dbChannelGet(chan, DBR_FLOAT, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_GR_ENUM):
    case(oldDBR_CTRL_ENUM):
        {
            struct dbr_ctrl_enum *pold = (struct dbr_ctrl_enum *)pbuffer;
            struct {
                DBRstatus
                DBRenumStrs
            } newSt;
            short no_str;

            memset(pold, '\0', sizeof(struct dbr_ctrl_enum));
            /* first get status and severity */
            options = DBR_STATUS | DBR_ENUM_STRS;
            status = dbChannelGet(chan, DBR_ENUM, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            no_str = newSt.no_str;
            if (no_str>16) no_str=16;
            pold->no_str = no_str;
            for (i = 0; i < no_str; i++)
                strncpy(pold->strs[i], newSt.strs[i], sizeof(pold->strs[i]));
            /*now get values*/
            options = 0;
            status = dbChannelGet(chan, DBR_ENUM, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_CTRL_CHAR):
        {
            struct dbr_ctrl_char *pold = (struct dbr_ctrl_char *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRgrLong
                DBRctrlLong
                DBRalLong
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_GR_LONG | DBR_CTRL_LONG |
                DBR_AL_LONG;
            status = dbChannelGet(chan, DBR_UCHAR, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            pold->upper_ctrl_limit = newSt.upper_ctrl_limit;
            pold->lower_ctrl_limit = newSt.lower_ctrl_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_UCHAR, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_CTRL_LONG):
        {
            struct dbr_ctrl_long *pold = (struct dbr_ctrl_long *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRgrLong
                DBRctrlLong
                DBRalLong
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_GR_LONG | DBR_CTRL_LONG |
                DBR_AL_LONG;
            status = dbChannelGet(chan, DBR_LONG, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            pold->upper_ctrl_limit = newSt.upper_ctrl_limit;
            pold->lower_ctrl_limit = newSt.lower_ctrl_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_LONG, &pold->value, &options,
                nRequest, pfl);
        }
        break;
    case(oldDBR_CTRL_DOUBLE):
        {
            struct dbr_ctrl_double *pold = (struct dbr_ctrl_double *)pbuffer;
            struct {
                DBRstatus
                DBRunits
                DBRprecision
                DBRgrDouble
                DBRctrlDouble
                DBRalDouble
            } newSt;

            options = DBR_STATUS | DBR_UNITS | DBR_PRECISION | DBR_GR_DOUBLE |
                DBR_CTRL_DOUBLE | DBR_AL_DOUBLE;
            status = dbChannelGet(chan, DBR_DOUBLE, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->precision = (dbr_short_t) newSt.precision.dp;
            strncpy(pold->units, newSt.units, MAX_UNITS_SIZE);
            pold->units[MAX_UNITS_SIZE-1] = '\0';
            pold->upper_disp_limit = newSt.upper_disp_limit;
            pold->lower_disp_limit = newSt.lower_disp_limit;
            pold->upper_alarm_limit = newSt.upper_alarm_limit;
            pold->upper_warning_limit = newSt.upper_warning_limit;
            pold->lower_warning_limit = newSt.lower_warning_limit;
            pold->lower_alarm_limit = newSt.lower_alarm_limit;
            pold->upper_ctrl_limit = newSt.upper_ctrl_limit;
            pold->lower_ctrl_limit = newSt.lower_ctrl_limit;
            options = 0;
            status = dbChannelGet(chan, DBR_DOUBLE, &pold->value, &options,
                nRequest, pfl);
        }
        break;

    case(oldDBR_STSACK_STRING):
        {
            struct dbr_stsack_string *pold = (struct dbr_stsack_string *)pbuffer;
            struct {
                DBRstatus
            } newSt;

            options = DBR_STATUS;
            status = dbChannelGet(chan, DBR_STRING, &newSt, &options, &zero, pfl);
            pold->status = newSt.status;
            pold->severity = newSt.severity;
            pold->ackt = newSt.ackt;
            pold->acks = newSt.acks;
            options = 0;
            status = dbChannelGet(chan, DBR_STRING, pold->value,
                &options, nRequest, pfl);
        }
        break;

    case(oldDBR_CLASS_NAME):
        {
            DBENTRY dbEntry;
            char *name = 0;
            char *pto = (char *)pbuffer;

            if (!pdbbase) {
                status = S_db_notFound;
                break;
            }
            dbInitEntry(pdbbase, &dbEntry);
            status = dbFindRecord(&dbEntry, dbChannelRecord(chan)->name);
            if (!status) name = dbGetRecordTypeName(&dbEntry);
            dbFinishEntry(&dbEntry);
            if (status) break;
            if (!name) {
                status = S_dbLib_recordTypeNotFound;
                break;
            }
            pto[MAX_STRING_SIZE-1] = 0;
            strncpy(pto, name, MAX_STRING_SIZE-1);
        }
        break;
    default:
        status = -1;
        break;
    }

    dbScanUnlock(dbChannelRecord(chan));

    if (status) return -1;
    return 0;
}

int dbChannel_put(struct dbChannel *chan, int src_type,
    const void *psrc, long no_elements)
{
    long status;

    switch (src_type) {
    case(oldDBR_STRING):
        status = dbChannelPutField(chan, DBR_STRING, psrc, no_elements);
        break;
/*  case(oldDBR_INT): */
    case(oldDBR_SHORT):
        status = dbChannelPutField(chan, DBR_SHORT, psrc, no_elements);
        break;
    case(oldDBR_FLOAT):
        status = dbChannelPutField(chan, DBR_FLOAT, psrc, no_elements);
        break;
    case(oldDBR_ENUM):
        status = dbChannelPutField(chan, DBR_ENUM, psrc, no_elements);
        break;
    case(oldDBR_CHAR):
        status = dbChannelPutField(chan, DBR_UCHAR, psrc, no_elements);
        break;
    case(oldDBR_LONG):
        status = dbChannelPutField(chan, DBR_LONG, psrc, no_elements);
        break;
    case(oldDBR_DOUBLE):
        status = dbChannelPutField(chan, DBR_DOUBLE, psrc, no_elements);
        break;

    case(oldDBR_STS_STRING):
        status = dbChannelPutField(chan, DBR_STRING,
            ((const struct dbr_sts_string *)psrc)->value, no_elements);
        break;
/*  case(oldDBR_STS_INT): */
    case(oldDBR_STS_SHORT):
        status = dbChannelPutField(chan, DBR_SHORT,
            &((const struct dbr_sts_short *)psrc)->value, no_elements);
        break;
    case(oldDBR_STS_FLOAT):
        status = dbChannelPutField(chan, DBR_FLOAT,
            &((const struct dbr_sts_float *)psrc)->value, no_elements);
        break;
    case(oldDBR_STS_ENUM):
        status = dbChannelPutField(chan, DBR_ENUM,
            &((const struct dbr_sts_enum *)psrc)->value, no_elements);
        break;
    case(oldDBR_STS_CHAR):
        status = dbChannelPutField(chan, DBR_UCHAR,
            &((const struct dbr_sts_char *)psrc)->value, no_elements);
        break;
    case(oldDBR_STS_LONG):
        status = dbChannelPutField(chan, DBR_LONG,
            &((const struct dbr_sts_long *)psrc)->value, no_elements);
        break;
    case(oldDBR_STS_DOUBLE):
        status = dbChannelPutField(chan, DBR_DOUBLE,
            &((const struct dbr_sts_double *)psrc)->value, no_elements);
        break;

    case(oldDBR_TIME_STRING):
        status = dbChannelPutField(chan, DBR_TIME,
            ((const struct dbr_time_string *)psrc)->value, no_elements);
        break;
/*  case(oldDBR_TIME_INT): */
    case(oldDBR_TIME_SHORT):
        status = dbChannelPutField(chan, DBR_SHORT,
            &((const struct dbr_time_short *)psrc)->value, no_elements);
        break;
    case(oldDBR_TIME_FLOAT):
        status = dbChannelPutField(chan, DBR_FLOAT,
            &((const struct dbr_time_float *)psrc)->value, no_elements);
        break;
    case(oldDBR_TIME_ENUM):
        status = dbChannelPutField(chan, DBR_ENUM,
            &((const struct dbr_time_enum *)psrc)->value, no_elements);
        break;
    case(oldDBR_TIME_CHAR):
        status = dbChannelPutField(chan, DBR_UCHAR,
            &((const struct dbr_time_char *)psrc)->value, no_elements);
        break;
    case(oldDBR_TIME_LONG):
        status = dbChannelPutField(chan, DBR_LONG,
            &((const struct dbr_time_long *)psrc)->value, no_elements);
        break;
    case(oldDBR_TIME_DOUBLE):
        status = dbChannelPutField(chan, DBR_DOUBLE,
            &((const struct dbr_time_double *)psrc)->value, no_elements);
        break;

    case(oldDBR_GR_STRING):
        /* no struct dbr_gr_string - use dbr_sts_string instead */
        status = dbChannelPutField(chan, DBR_STRING,
            ((const struct dbr_sts_string *)psrc)->value, no_elements);
        break;
/*  case(oldDBR_GR_INT): */
    case(oldDBR_GR_SHORT):
        status = dbChannelPutField(chan, DBR_SHORT,
            &((const struct dbr_gr_short *)psrc)->value, no_elements);
        break;
    case(oldDBR_GR_FLOAT):
        status = dbChannelPutField(chan, DBR_FLOAT,
            &((const struct dbr_gr_float *)psrc)->value, no_elements);
        break;
    case(oldDBR_GR_ENUM):
        status = dbChannelPutField(chan, DBR_ENUM,
            &((const struct dbr_gr_enum *)psrc)->value, no_elements);
        break;
    case(oldDBR_GR_CHAR):
        status = dbChannelPutField(chan, DBR_UCHAR,
            &((const struct dbr_gr_char *)psrc)->value, no_elements);
        break;
    case(oldDBR_GR_LONG):
        status = dbChannelPutField(chan, DBR_LONG,
            &((const struct dbr_gr_long *)psrc)->value, no_elements);
        break;
    case(oldDBR_GR_DOUBLE):
        status = dbChannelPutField(chan, DBR_DOUBLE,
            &((const struct dbr_gr_double *)psrc)->value, no_elements);
        break;

    case(oldDBR_CTRL_STRING):
        /* no struct dbr_ctrl_string - use dbr_sts_string instead */
        status = dbChannelPutField(chan, DBR_STRING,
            ((const struct dbr_sts_string *)psrc)->value, no_elements);
        break;
/*  case(oldDBR_CTRL_INT): */
    case(oldDBR_CTRL_SHORT):
        status = dbChannelPutField(chan, DBR_SHORT,
            &((const struct dbr_ctrl_short *)psrc)->value, no_elements);
        break;
    case(oldDBR_CTRL_FLOAT):
        status = dbChannelPutField(chan, DBR_FLOAT,
            &((const struct dbr_ctrl_float *)psrc)->value, no_elements);
        break;
    case(oldDBR_CTRL_ENUM):
        status = dbChannelPutField(chan, DBR_ENUM,
            &((const struct dbr_ctrl_enum *)psrc)->value, no_elements);
        break;
    case(oldDBR_CTRL_CHAR):
        status = dbChannelPutField(chan, DBR_UCHAR,
            &((const struct dbr_ctrl_char *)psrc)->value, no_elements);
        break;
    case(oldDBR_CTRL_LONG):
        status = dbChannelPutField(chan, DBR_LONG,
            &((const struct dbr_ctrl_long *)psrc)->value, no_elements);
        break;
    case(oldDBR_CTRL_DOUBLE):
        status = dbChannelPutField(chan, DBR_DOUBLE,
            &((const struct dbr_ctrl_double *)psrc)->value, no_elements);
        break;

    case(oldDBR_PUT_ACKT):
        status = dbChannelPutField(chan, DBR_PUT_ACKT, psrc, no_elements);
        break;
    case(oldDBR_PUT_ACKS):
        status = dbChannelPutField(chan, DBR_PUT_ACKS, psrc, no_elements);
        break;

    default:
        return -1;
    }
    if (status) return -1;
    return 0;
}


static int mapOldType (short oldtype)
{
    int dbrType = -1;

    switch (oldtype) {
    case oldDBR_STRING:
        dbrType = DBR_STRING;
        break;
/*  case oldDBR_INT: */
    case oldDBR_SHORT:
        dbrType = DBR_SHORT;
        break;
    case oldDBR_FLOAT:
        dbrType = DBR_FLOAT;
        break;
    case oldDBR_ENUM:
        dbrType = DBR_ENUM;
        break;
    case oldDBR_CHAR:
        dbrType = DBR_UCHAR;
        break;
    case oldDBR_LONG:
        dbrType = DBR_LONG;
        break;
    case oldDBR_DOUBLE:
        dbrType = DBR_DOUBLE;
        break;
    case oldDBR_PUT_ACKT:
        dbrType = DBR_PUT_ACKT;
        break;
    case oldDBR_PUT_ACKS:
        dbrType = DBR_PUT_ACKS;
        break;
    default:
        return -1;
    }
    return dbrType;
}

int db_put_process(processNotify *ppn, notifyPutType type,
    int src_type, const void *psrc, int no_elements)
{
    int status = 0;
    int dbrType =  mapOldType(src_type);
    switch(type) {
    case putDisabledType:
        ppn->status = notifyError;
        return 0;
    case putFieldType:
        status = dbChannelPutField(ppn->chan, dbrType, psrc, no_elements);
        break;
    case putType:
        status = dbChannelPut(ppn->chan, dbrType, psrc, no_elements);
        break;
    }
    if (status)
        ppn->status = notifyError;
    return 1;
}
