/* dbConvert.h */
/*
 *      Author:	Marty Kraimer
 *      Date:	13OCT95
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
 * .01  13OCT95	mrk	Created header file as part of extracting convert from
 *			dbLink
 */


#include <dbFldTypes.h>
extern long (*dbGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])
    (DBADDR *paddr, void *pbuffer,long nRequest, long no_elements, long offset);
extern long (*dbPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])
    (DBADDR *paddr, void *pbuffer,long nRequest, long no_elements, long offset);
extern long (*dbFastGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])();
extern long (*dbFastPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])();
