/* share/src/db $Id$ */
/*
 *      Author:		Marty Kraimer
 *      Date:		06-01-91
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
 * .01  09-05-92	rcz	initial version
 * .02  09-10-92	rcz	changed completely
 *
 */


#ifndef INCinitHooksh
#define INCinitHooksh 1

#define INITHOOKatBeginning		0
#define INITHOOKafterGetResources	1
#define INITHOOKafterLogInit		2
#define INITHOOKafterCallbackInit	3
#define INITHOOKafterCaLinkInit		4
#define INITHOOKafterInitDrvSup		5
#define INITHOOKafterInitRecSup		6
#define INITHOOKafterInitDevSup		7
#define INITHOOKafterTS_init		8
#define INITHOOKafterInitDatabase	9
#define INITHOOKafterFinishDevSup	10
#define INITHOOKafterScanInit		11
#define INITHOOKafterInterruptAccept	12
#define INITHOOKafterInitialProcess	13
#define INITHOOKatEnd			14

#endif /*INCinitHooksh*/
