/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* errMdef.h  err.h - Error Handling definitions */
/* share/epicsH $Id$ */
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 *
 */

#ifndef INCerrMdefh
#define INCerrMdefh

#ifdef __STDC__
#define errMDefUseFuncProto
#endif

#ifdef __cplusplus
#ifndef errMDefUseFuncProto
#define errMDefUseFuncProto
#endif
#endif

#ifdef errMDefUseFuncProto
#       include <stdarg.h>
#else
#       include <varargs.h>
#endif
#include <ellLib.h>

#define RTN_SUCCESS(STATUS) ((STATUS)==0)

#define M_dbAccess	(501 <<16) /*Database Access Routines */
#define M_sdr		(502 <<16) /*Self Defining Records */
#define M_drvSup	(503 <<16) /*Driver Support*/
#define M_devSup	(504 <<16) /*Device Support*/
#define M_recSup	(505 <<16) /*Record Support*/
#define M_recType	(506 <<16) /*Record Type*/
#define M_record	(507 <<16) /*Database Records*/
#define M_ar		(508 <<16) /*Archiver; see arDefs.h*/
#define M_ts            (509 <<16) /*Time Stamp Routines; see tsDefs.h*/
#define M_arAcc         (510 <<16) /*Archive Access Library Routines*/
#define M_bf            (511 <<16) /*Block File Routines; see bfDefs.h*/
#define M_syd           (512 <<16) /*Sync Data Routines; see sydDefs.h*/
#define M_ppr           (513 <<16) /*Portable Plot Routines; see pprPlotDefs.h*/
#define M_env           (514 <<16) /*Environment Routines; see envDefs.h*/
#define M_gen           (515 <<16) /*General Purpose Routines; see genDefs.h*/
#define	M_gpib		(516 <<16) /*Gpib driver & device support; see drvGpibInterface.h*/
#define	M_bitbus	(517 <<16) /*Bitbus driver & device support; see drvBitBusInterface.h*/
#define M_dbCa          (518 <<16) /*CA_LINKs; see calink.h*/
#define M_dbLib         (519 <<16) /*Static Database Access */
#define M_epvxi		(520 <<16) /*VXI Driver*/
#define M_devLib	(521 <<16) /*Device Resource Registration*/
#define M_asLib		(522 <<16) /*Access Security		*/
#define M_cas		(523 <<16) /*CA server*/
#define M_casApp	(524 <<16) /*CA server application*/
#define M_bucket	(525 <<16) /*Bucket Hash*/
#define M_sbuf		(526 <<16) /*Shared Buffer*/


/*
 * redefine errMessage with a macro so we can print 
 * the file and line number
 */
#define errMessage(S, PM) \
         errPrintf(S, __FILE__, __LINE__, PM)

#ifdef errMDefUseFuncProto
int errSymFind(long status, char *name);
int UnixSymFind(long status, char *name, long *value);
int ModSymFind(long status, char *name, long *value);
void errSymTest(unsigned short modnum, unsigned short begErrNum, unsigned short endErrNum);
void errSymTestPrint(long errNum);
int errSymBld();
int errSymbolAdd (long errNum,char *name);
void errPrintf(long status, const char *pFileName, 
	int lineno, const char *pformat, ...);
void errSymDump();
void tstErrSymFind();
void errInit(void);

#else /*errMDefUseFuncProto*/

void errSymTest();
int errSymFind();
int UnixSymFind();
int ModSymFind();
void errSymTestPrint();
int errSymBld();
int errSymbolAdd();
void errPrintf();
void errSymDump();
void tstErrSymFind();
#endif /*errMDefUseFuncProto*/

extern int errVerbose;

#endif /*INCerrMdefh*/
