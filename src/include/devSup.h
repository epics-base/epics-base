/* devSup.h	Device Support		*/
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
 * .01  10-04-91        jba     Added error message
 * .02  05-18-92        rcz     Changed macro "GET_DEVSUP("  to "GET_PDEVSUP(precDevSup,"
 * .03  05-18-92        rcz     Structure devSup changed element name from dsetName to papDsetName
 * .04  05-18-92        rcz     New database access
 */

#ifndef INCdevSuph
#define INCdevSuph 1

#ifdef __cplusplus
typedef long (*DEVSUPFUN)(void*);	/* ptr to device support function*/
#else
typedef long (*DEVSUPFUN)();	/* ptr to device support function*/
#endif

struct dset {	/* device support entry table */
	long		number;		/*number of support routines*/
	DEVSUPFUN	report;		/*print report*/
	DEVSUPFUN	init;		/*init support*/
	DEVSUPFUN	init_record;	/*init support for particular record*/
	DEVSUPFUN	get_ioint_info;	/* get io interrupt information*/
	/*other functions are record dependent*/
	};

#define S_dev_noDevSup      (M_devSup| 1) /*SDR_DEVSUP: Device support missing*/
#define S_dev_noDSET        (M_devSup| 3) /*Missing device support entry table*/
#define S_dev_missingSup    (M_devSup| 5) /*Missing device support routine*/
#define S_dev_badInpType    (M_devSup| 7) /*Bad INP link type*/
#define S_dev_badOutType    (M_devSup| 9) /*Bad OUT link type*/
#define S_dev_badInitRet    (M_devSup|11) /*Bad init_rec return value */
#define S_dev_badBus        (M_devSup|13) /*Illegal bus type*/
#define S_dev_badCard       (M_devSup|15) /*Illegal or nonexistant module*/
#define S_dev_badSignal     (M_devSup|17) /*Illegal signal*/
#define S_dev_NoInit        (M_devSup|19) /*No init*/
#define S_dev_Conflict      (M_devSup|21) /*Multiple records accessing same signal*/

#endif
