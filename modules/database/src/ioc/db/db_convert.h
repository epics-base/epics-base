/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* db_convert.h */

#ifndef INCLdb_converth
#define INCLdb_converth

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"
#include "dbAddr.h"

epicsShareExtern struct dbBase *pdbbase;
epicsShareExtern volatile int interruptAccept;

/*Definitions that allow old database access to use new conversion routines*/
#define newDBF_DEVICE 13
#define newDBR_ENUM    11
epicsShareExtern long (*dbGetConvertRoutine[newDBF_DEVICE+1][newDBR_ENUM+1])
    (struct dbAddr *paddr, void *pbuffer,long nRequest,
        long no_elements, long offset);
epicsShareExtern long (*dbPutConvertRoutine[newDBR_ENUM+1][newDBF_DEVICE+1])
    (struct dbAddr *paddr, const void *pbuffer,long nRequest,
        long no_elements, long offset);
epicsShareExtern long (*dbFastGetConvertRoutine[newDBF_DEVICE+1][newDBR_ENUM+1])
    (const void *from, void *to, dbAddr *paddr);
epicsShareExtern long (*dbFastPutConvertRoutine[newDBR_ENUM+1][newDBF_DEVICE+1])
    (const void *from, void *to, dbAddr *paddr);

/*Conversion between old and new DBR types*/
epicsShareExtern unsigned short dbDBRoldToDBFnew[DBR_DOUBLE+1];
epicsShareExtern unsigned short dbDBRnewToDBRold[newDBR_ENUM+1];
#ifdef DB_CONVERT_GBLSOURCE
epicsShareDef unsigned short dbDBRoldToDBFnew[DBR_DOUBLE+1] = {
	0, /*DBR_STRING to DBF_STRING*/
	3, /*DBR_INT to DBF_SHORT*/
	9, /*DBR_FLOAT to DBF_FLOAT*/
	11, /*DBR_ENUM to DBF_ENUM*/
	1, /*DBR_CHAR to DBF_CHAR*/
	5, /*DBR_LONG to DBF_LONG*/
	10  /*DBR_DOUBLE to DBF_DOUBLE*/
};
epicsShareDef unsigned short dbDBRnewToDBRold[newDBR_ENUM+1] = {
	0, /*DBR_STRING to DBR_STRING*/
	4, /*DBR_CHAR to DBR_CHAR*/
	4, /*DBR_UCHAR to DBR_CHAR*/
	1, /*DBR_SHORT to DBR_SHORT*/
	5, /*DBR_USHORT to DBR_LONG*/
	5, /*DBR_LONG to DBR_LONG*/
	6, /*DBR_ULONG to DBR_DOUBLE*/
	6, /*DBR_INT64 to DBR_DOUBLE*/
	6, /*DBR_UINT64 to DBR_DOUBLE*/
	2, /*DBR_FLOAT to DBR_FLOAT*/
	6, /*DBR_DOUBLE to DBR_DOUBLE*/
	3, /*DBR_ENUM to DBR_ENUM*/
};
#endif /*DB_CONVERT_GBLSOURCE*/


#ifdef __cplusplus
}
#endif

#endif /* INCLdb_converth */
