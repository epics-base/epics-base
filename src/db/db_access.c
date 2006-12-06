/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* db_access.c */
/* base/src/db	$Id$ */
/* db_access.c - Interface between old database access and new */
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */


/***
 *** note:  the order of the DBR_... elements in the "new" structures
 ***	in db_get_field() is important - and must correspond to
 ***	the presumed order in dbAccess.c's dbGetField() routine
 ***/

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "epicsConvert.h"
#include "dbDefs.h"
#include "errlog.h"
#include "ellLib.h"
#include "epicsTime.h"
#include "dbStaticLib.h"
#include "dbBase.h"
#include "dbCommon.h"
#include "errMdef.h"
#include "recSup.h"
#include "alarm.h"
#define db_accessHFORdb_accessC
#include "db_access.h"
#undef db_accessHFORdb_accessC
#define epicsExportSharedSymbols
#include "dbNotify.h"
#include "dbAccessDefs.h"
#include "dbEvent.h"
#include "db_access_routines.h"

#ifndef NULL
#define NULL 0
#endif


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
#define oldDBR_PUT_ACKT		oldDBR_CTRL_DOUBLE + 1
#define oldDBR_PUT_ACKS		oldDBR_PUT_ACKT + 1
#define oldDBR_STSACK_STRING	oldDBR_PUT_ACKS + 1
#define oldDBR_CLASS_NAME	oldDBR_STSACK_STRING + 1

/*Following is defined in dbConvert.h*/
extern unsigned short dbDBRnewToDBRold[DBR_ENUM+1];


/* function declarations */

#ifndef MAX_STRING_SIZE
#define MAX_STRING_SIZE	40
#endif

/* From $cs/dblib/src/dbiocsubs.c
 * subroutines
 *
 *      db_process      process a database record
 *          args
 *              pdb_addr        pointer to a database address
 *              command_mask    mask of operations to perform
 *      new_alarm       send an alarm message (NYI!!!!!)
 *      fill            fill a buffer with a character
 *          args
 *              pbuffer         pointer to the buffer
 *              size            number of characters
 *              fillchar        character with which to fill
 */

/*
 * DB_PROCESS
 *
 * process database records
 */
void db_process(struct dbAddr *pdb_addr)
{
        long    status;

        status=dbProcess((void *)pdb_addr->precord);
        if(status) errMessage(status,"db_process failed");
        return;
}

/*	IOCDBACCESS.C 
 * Global Subroutines
 *
 * db_name_to_addr      converts a database name in the format "pv<.field>"
 *                      to a database address
 *    args
 *      pname           pointer to the database name
 *      paddr           pointer to the database address structure
 *    returns
 *      0               successful
 *      -1              not successful
 *              pv portion failed - node, record type and record number are -1
 *              field portion failed - field type, offset, size and ext are -1
 *
 *
 * db_get_field 	get a field from the database and convert it to
 * 			the specified type.
 *
 *    args
 *	paddr		pointer to the database address structure
 *	buffer_type	the type of data to return
 *			see DBR_ defines in db_access.h
 *	pbuffer		return buffer
 *	no_elements	number of elements
 *      void		*pfl;
 *    returns
 *	0		successful
 *	-1		failed
 *
 * db_put_field 	put a converted buffer into the database
 *    args
 *	paddr		pointer to the database address structure
 *	buffer_type	the type of data to be converted and stored
 *			see DBR_ defines in db_access.h
 *	pbuffer		return buffer
 *    returns
 *	0		successful
 *	-1		failed
 *
 */

/*
 * DB_NAME_TO_ADDR
 */
int epicsShareAPI db_name_to_addr(const char *pname, struct dbAddr *paddr)
{
        long    status;
	short   ftype;

        status=dbNameToAddr(pname, paddr);
        if(!status) {
	    ftype = paddr->dbr_field_type;
	    if(INVALID_DB_REQ(ftype)) {
                errlogPrintf("%s dbNameToAddr failed\n",pname);
		return(-2);
	    }
	    paddr->dbr_field_type = dbDBRnewToDBRold[ftype];
            return(0);
	}
        else
            return(-1);
}

typedef char DBSTRING[MAX_STRING_SIZE];

int epicsShareAPI db_get_field(
struct dbAddr	*paddr,
int		buffer_type,
void		*pbuffer,
int		no_elements,
void		*pfl
)
{
    long status;
    long options;
    long nRequest;
    long i;


    switch(buffer_type) {
    case(oldDBR_STRING):
	{
		DBSTRING *pvalue = (DBSTRING *)pbuffer;

		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_STRING,pbuffer,&options,&nRequest,
			pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i][0] = 0;
	}
	break;
/*  case(oldDBR_INT): */
    case(oldDBR_SHORT):
	{
		dbr_short_t *pvalue = (dbr_short_t *)pbuffer;

		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,pbuffer,&options,&nRequest,
			pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_FLOAT):
	{
		dbr_float_t *pvalue = (dbr_float_t *)pbuffer;

		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,pbuffer,&options,&nRequest,
			pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_ENUM):
	{
		dbr_enum_t *pvalue = (dbr_enum_t *)pbuffer;

		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_ENUM,pbuffer,&options,&nRequest,
			pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_CHAR):
	{
		dbr_char_t *pvalue = (dbr_char_t *)pbuffer;

		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_CHAR,pbuffer,&options,&nRequest,
			pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_LONG):
	{
		dbr_long_t *pvalue = (dbr_long_t *)pbuffer;

		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_LONG,pbuffer,&options,&nRequest,
			pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_DOUBLE):
	{
		dbr_double_t *pvalue = (dbr_double_t *)pbuffer;

		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_DOUBLE,pbuffer,&options,&nRequest,
			pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_STS_STRING):
    case(oldDBR_GR_STRING):
    case(oldDBR_CTRL_STRING):
	{
		struct dbr_sts_string *pold = (struct dbr_sts_string *)pbuffer;
		struct {
			DBRstatus
		} new;
		DBSTRING *pvalue = (DBSTRING *)(pold->value);

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_STRING,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_STRING,&(pold->value[0]),
			&options,&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i][0] = 0;
	}
	break;
/*  case(oldDBR_STS_INT): */
    case(oldDBR_STS_SHORT):
	{
		struct dbr_sts_int *pold = (struct dbr_sts_int *)pbuffer;
		struct {
			DBRstatus
		} new;
		dbr_short_t *pvalue = &pold->value;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_SHORT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_STS_FLOAT):
	{
		struct dbr_sts_float *pold = (struct dbr_sts_float *)pbuffer;
		struct {
			DBRstatus
		} new;
		dbr_float_t *pvalue = &pold->value;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_FLOAT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_STS_ENUM):
	{
		struct dbr_sts_enum *pold = (struct dbr_sts_enum *)pbuffer;
		struct {
			DBRstatus
		} new;
		dbr_enum_t *pvalue = &pold->value;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_ENUM,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_ENUM,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_STS_CHAR):
	{
		struct dbr_sts_char *pold = (struct dbr_sts_char *)pbuffer;
		struct {
			DBRstatus
		} new;
		dbr_char_t *pvalue = &pold->value;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_UCHAR,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_UCHAR,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_STS_LONG):
	{
		struct dbr_sts_long *pold = (struct dbr_sts_long *)pbuffer;
		struct {
			DBRstatus
		} new;
		dbr_long_t *pvalue = &pold->value;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_LONG,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_LONG,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_STS_DOUBLE):
	{
		struct dbr_sts_double *pold = (struct dbr_sts_double *)pbuffer;
		struct {
			DBRstatus
		} new;
		dbr_double_t *pvalue = &pold->value;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_DOUBLE,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_DOUBLE,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_TIME_STRING):
	{
		struct dbr_time_string *pold=(struct dbr_time_string *)pbuffer;
		struct {
			DBRstatus
			DBRtime
		} new;
		DBSTRING *pvalue = (DBSTRING *)(pold->value);

		options=DBR_STATUS | DBR_TIME;
		nRequest=0;
		status = dbGetField(paddr,DBR_STRING,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->stamp = new.time;		/* structure copy */
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_STRING,pold->value,&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i][0] = 0;
	}
	break;
/*  case(oldDBR_TIME_INT): */
    case(oldDBR_TIME_SHORT):
	{
		struct dbr_time_short *pold=(struct dbr_time_short *)pbuffer;
		struct {
			DBRstatus
			DBRtime
		} new;
		dbr_short_t *pvalue = &pold->value;

		options=DBR_STATUS | DBR_TIME;
		nRequest=0;
		status = dbGetField(paddr,DBR_SHORT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->stamp = new.time;		/* structure copy */
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_TIME_FLOAT):
	{
		struct dbr_time_float *pold=(struct dbr_time_float *)pbuffer;
		struct {
			DBRstatus
			DBRtime
		} new;
		dbr_float_t *pvalue = &pold->value;

		options=DBR_STATUS | DBR_TIME;
		nRequest=0;
		status = dbGetField(paddr,DBR_FLOAT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->stamp = new.time;		/* structure copy */
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_TIME_ENUM):
	{
		struct dbr_time_enum *pold=(struct dbr_time_enum *)pbuffer;
		struct {
			DBRstatus
			DBRtime
		} new;
		dbr_enum_t *pvalue = &pold->value;

		options=DBR_STATUS | DBR_TIME;
		nRequest=0;
		status = dbGetField(paddr,DBR_ENUM,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->stamp = new.time;		/* structure copy */
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_ENUM,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_TIME_CHAR):
	{
		struct dbr_time_char *pold=(struct dbr_time_char *)pbuffer;
		struct {
			DBRstatus
			DBRtime
		} new;
		dbr_char_t *pvalue = &pold->value;

		options=DBR_STATUS | DBR_TIME;
		nRequest=0;
		status = dbGetField(paddr,DBR_CHAR,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->stamp = new.time;		/* structure copy */
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_CHAR,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_TIME_LONG):
	{
		struct dbr_time_long *pold=(struct dbr_time_long *)pbuffer;
		struct {
			DBRstatus
			DBRtime
		} new;
		dbr_long_t *pvalue = &pold->value;

		options=DBR_STATUS | DBR_TIME;
		nRequest=0;
		status = dbGetField(paddr,DBR_LONG,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->stamp = new.time;		/* structure copy */
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_LONG,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_TIME_DOUBLE):
	{
		struct dbr_time_double *pold=(struct dbr_time_double *)pbuffer;
		struct {
			DBRstatus
			DBRtime
		} new;
		dbr_double_t *pvalue = &pold->value;

		options=DBR_STATUS | DBR_TIME;
		nRequest=0;
		status = dbGetField(paddr,DBR_DOUBLE,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->stamp = new.time;		/* structure copy */
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_DOUBLE,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_short_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_SHORT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_float_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_PRECISION|DBR_GR_DOUBLE
			|DBR_AL_DOUBLE;
		nRequest=0;
		status = dbGetField(paddr,DBR_FLOAT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->precision = new.precision;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
                pold->upper_disp_limit = epicsConvertDoubleToFloat(new.upper_disp_limit);
                pold->lower_disp_limit = epicsConvertDoubleToFloat(new.lower_disp_limit);
                pold->upper_alarm_limit = epicsConvertDoubleToFloat(new.upper_alarm_limit);
                pold->lower_alarm_limit = epicsConvertDoubleToFloat(new.lower_alarm_limit);
                pold->upper_warning_limit = epicsConvertDoubleToFloat(new.upper_warning_limit);
                pold->lower_warning_limit = epicsConvertDoubleToFloat(new.lower_warning_limit);
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_char_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_UCHAR,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_UCHAR,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_long_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_LONG,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_LONG,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_double_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_PRECISION|DBR_GR_DOUBLE
			|DBR_AL_DOUBLE;
		nRequest=0;
		status = dbGetField(paddr,DBR_DOUBLE,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->precision = new.precision;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_DOUBLE,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_short_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_CTRL_LONG
			|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_SHORT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		pold->upper_ctrl_limit = new.upper_ctrl_limit;
		pold->lower_ctrl_limit = new.lower_ctrl_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_float_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_PRECISION|DBR_GR_DOUBLE
			|DBR_CTRL_DOUBLE|DBR_AL_DOUBLE;
		nRequest=0;
		status = dbGetField(paddr,DBR_FLOAT,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->precision = new.precision;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
                pold->upper_disp_limit = epicsConvertDoubleToFloat(new.upper_disp_limit);
                pold->lower_disp_limit = epicsConvertDoubleToFloat(new.lower_disp_limit);
                pold->upper_alarm_limit = epicsConvertDoubleToFloat(new.upper_alarm_limit);
                pold->lower_alarm_limit = epicsConvertDoubleToFloat(new.lower_alarm_limit);
                pold->upper_warning_limit = epicsConvertDoubleToFloat(new.upper_warning_limit);
                pold->lower_warning_limit = epicsConvertDoubleToFloat(new.lower_warning_limit);
                pold->upper_ctrl_limit = epicsConvertDoubleToFloat(new.upper_ctrl_limit);
                pold->lower_ctrl_limit = epicsConvertDoubleToFloat(new.lower_ctrl_limit);
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_GR_ENUM):
    case(oldDBR_CTRL_ENUM):
	{
		struct dbr_ctrl_enum *pold = (struct dbr_ctrl_enum *)pbuffer;
		struct {
			DBRstatus
			DBRenumStrs
		} new;
		short no_str;
		dbr_enum_t *pvalue = &pold->value;

		memset(pold,'\0',sizeof(struct dbr_ctrl_enum));
		/* first get status and severity */
		options=DBR_STATUS|DBR_ENUM_STRS;
		nRequest=0;
		status = dbGetField(paddr,DBR_ENUM,&new,&options,&nRequest,pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		no_str = new.no_str;
		if(no_str>16) no_str=16;
		pold->no_str = no_str;
		for (i=0; i<no_str; i++) 
			strncpy(pold->strs[i],new.strs[i],
			sizeof(pold->strs[i]));
		/*now get values*/
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_ENUM,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_char_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_CTRL_LONG
			|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_UCHAR,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		pold->upper_ctrl_limit = new.upper_ctrl_limit;
		pold->lower_ctrl_limit = new.lower_ctrl_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_UCHAR,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
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
		} new;
		dbr_long_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_CTRL_LONG
			|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_LONG,&new,&options,&nRequest,pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		pold->upper_ctrl_limit = new.upper_ctrl_limit;
		pold->lower_ctrl_limit = new.lower_ctrl_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_LONG,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_CTRL_DOUBLE):
	{
		struct dbr_ctrl_double *pold=(struct dbr_ctrl_double *)pbuffer;
		struct {
			DBRstatus
			DBRunits
			DBRprecision
			DBRgrDouble
			DBRctrlDouble
			DBRalDouble
		} new;
		dbr_double_t *pvalue = &pold->value;

		options=DBR_STATUS|DBR_UNITS|DBR_PRECISION|DBR_GR_DOUBLE
			|DBR_CTRL_DOUBLE|DBR_AL_DOUBLE;
		nRequest=0;
		status = dbGetField(paddr,DBR_DOUBLE,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->precision = new.precision;
		strncpy(pold->units,new.units,MAX_UNITS_SIZE);
		pold->units[MAX_UNITS_SIZE-1] = '\0';
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		pold->upper_ctrl_limit = new.upper_ctrl_limit;
		pold->lower_ctrl_limit = new.lower_ctrl_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_DOUBLE,&(pold->value),&options,
			&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i] = 0;
	}
	break;
    case(oldDBR_STSACK_STRING):
	{
		struct dbr_stsack_string *pold = (struct dbr_stsack_string *)pbuffer;
		struct {
			DBRstatus
		} new;
		DBSTRING *pvalue = (DBSTRING *)(pold->value);

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_STRING,&new,&options,&nRequest,
			pfl);
		pold->status = new.status;
		pold->severity = new.severity;
		pold->ackt = new.ackt;
		pold->acks = new.acks;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_STRING,&(pold->value[0]),
			&options,&nRequest,pfl);
		for(i=nRequest; i<no_elements; i++) pvalue[i][0] = 0;
	}
	break;
    case(oldDBR_CLASS_NAME):
	{
	    DBENTRY dbEntry;
	    char    *name = 0;
	    char    *pto = (char *)pbuffer;

	    if(!pdbbase) {
		status = S_db_notFound;
		break;
	    }
	    dbInitEntry(pdbbase,&dbEntry);
	    status = dbFindRecord(&dbEntry,paddr->precord->name);
	    if(!status) name = dbGetRecordTypeName(&dbEntry);
	    dbFinishEntry(&dbEntry);
	    if(status) break;
	    if(!name) {
		status = S_dbLib_recordTypeNotFound;
		break;
	    }
	    pto[MAX_STRING_SIZE-1] = 0;
	    strncpy(pto,name,MAX_STRING_SIZE-1);
	}
	break;
    default:
	return(-1);
    }
    if(status) return(-1);
    return(0);
}

/* DB_PUT_FIELD put a field and convert it to the desired type */

int epicsShareAPI db_put_field(
struct dbAddr	*paddr,		/* where to put it */
int		src_type, 
const void	*psrc,		/* where to get it from */
int		no_elements
)
{
    long status;


    switch(src_type) {
    case(oldDBR_STRING):
	status = dbPutField(paddr,DBR_STRING,psrc,(long)no_elements);
	break;
/*  case(oldDBR_INT): */
    case(oldDBR_SHORT):
	status = dbPutField(paddr,DBR_SHORT,psrc,(long)no_elements);
	break;
    case(oldDBR_FLOAT):
	status = dbPutField(paddr,DBR_FLOAT,psrc,(long)no_elements);
	break;
    case(oldDBR_ENUM):
	status = dbPutField(paddr,DBR_ENUM,psrc,(long)no_elements);
	break;
    case(oldDBR_CHAR):
	status = dbPutField(paddr,DBR_UCHAR,psrc,(long)no_elements);
	break;
    case(oldDBR_LONG):
	status = dbPutField(paddr,DBR_LONG,psrc,(long)no_elements);
	break;
    case(oldDBR_DOUBLE):
	status = dbPutField(paddr,DBR_DOUBLE,psrc,(long)no_elements);
	break;
    case(oldDBR_STS_STRING):
	status = dbPutField(paddr,DBR_STRING,
	    ((const struct dbr_sts_string *)psrc)->value,(long)no_elements);
	break;
/*  case(oldDBR_STS_INT): */
    case(oldDBR_STS_SHORT):
	status = dbPutField(paddr,DBR_SHORT,
	    &(((const struct dbr_sts_short *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_STS_FLOAT):
	status = dbPutField(paddr,DBR_FLOAT,
	    &(((const struct dbr_sts_float *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_STS_ENUM):
	status = dbPutField(paddr,DBR_ENUM,
	    &(((const struct dbr_sts_enum *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_STS_CHAR):
	status = dbPutField(paddr,DBR_UCHAR,
	    &(((const struct dbr_sts_char *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_STS_LONG):
	status = dbPutField(paddr,DBR_LONG,
	    &(((const struct dbr_sts_long *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_STS_DOUBLE):
	status = dbPutField(paddr,DBR_DOUBLE,
	    &(((const struct dbr_sts_double *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_TIME_STRING):
	status = dbPutField(paddr,DBR_TIME,
	    ((const struct dbr_time_string *)psrc)->value,(long)no_elements);
	break;
/*  case(oldDBR_TIME_INT): */
    case(oldDBR_TIME_SHORT):
	status = dbPutField(paddr,DBR_SHORT,
	    &(((const struct dbr_time_short *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_TIME_FLOAT):
	status = dbPutField(paddr,DBR_FLOAT,
	    &(((const struct dbr_time_float *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_TIME_ENUM):
	status = dbPutField(paddr,DBR_ENUM,
	    &(((const struct dbr_time_enum *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_TIME_CHAR):
	status = dbPutField(paddr,DBR_UCHAR,
	    &(((const struct dbr_time_char *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_TIME_LONG):
	status = dbPutField(paddr,DBR_LONG,
	    &(((const struct dbr_time_long *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_TIME_DOUBLE):
	status = dbPutField(paddr,DBR_DOUBLE,
	    &(((const struct dbr_time_double *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_GR_STRING):
	/* no struct dbr_gr_string - use dbr_sts_string instead */
	status = dbPutField(paddr,DBR_STRING,
	    ((const struct dbr_sts_string *)psrc)->value,(long)no_elements);
	break;
/*  case(oldDBR_GR_INT): */
    case(oldDBR_GR_SHORT):
	status = dbPutField(paddr,DBR_SHORT,
	    &(((const struct dbr_gr_short *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_GR_FLOAT):
	status = dbPutField(paddr,DBR_FLOAT,
	    &(((const struct dbr_gr_float *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_GR_ENUM):
	status = dbPutField(paddr,DBR_ENUM,
	    &(((const struct dbr_gr_enum *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_GR_CHAR):
	status = dbPutField(paddr,DBR_UCHAR,
	    &(((const struct dbr_gr_char *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_GR_LONG):
	status = dbPutField(paddr,DBR_LONG,
	    &(((const struct dbr_gr_long *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_GR_DOUBLE):
	status = dbPutField(paddr,DBR_DOUBLE,
	    &(((const struct dbr_gr_double *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_CTRL_STRING):
	/* no struct dbr_ctrl_string - use dbr_sts_string instead */
	status = dbPutField(paddr,DBR_STRING,
	    ((const struct dbr_sts_string *)psrc)->value,(long)no_elements);
	break;
/*  case(oldDBR_CTRL_INT): */
    case(oldDBR_CTRL_SHORT):
	status = dbPutField(paddr,DBR_SHORT,
	    &(((const struct dbr_ctrl_short *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_CTRL_FLOAT):
	status = dbPutField(paddr,DBR_FLOAT,
	    &(((const struct dbr_ctrl_float *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_CTRL_ENUM):
	status = dbPutField(paddr,DBR_ENUM,
	    &(((const struct dbr_ctrl_enum *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_CTRL_CHAR):
	status = dbPutField(paddr,DBR_UCHAR,
	    &(((const struct dbr_ctrl_char *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_CTRL_LONG):
	status = dbPutField(paddr,DBR_LONG,
	    &(((const struct dbr_ctrl_long *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_CTRL_DOUBLE):
	status = dbPutField(paddr,DBR_DOUBLE,
	    &(((const struct dbr_ctrl_double *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_PUT_ACKT):
	status = dbPutField(paddr,DBR_PUT_ACKT,psrc,(long)no_elements);
	break;
    case(oldDBR_PUT_ACKS):
	status = dbPutField(paddr,DBR_PUT_ACKS,psrc,(long)no_elements);
	break;

    default:
	return(-1);
    }
    if(status) return(-1);
    return(0);
}


epicsShareFunc int epicsShareAPI dbPutNotifyMapType (putNotify *ppn, short oldtype)
{
    switch(oldtype) {
    case(oldDBR_STRING):
	ppn->dbrType = DBR_STRING;
	break;
/*  case(oldDBR_INT): */
    case(oldDBR_SHORT):
	ppn->dbrType = DBR_SHORT;
	break;
    case(oldDBR_FLOAT):
	ppn->dbrType = DBR_FLOAT;
	break;
    case(oldDBR_ENUM):
	 ppn->dbrType = DBR_ENUM;
	break;
    case(oldDBR_CHAR):
	ppn->dbrType = DBR_UCHAR;
	break;
    case(oldDBR_LONG):
	ppn->dbrType = DBR_LONG;
	break;
    case(oldDBR_DOUBLE):
	 ppn->dbrType = DBR_DOUBLE;
	break;
    case(oldDBR_PUT_ACKT):
         ppn->dbrType = DBR_PUT_ACKT;
        break;
    case(oldDBR_PUT_ACKS):
         ppn->dbrType = DBR_PUT_ACKS;
	break;
    default:
	return(-1);
    }
    return(0);
}
