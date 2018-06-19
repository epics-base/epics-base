/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */
#ifndef INCdbFldTypesh
#define INCdbFldTypesh 1

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* field types */
typedef enum {
	DBF_STRING,
	DBF_CHAR,
	DBF_UCHAR,
	DBF_SHORT,
	DBF_USHORT,
	DBF_LONG,
	DBF_ULONG,
	DBF_INT64,
	DBF_UINT64,
	DBF_FLOAT,
	DBF_DOUBLE,
	DBF_ENUM,
	DBF_MENU,
	DBF_DEVICE,
	DBF_INLINK,
	DBF_OUTLINK,
	DBF_FWDLINK,
	DBF_NOACCESS
}dbfType;
#define DBF_NTYPES DBF_NOACCESS+1

typedef struct mapdbfType{
	char	*strvalue;
	dbfType	value;
}mapdbfType;

epicsShareExtern mapdbfType pamapdbfType[];
#ifdef DBFLDTYPES_GBLSOURCE
epicsShareDef mapdbfType pamapdbfType[DBF_NTYPES] = {
	{"DBF_STRING",DBF_STRING},
	{"DBF_CHAR",DBF_CHAR},
	{"DBF_UCHAR",DBF_UCHAR},
	{"DBF_SHORT",DBF_SHORT},
	{"DBF_USHORT",DBF_USHORT},
	{"DBF_LONG",DBF_LONG},
	{"DBF_ULONG",DBF_ULONG},
	{"DBF_INT64",DBF_INT64},
	{"DBF_UINT64",DBF_UINT64},
	{"DBF_FLOAT",DBF_FLOAT},
	{"DBF_DOUBLE",DBF_DOUBLE},
	{"DBF_ENUM",DBF_ENUM},
	{"DBF_MENU",DBF_MENU},
	{"DBF_DEVICE",DBF_DEVICE},
	{"DBF_INLINK",DBF_INLINK},
	{"DBF_OUTLINK",DBF_OUTLINK},
	{"DBF_FWDLINK",DBF_FWDLINK},
	{"DBF_NOACCESS",DBF_NOACCESS}
};
#endif /*DBFLDTYPES_GBLSOURCE*/

/* data request buffer types */
#define DBR_STRING      DBF_STRING
#define DBR_CHAR        DBF_CHAR
#define DBR_UCHAR       DBF_UCHAR
#define DBR_SHORT       DBF_SHORT
#define DBR_USHORT      DBF_USHORT
#define DBR_LONG        DBF_LONG
#define DBR_ULONG       DBF_ULONG
#define DBR_INT64       DBF_INT64
#define DBR_UINT64      DBF_UINT64
#define DBR_FLOAT       DBF_FLOAT
#define DBR_DOUBLE      DBF_DOUBLE
#define DBR_ENUM        DBF_ENUM
#define DBR_PUT_ACKT	DBR_ENUM+1
#define DBR_PUT_ACKS	DBR_PUT_ACKT+1
#define DBR_NOACCESS    DBF_NOACCESS
#define VALID_DB_REQ(x) ((x >= 0) && (x <= DBR_ENUM))
#define INVALID_DB_REQ(x)       ((x < 0) || (x > DBR_ENUM))

#ifdef __cplusplus
}
#endif

#endif /*INCdbFldTypesh*/
