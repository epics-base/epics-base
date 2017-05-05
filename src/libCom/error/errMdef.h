/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Error Handling definitions */
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCerrMdefh
#define INCerrMdefh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTN_SUCCESS(STATUS) ((STATUS)==0)

#define M_dbAccess	(501 <<16) /*Database Access Routines */
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
#define M_dbLib         (519 <<16) /*Static Database Access */
#define M_epvxi		(520 <<16) /*VXI Driver*/
#define M_devLib	(521 <<16) /*Device Resource Registration*/
#define M_asLib		(522 <<16) /*Access Security		*/
#define M_cas		(523 <<16) /*CA server*/
#define M_casApp	(524 <<16) /*CA server application*/
#define M_bucket	(525 <<16) /*Bucket Hash*/
#define M_gddFuncTbl	(526 <<16) /*gdd jump table*/
#define M_stdlib        (527 <<16) /*EPICS Standard library*/
#define M_pool          (528 <<16) /*Thread pool*/
#define M_time          (529 <<16) /*epicsTime*/

epicsShareFunc void epicsShareAPI errSymLookup(long status, char *pBuf, unsigned bufLength);
epicsShareFunc const char* errSymMsg(long status);
epicsShareFunc void epicsShareAPI errSymTest(unsigned short modnum, unsigned short begErrNum, unsigned short endErrNum);
epicsShareFunc void epicsShareAPI errSymTestPrint(long errNum);
epicsShareFunc int epicsShareAPI errSymBld(void);
epicsShareFunc int epicsShareAPI errSymbolAdd (long errNum,char *name);
epicsShareFunc void epicsShareAPI errSymDump(void);
epicsShareFunc void epicsShareAPI tstErrSymFind(void);

#ifdef __cplusplus
}
#endif

#endif /*INCerrMdefh*/
