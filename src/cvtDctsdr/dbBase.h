/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *
 *      Current Author:         Marty Kraimer
 *      Date:                   03-19-92
 *
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
