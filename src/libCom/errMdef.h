/* errMdef.h  err.h - Error Handling definitions */
/* share/epicsH $Id$ */
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  mm-dd-yy        iii     Comment
 * .02	12-02-91	jrw	added GPIB and BB message codes
 * .03  03-11-93        joh     added __FILE__ and __LINE__ to errMessage()
 * .04  04-01-93        joh     added vxi
 * .05  04-29-93        joh     added errPrintStatus() func proto
 * .06  09-04-93        rcz     added functions ... for errSymLib.c merge
 * .07  02-03-94	mrk	RTN_SUCCESS is true only if status=0
 * .08  02-03-94	mrk	Add access security library
 * .09  05-23-95	joh	Add bucket hashing library
 */

#ifndef INCerrMdefh
#define INCerrMdefh

#ifdef __cplusplus
extern "C" {
#define errMDefUseProtoANSI
#endif

#ifdef __STDC__
#ifndef errMDefUseProtoANSI
#define errMDefUseProtoANSI
#endif
#endif

#ifdef errMDefUseProtoANSI
#       include <stdarg.h>
#else
#       include <varargs.h>
#endif

#include "ellLib.h"
#include "shareLib.h"

/*The following is only included because before 3.13.0beta12 errMessage */
/*and errPrintf were defined here					*/
#include "errlog.h"

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

#ifdef errMDefUseProtoANSI
epicsShareFunc int epicsShareAPI errSymFind(long status, char *name);
epicsShareFunc int epicsShareAPI UnixSymFind(long status, char *name, long *value);
epicsShareFunc int epicsShareAPI ModSymFind(long status, char *name, long *value);
epicsShareFunc void epicsShareAPI errSymTest(unsigned short modnum, unsigned short begErrNum, unsigned short endErrNum);
epicsShareFunc void epicsShareAPI errSymTestPrint(long errNum);
epicsShareFunc int epicsShareAPI errSymBld();
epicsShareFunc int epicsShareAPI errSymbolAdd (long errNum,char *name);
epicsShareFunc void epicsShareAPI errSymDump();
epicsShareFunc void epicsShareAPI tstErrSymFind();

#else /* errMDefUseProtoANSI */ 

epicsShareFunc void epicsShareAPI errSymTest();
epicsShareFunc int epicsShareAPI errSymFind();
epicsShareFunc int epicsShareAPI UnixSymFind();
epicsShareFunc int epicsShareAPI ModSymFind();
epicsShareFunc void epicsShareAPI errSymTestPrint();
epicsShareFunc int epicsShareAPI errSymBld();
epicsShareFunc int epicsShareAPI errSymbolAdd();
epicsShareFunc void epicsShareAPI errSymDump();
epicsShareFunc void epicsShareAPI tstErrSymFind();
#endif /* errMDefUseProtoANSI */ 

extern int errVerbose;

#ifdef __cplusplus
}
#endif

#endif /*INCerrMdefh*/
