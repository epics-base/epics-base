/* $Id$
 *
 *      Current Author:         Marty Kraimer
 *      Date:                   03-19-92
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
 * .01  03-19-92        mrk     Original
 * .02  05-18-92        rcz     New database access
 * .03  09-21-92        rcz     removed #include <dbPvd.h>
 */

#ifndef INCdbBaseh
#define INCdbBaseh 1

#ifdef vxWorks
#ifndef INCchoiceh
#include <choice.h>
#endif
#ifndef INCcvtTableh
#include <cvtTable.h>
#endif
#ifndef INCdbDefsh
#include <dbDefs.h>
#endif
#ifndef INCdbRecDesh
#include <dbRecDes.h>
#endif
#ifndef INCdbRecTypeh
#include <dbRecType.h>
#endif
#ifndef INCdbRecordsh
#include <dbRecords.h>
#endif
#ifndef INCdevSuph
#include <devSup.h>
#endif
#ifndef INCdrvSuph
#include <drvSup.h>
#endif
#ifndef INCrecSuph
#include <recSup.h>
#endif
#endif /* vxWorks */

/******************************************************************
 * Note: Functions dbFreeMem() and dbRead() must be kept in sync if
 * struct dbBase changes.
 ******************************************************************/
struct dbBase {
	struct choiceSet        *pchoiceCvt;
	struct arrChoiceSet     *pchoiceGbl;
	struct choiceRec        *pchoiceRec;
	struct devChoiceRec     *pchoiceDev;
	struct arrBrkTable 	*pcvtTable;
	struct recDes 		*precDes;
	struct recType 		*precType;
	struct recHeader 	*precHeader;
	struct recDevSup 	*precDevSup;
	struct drvSup 		*pdrvSup;
	struct recSup 		*precSup;
	struct pvd		*pdbPvd; /* DCT pvd - remove when DCT goes away */
	void			*ppvd;/* pointer to process variable directory*/
	char			*pdbName;/* pointer to database name*/
	struct sdrSum		*psdrSum;/* pointer to default sum */
	long                    sdrFileSize; /*size of default.dctsdr file*/
	long                    pvtSumFlag; /*internal use only*/
};

#endif
