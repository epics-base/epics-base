/*   @(#)epicsEnvParams.c	1.5        6/11/93
 *      Author: Roger A. Cole
 #      Date:   07-20-91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	mm-dd-yy	iii	Comment
 * .02	07-29-92	rcz	Put under sccs in site directory
 * .03	09-14-92	jba	Changed EPICS_TS_MIN_WEST to 300
 * .04	09-18-92	jba	Changed EPICS_TS_MIN_WEST back to 360
 * 	...
 */
#include <envDefs.h>

epicsSetEnvParams()
{
    printf("setting EPICS environment parameters\n");
    envSetConfigParam(&EPICS_TS_MIN_WEST, "480");
    envSetConfigParam(&EPICS_SYSCLK_INET, "128.3.112.85");
    envSetConfigParam(&EPICS_IOCMCLK_INET, "131.243.192.31");
    envSetConfigParam(&EPICS_SYSCLK_PORT, "2200");
    envSetConfigParam(&EPICS_IOCMCLK_PORT, "2300");
    envSetConfigParam(&EPICS_AR_PORT, "7002");
    envSetConfigParam(&EPICS_CMD_PROTO_PORT, "7003");
    envSetConfigParam(&EPICS_IOC_LOG_INET, "128.3.112.85");
    envSetConfigParam(&EPICS_IOC_LOG_PORT, "7004");
    envSetConfigParam(&EPICS_IOC_LOG_FILE_LIMIT, "400000");
    envSetConfigParam(&EPICS_IOC_LOG_FILE_NAME, "/home/dfs/epics/apple/log/iocLog.text");
    return 0;
}
epicsPrtEnvParams()
{
    envPrtConfigParam(&EPICS_TS_MIN_WEST);
    envPrtConfigParam(&EPICS_SYSCLK_INET);
    envPrtConfigParam(&EPICS_IOCMCLK_INET);
    envPrtConfigParam(&EPICS_SYSCLK_PORT);
    envPrtConfigParam(&EPICS_IOCMCLK_PORT);
    envPrtConfigParam(&EPICS_AR_PORT);
    envPrtConfigParam(&EPICS_CMD_PROTO_PORT);
    envPrtConfigParam(&EPICS_IOC_LOG_INET);
    envPrtConfigParam(&EPICS_IOC_LOG_PORT);
    envPrtConfigParam(&EPICS_IOC_LOG_FILE_LIMIT);
    envPrtConfigParam(&EPICS_IOC_LOG_FILE_NAME);
    return 0;
}
