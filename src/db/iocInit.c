/* iocInit.c	ioc initialization */ 
/* base/src/db $Id$ */
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
 * .01  07-20-91	rac	print release data; set env params
 * .02	08-06-91	mrk	parm string length test changed to warning
 * .03  08-09-91        joh     added ioc log client init
 * .04  09-10-91        joh     moved VME stuff from here to initVme()
 * .05  09-10-91        joh     printf() -> logMsg()
 * .06  09-10-91        joh     print message only on failure
 * .07  08-30-91	rcz	completed .02 fix
 * .08  10-10-91        rcz     changed getResources to accomodate EPICS_
 *                              parameters in a structure (first try)
 * .09  12-02-91        mrk     Added finishDevSup 
 * .10  02-10-92        jba     Changed error messages
 * .11  02-28-92        jba     ANSI C changes
 * .12  03-26-92        mrk     changed test if(status) to if(rtnval)
 * .13  04-17-92        rcz     changed sdrLoad to dbRead
 * .14	04-17-92	mrk	Added wait before interruptAccept
 * .15	05-17-92	rcz	moved sdrSum stuff to dbReadWrite.c
 * .16	05-19-92	mrk	Changes for internal database structure changes
 * .17	06-16-92	jba	Added prset test to call of init_record second time loop
 * .18	07-31-92	rcz	moved database loading to function dbLoad
 * .19	08-14-92	jba	included dblinks with maximize severity in lockset
 * .20	08-27-92	mrk	removed wakeup_init (For old I/O Event scanning)
 * .21	09-05-92	rcz	changed dbUserExit to initHooks
 * .22	09-10-92	rcz	added many initHooks - INITHOOK*<place> argument
 * .23	09-10-92	rcz	changed funcptr pinitHooks from ret long to void 
 * .24	09-11-92	rcz	moved setMasterTimeToSelf to a seperate C file
 * .25	07-15-93	mrk	Changed dbLoad for new dbStaticLib support
<<<<<<< iocInit.c
 * .26	02-09-94	jbk	changed to new time stamp support software ts_init()
 * .27	03-18-94	mcn	added comments
 * .28	03-23-94	mrk	Added asInit
 * .29	04-04-94	mcn	added code for uninitialized conversions (link conversion field)
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<string.h>
#include	<ellLib.h>
#include	<sysLib.h>
#include	<symLib.h>
#include	<sysSymTbl.h>	/* for sysSymTbl*/
#include	<a_out.h>	/* for N_TEXT */
#include	<logLib.h>
#include	<taskLib.h>

#include	<sdrHeader.h>
#include	<fast_lock.h>
#include	<choice.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include	<taskwd.h>
#include	<callback.h>
#include	<rec/dbCommon.h>
#include	<dbBase.h>
#include	<dbFldTypes.h>
#include 	<dbRecDes.h>
#include 	<dbRecType.h>
#include	<dbRecords.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<envDefs.h>
#include	<dbStaticLib.h>
#include	<initHooks.h>

/*This module will declare and initilize module_type variables*/
#define MODULE_TYPES_INIT 1
#include        <module_types.h>

static initialized=FALSE;

/* The following is for use by interrupt routines */
int interruptAccept=FALSE;

struct dbBase *pdbBase=NULL;

/* added for Channel Access Links */
long dbCommonInit();

/* define forward references*/
static long initDrvSup(void);
static long initRecSup(void);
static long initDevSup(void);
static long finishDevSup(void);
static long initDatabase(void);
static void createLockSets(void);
static short makeSameSet(struct dbAddr *paddr,short set);
static long initialProcess(void);
static long getResources(char  *fname);
void setMasterTimeToSelf(void);

/*
 *  Initialize EPICS on the IOC.
 */
int iocInit(char * pResourceFilename)
{
    long status;
    char name[40];
    long rtnval;
    void (*pinitHooks)() = NULL;
    SYM_TYPE type;

    if (initialized) {
	logMsg("iocInit can only be called once\n",0,0,0,0,0,0);
	return(-1);
    }

    if (!pdbBase) {
	logMsg("iocInit aborting because No database loaded by dbLoad\n",0,0,0,0,0,0);
	return(-1);
    }

   /*
    *  Build the data structure for error reporting
    */
    if ((errSymBld()) != 0) {
	logMsg("iocInit aborting because errSymBld failed to initialize \n",0,0,0,0,0,0);
	return(-1);
    }

   /*
    *  Setup initialization hooks, but only if an initHooks routine has been defined.
    */
    strcpy(name, "_");
    strcat(name, "initHooks");
    rtnval = symFindByName(sysSymTbl, name, (void *) &pinitHooks, &type);

   /*
    *  Check to see if initHooks defined, and make sure it is a TEXT symbol (code)
    *    rather than any other kind of vxWorks symbol type.
    */
    if (rtnval == OK && !((type & N_TEXT) != 0)) {
	logMsg("iocInit - WARNING symbol initHooks has wrong type - skipping all init hooks\n",0,0,0,0,0,0);
	pinitHooks=NULL;
    }

   /* Call the user-defined initialization hook before anything else is done */
    if (pinitHooks) (*pinitHooks)(INITHOOKatBeginning);

   /*
    *  Print out version of iocCore
    */
    coreRelease();

   /*
    *  Read EPICS environment
    */
    epicsSetEnvParams();

   /* Hook after environment has been set */
    if (pinitHooks) (*pinitHooks)(INITHOOKafterSetEnvParams);

   /*
    *  Read EPICS resources.
    */
    status = getResources(pResourceFilename);
    if (status != 0) {
	logMsg("iocInit aborting because getResources failed\n",0,0,0,0,0,0);
	return(-1);
    }

   /* Call hook for after resources are read. */
    if (pinitHooks) (*pinitHooks)(INITHOOKafterGetResources);

   /* Initialize log client, and call hook */
    status = iocLogInit();
    if (status!=0) {
        logMsg("iocInit Failed to Initialize Ioc Log Client \n",0,0,0,0,0,0);
    }
    if (pinitHooks) (*pinitHooks)(INITHOOKafterLogInit);


   /*
    *  After this point, further calls to iocInit() are disallowed.
    */
    initialized = TRUE;

   /*
    *  Initialize the watchdog task.  These is different from vxWorks watchdogs,
    *    this task is assigned to watching and reporting if the vxWorks tasks
    *    (that it has been told to watch) are suspended.
    */
    taskwdInit();

   /*
    *  Initialize EPICS callback tasks
    */
    callbackInit();

   /*
    *  Wait 1/10 second to make sure that the above initializations
    *    are complete.
    */
    (void)taskDelay(sysClkRateGet()/10);

   /* Hook after callbacks are initialized */
    if (pinitHooks) (*pinitHooks)(INITHOOKafterCallbackInit);

   /*
    *  Initialize Channel Access Link mechanism.  Pass #1
    *     Call hook after CA links are initialized
    */
    dbCaLinkInit((int) 1);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterCaLinkInit1);

   /*
    *  Initialize Driver Support, Record Support, and finally Device Support.
    */
    if (initDrvSup() != 0)
         logMsg("iocInit: Drivers Failed during Initialization\n",0,0,0,0,0,0);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitDrvSup);

    if (initRecSup() != 0)
         logMsg("iocInit: Record Support Failed during Initialization\n",0,0,0,0,0,0);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitRecSup);

    if (initDevSup() != 0)
         logMsg("iocInit: Device Support Failed during Initialization\n",0,0,0,0,0,0);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitDevSup);

   /*
    *  Initialize the time stamp code.  This starts up the time-stamp task.
    */
    /* ts_init(); */ /* old time stamp driver (jbk) */
    TSinit(); /* new time stamp driver (jbk) */

    if (pinitHooks) (*pinitHooks)(INITHOOKafterTS_init);

   /*
    *  First pass at initializing the database structure
    */
    if (initDatabase() != 0)
         logMsg("iocInit: Database Failed during Initialization\n",0,0,0,0,0,0);

    createLockSets();

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitDatabase);

   /* added for Channel Access Links */
    dbCaLinkInit((int) 2);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterCaLinkInit2);

   /*
    *  Finish initializing device support
    */
    if (finishDevSup() != 0)
         logMsg("iocInit: Device Support Failed during Finalization\n",0,0,0,0,0,0);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterFinishDevSup);

   /*
    *  Initialize Scan tasks
    */
    scanInit();

   /* wait 1/2 second to make sure all tasks are started*/

   /* initialize access security */
    asInit();

    (void)taskDelay(sysClkRateGet()/2);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterScanInit);

   /*
    *  Enable scan tasks and some driver support functions.
    */
    interruptAccept=TRUE;

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInterruptAccept);

   /*
    *  Process all records that have their "process at initialization"
    *      field set (pini).
    */
    if (initialProcess() != 0)
          logMsg("iocInit: initialProcess Failed\n",0,0,0,0,0,0);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitialProcess);

   /*
    *  Start up CA server
    */
    rsrv_init();

    logMsg("iocInit: All initialization complete\n",0,0,0,0,0,0);

    if (pinitHooks) (*pinitHooks)(INITHOOKatEnd);

    return(0);
}

/*
 *  Initialize Driver Support
 *      Locate all driver support entry tables.
 *      Call the initialization routine (init) for each
 *        driver type.
 */
static long initDrvSup(void) /* Locate all driver support entry tables */
{
    char	*pname;
    char	name[40];
    int		i;
    SYM_TYPE	type;
    char	message[100];
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    struct drvSup *pdrvSup;

   /*
    *  Make sure at least one device driver is defined in
    *    the ASCII file.
    */
    
    if (!(pdrvSup=pdbBase->pdrvSup)) {
	status = S_drv_noDrvSup;
	errMessage(status,"No device drivers are defined");
	return(status);
    }

   /*
    *  For every driver support module, look up the name
    *    for that function in the vxWorks symbol table.  If
    *    a driver entry table doesn't exist, report that
    *    fact.
    */
    for(i=0; i< (pdrvSup->number); i++) {
	if (!(pname = pdrvSup->papDrvName[i])) continue;

	strcpy(name,"_");
	strcat(name,pname);

	vxstatus = symFindByName(sysSymTbl, name, (void *) &(pdrvSup->papDrvet[i]), &type);

       /* Make sure it is program text */
	if (vxstatus != OK || (type & N_TEXT == 0)) {
	    strcpy(message,"driver entry table not found for ");
	    strcat(message,pname);
	    status = S_drv_noDrvet;
	    errMessage(status,message);
	    continue;
	}

       /*
        *   If an initialization routine is defined (not NULL),
        *      for the driver support call it.
        */
	if (!(pdrvSup->papDrvet[i]->init))
            continue;
	rtnval = (*(pdrvSup->papDrvet[i]->init))();

	if (status == 0)
            status = rtnval;
    }
    return(status);
}

/*
 *  Initialize Record Support
 *      Allocate a record support structure for every record type
 *        plus space for an array of pointers to RSETs.
 *      Locate all record support entry tables.
 *      Call the initialization routine (init) for each
 *        record type.
 */
static long initRecSup(void)
{
    char	name[40];
    int		i;
    SYM_TYPE	type;
    char	message[100];
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    int		nbytes;
    struct recType *precType;
    struct recSup  *precSup;
    
    if(!(precType=pdbBase->precType)) {
	status = S_rectype_noRecs;
	errMessage(status,"No record types defined");
	return(status);
    }

   /*
    *  Allocate record support structure, and a list of pointers
    *     after it to point to the Record Support Entry Tables (RSETs)
    *     of individual record types.
    */
    nbytes = sizeof(struct recSup) + precType->number * sizeof(void *);
    precSup = dbCalloc(1,nbytes);
    pdbBase->precSup = precSup;

   /*  The number of record support entry tables equals the number of record types */
    precSup->number = precType->number;

   /*
    *  The pointer to the array of pointers of Record Support Entry Tables
    *    exists at the end of the record support structure.
    */
    precSup->papRset = (void *)((long)precSup + (long)sizeof(struct recSup));

   /*
    *  For every record support module, look up the name
    *    for that function in the vxWorks symbol table.  If
    *    a record support entry table doesn't exist, report
    *    that fact.
    */
    for(i=0; i< (precSup->number); i++) {
	if (precType->papName[i] == NULL)
           continue;

       /*
        *  Create string that is the name of the RSET
        *   (record support entry table) structure for
        *   each record type.
        */
	strcpy(name,"_");
	strcat(name,precType->papName[i]);
	strcat(name,"RSET");

       /* Lookup name in vxWorks symbol table - and make sure it is program text */
	vxstatus = symFindByName(sysSymTbl, name,
            (void *) (&precSup->papRset[i]), &type);

	if (vxstatus != OK || ((type & N_TEXT) == 0)) {
	    strcpy(message,"record support entry table not found for ");
	    strcat(message,name);
	    status = S_rec_noRSET;
	    errMessage(status,message);
	    continue;
	}

       /* If an initialization routine exists for a record type, execute it */
	if (!(precSup->papRset[i]->init))
            continue;
	else {
	    rtnval = (*(precSup->papRset[i]->init))();
	    if (status==0)
                status = rtnval;
	}
    }

    return(status);
}

/*
 *  Initialize Device Support
 *      Locate all device support entry tables.
 *      Call the initialization routine (init) for each
 *        device type (First Pass).
 */
static long initDevSup(void)
{
    char	*pname;
    char	name[40];
    int		i,j;
    SYM_TYPE	type;
    char	message[100];
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    struct recDevSup	*precDevSup;
    struct devSup	*pdevSup;
    
    if (!(precDevSup = pdbBase->precDevSup)) {
	status = S_dev_noDevSup;
	errMessage(status,"No device support is defined");
	return(status);
    }

   /*
    *  For all possible records, initialize their device support
    *    routines.  There is usually more than one device support
    *    routine defined for every record type, this accounts for
    *    the doubly nested for() loops.  precDevSup->number = number
    *    of record types.
    */
    for (i=0; i< (precDevSup->number); i++) {
       /*
        *  Find the device support routine in the array of pointers
        */
	if ((pdevSup = precDevSup->papDevSup[i]) == NULL)
            continue;

       /* For every device support module defined for a record type... */
	for(j=0; j < (pdevSup->number); j++) {
           /* Create the name */
	    if (!(pname = pdevSup->papDsetName[j]))
                 continue;
	    strcpy(name, "_");
	    strcat(name, pname);

           /* Lookup name in vxWorks symbol table - make sure it is program text */
	    vxstatus = (long) symFindByName(sysSymTbl, name,
		(void *) &(pdevSup->papDset[j]), &type);

	    if (vxstatus != OK || ((type & N_TEXT) == 0)) {
                pdevSup->papDset[j]=NULL;
		strcpy(message, "device support entry table not found for ");
		strcat(message, pname);
		status = S_dev_noDSET;
		errMessage(status, message);
		continue;
	    }

           /* If an init routine exists for the device support module, execute it */
	    if (!(pdevSup->papDset[j]->init))
                continue;

	    rtnval = (*(pdevSup->papDset[j]->init))(0);

	    if (status == 0)
                status = rtnval;
	}
    }
    return(status);
}

/*
 *  Calls the second pass for each device support
 *    initialization routine.  The second pass is made
 *    after the database records have been initialized and
 *    placed into lock sets.
 */
static long finishDevSup(void) 
{
    int		i,j;
    struct recDevSup	*precDevSup;
    struct devSup	*pdevSup;

   /* Find pointer to device support for all record types */
    if (!(precDevSup=pdbBase->precDevSup))
         return(0);

   /* For every record type... */
    for(i=0; i< (precDevSup->number); i++) {
       /* Find pointer to device support for an individual record type */
	if ((pdevSup = precDevSup->papDevSup[i]) == NULL)
            continue;

       /* For every device support module defined for a record type */
	for(j=0; j < (pdevSup->number); j++) {
	    if (!(pdevSup->papDset[j]))
                continue;

           /* Call the second pass of the initialization, if the routine exists */
	    if (!(pdevSup->papDset[j]->init))
                continue;

	    (*(pdevSup->papDset[j]->init))(1);
	}
    
    }
    return(0);
}

static long initDatabase(void)
{
    char		name[PVNAME_SZ+FLDNAME_SZ+2];
    short		i,j;
    char		message[120];
    long		status=0;
    long		rtnval=0;
    struct recLoc	*precLoc;
    struct rset		*prset;
    struct recDes	*precDes;
    struct recTypDes	*precTypDes;
    struct recHeader	*precHeader;
    RECNODE		*precNode;
    struct fldDes	*pfldDes;
    struct dbCommon	*precord;
    struct dbAddr	dbAddr;
    struct link		*plink;
    struct devSup	*pdevSup;
    struct recSup	*precSup;
    struct recType	*precType;
   
   /* Find the record type and record support structures */ 
    if(!(precType=pdbBase->precType)) return(0);
    if(!(precSup=pdbBase->precSup)) return(0);

   /*
    *  Locate database record header
    *    This structure contains a pointer to an array of pointers to
    *    record location structures.  A record location structure
    *    exists for every record type.  It contains a pointer to a
    *    linked list of all instances of records with that type.
    */
    if (!(precHeader = pdbBase->precHeader)) {
	status = S_record_noRecords;
	errMessage(status,"No database records are defined");
	return(status);
    }

   /*
    *  Find record descriptions
    *    The record descriptions contain every piece of information
    *    you ever wanted to know about the individual fields in a
    *    particular record type.  See header file for more information.
    */
    if (!(precDes = pdbBase->precDes)) {
	status = S_record_noRecords;
	errMessage(status,"Database record descriptions were not defined");
	return(status);
    }

   /* For every record type ... */
    for(i=0; i< (precHeader->number); i++) {
       /* Get pointer to location structure for instances of record type 'i' */
	if (!(precLoc = precHeader->papRecLoc[i]))continue;

	if (!precLoc->preclist) continue;

       /*
        *  Find record support entry table, record description, and device
        *     support associated with record type 'i'
        */
	prset = GET_PRSET(precSup,i);
	precTypDes = precDes->papRecTypDes[i];
	pdevSup = GET_PDEVSUP(pdbBase->precDevSup,i);

       /*
        *  For all instances of record type 'i '...
        *     These records are contained in the linked list
        */
	for (precNode=(RECNODE *)ellFirst(precLoc->preclist);
	         precNode; precNode = (RECNODE *)ellNext(&precNode->node)) {

           /*
            *   If there is not record support entry table for record
            *     type 'i', report message, and break from for loop.
            */
	    if (!prset) {
		strcpy(name,precType->papName[i]);
		strcat(name,"RSET");
		strcpy(message,"record support entry table not found for ");
		strcat(message,name);
		status = S_rec_noRSET;
		errMessage(status,message);
		break;
	    }
           /* Find pointer to record instance */
	    precord = precNode->precord;

	    /* If NAME is null then skip this record*/
	    if(!(precord->name[0])) continue;

	    /* Initialize record's record support entry table field */
	    precord->rset = prset;

           /*
            *  Initialize mlok and mlis.
            *    (The monitor lock - basically a semaphore) and the
            *     monitor list field.  The list of "applications"
            *     observing changes in a database field.
            */
	    FASTLOCKINIT(&precord->mlok);
	    ellInit(&(precord->mlis));

           /* Reset the process active field */
	    precord->pact=FALSE;

	    /* Init DSET NOTE that result may be NULL */
	    precord->dset=(struct dset *)GET_PDSET(pdevSup,precord->dtyp);

           /* Initialize dbCommon structure of the record - First pass (pass=0) */
	    rtnval = dbCommonInit(precord,0);

           /*
            *  Call record support init_record routine - First pass
            *     (pass=0).  (only if init_record is defined...)
            */
	    if (!(precSup->papRset[i]->init_record)) continue;

	    rtnval = (*(precSup->papRset[i]->init_record))(precord,0);

	    if (status==0)
               status = rtnval;
	}
    }

   /*
    *  Second pass to resolve links
    */
   /* For all record types */
    for(i=0; i< (precHeader->number); i++) {
       /* Find record instances */
	if (!(precLoc = precHeader->papRecLoc[i]))continue;

	if (!precLoc->preclist) continue;

       /* Find record field descriptions */
	precTypDes = precDes->papRecTypDes[i];

       /* For all record instances of type 'i' in the linked list... */
	for (precNode=(RECNODE *)ellFirst(precLoc->preclist);
	      precNode; precNode = (RECNODE *)ellNext(&precNode->node)) {
		precord = precNode->precord;

	        /* If NAME is null then skip this record*/
		if (!(precord->name[0])) continue;

               /*
                *  Convert all PV_LINKs to DB_LINKs or CA_LINKs
                *    Figures out what type of link to use.  A
                *    database link local to the IOC, or a channel
                *    access link across the network.
                */
               /* For all the links in the record type... */
		for(j=0; j<precTypDes->no_links; j++) {
		    pfldDes = precTypDes->papFldDes[precTypDes->link_ind[j]];

                   /*
                    *  The actual link structure is located at this offset
                    *    within the link structure.
                    */
		    plink = (struct link *)((char *)precord + pfldDes->offset);

                   /*
                    *  Link type should be PV_LINK, unless it is
                    *    constant or hardware-specific
                    */
		    if (plink->type == PV_LINK) {
			strncpy(name,plink->value.pv_link.pvname,PVNAME_SZ);

                       /* create record name */
			name[PVNAME_SZ]=0;
			strcat(name,".");
			strncat(name,plink->value.pv_link.fldname,FLDNAME_SZ);

                       /*
                        *  Lookup record name in database
                        *    If a record is _not_ local to the IOC, it is a
                        *    channel access link, otherwise it is a
                        *    database link.
                        */
			if (dbNameToAddr(name,&dbAddr) == 0) {
			    plink->type = DB_LINK;
			    plink->value.db_link.pdbAddr =
				dbCalloc(1,sizeof(struct dbAddr));
			    *((struct dbAddr *)(plink->value.db_link.pdbAddr))=dbAddr;
                           /*
                            *  Initialize conversion to "uninitialized" conversion
                            */
                            plink->value.db_link.conversion = cvt_uninit;
			}
			else {
                           /*
                            *  Not a local process variable ... assuming a CA_LINK
                            *    Only supporting Non Process Passive links, Input Maximize
                            *    Severity/No Maximize Severity(MS/NMS), and output NMS
                            *    links ... The following code checks for this.
                            */
                            if (plink->value.db_link.process_passive
                                || (pfldDes->field_type == DBF_OUTLINK
                                    && plink->value.db_link.maximize_sevr))
                            {
                               /*
                                * Link PP and/or Outlink MS ...
                                *   neither supported under CA_LINKs
                                */
                                strncpy(message,precord->name,PVNAME_SZ);
                                message[PVNAME_SZ]=0;
                                strcat(message,".");
                                strncat(message,pfldDes->fldname,FLDNAME_SZ);
                                strcat(message,": link process variable =");
                                strcat(message,name);
                                strcat(message," not found");
                                status = S_db_notFound;
                                errMessage(status,message);
                                if(rtnval==OK) rtnval=status;
                            }
			}
		    }
		}
	}
    }

   /*
    *  Call record support init_record routine - Second pass
    */
   /* For all record types... */
    for(i=0; i< (precHeader->number); i++) {
	if (!(precLoc = precHeader->papRecLoc[i])) continue;
	if (!precLoc->preclist) continue;
	if (!(prset=GET_PRSET(precSup,i))) continue;
	precTypDes = precDes->papRecTypDes[i];

       /* For all record instances of type 'i' */
	for (precNode=(RECNODE *)ellFirst(precLoc->preclist);
	    precNode; precNode = (RECNODE *)ellNext(&precNode->node)) {
               /* Locate name of record */
		precord = precNode->precord;
	       /* If NAME is null then skip this record*/
		if (!(precord->name[0])) continue;
		rtnval = dbCommonInit(precord,1);
		if (status==0) status = rtnval;
	       /* call record support init_record routine - Second pass (pass = 1) */
		if (!(precSup->papRset[i]->init_record)) continue;
		rtnval = (*(precSup->papRset[i]->init_record))(precord, 1);
		if (status == 0) status = rtnval;
	}
    }
    return(status);
}

/*
 *  Create lock sets for records
 *    A lock set is a set of records that must be locked
 *    with a semaphore so as to prevent mutual exclusion
 *    hazards.  Lock sets are determined by examining the
 *    links interconnecting the records.  If a link connecting
 *    two records is not an NPP/NMS single-valued input link,
 *    then the two records are considered part of the same
 *    lock set.  Records connected by forward links are
 *    definately considered part of the same lockset.
 */
static void createLockSets(void)
{
    int			i,link;
    struct recLoc	*precLoc;
    struct recDes	*precDes;
    struct recTypDes	*precTypDes;
    struct recHeader	*precHeader;
    RECNODE		*precNode;
    struct fldDes	*pfldDes;
    struct dbCommon	*precord;
    struct link		*plink;
    short		nset,maxnset,newset;
    int			again;
    
    nset = 0;
    if(!(precHeader = pdbBase->precHeader)) return;
    if(!(precDes = pdbBase->precDes)) return;
    for(i=0; i< (precHeader->number); i++) {
	if(!(precLoc = precHeader->papRecLoc[i]))continue;
	if(!precLoc->preclist) continue;
	precTypDes = precDes->papRecTypDes[i];
	for(precNode=(RECNODE *)ellFirst(precLoc->preclist);
	precNode; precNode = (RECNODE *)ellNext(&precNode->node)) {
	    precord = precNode->precord;
	    /* If NAME is null then skip this record*/
	    if(!(precord->name[0])) continue;
	    if(precord->lset) continue; /*already in a lock set*/

           /*
            *  At First, assume record is in a different lockset
            *    We shall see later if this assumption is incorrect.
            */
	    precord->lset = maxnset = ++nset;

           /*
            *  Use the process active flag to eliminate traversing
            *     cycles in the database "graph"
            */
	    precord->pact = TRUE; again = TRUE;
	    while(again) {
		again = FALSE;
    		for(link=0; link<precTypDes->no_links; link++) {
		    struct dbAddr	*pdbAddr;

		    pfldDes = precTypDes->papFldDes[precTypDes->link_ind[link]];

                   /*
                    *  Find link structure, which is at an offset in the record
                    */
		    plink = (struct link *)((char *)precord + pfldDes->offset);
                   /* Ignore link if type is constant, channel access, or hardware specific */
		    if(plink->type != DB_LINK) continue;

		    pdbAddr = (struct dbAddr *)(plink->value.db_link.pdbAddr);

                   /* The current record is in a different lockset -IF-
                    *    1. Input link
                    *    2. Not Process Passive
                    *    3. Not Maximize Severity
                    *    4. Not An Array Operation - single element only
                    */
		    if (pfldDes->field_type==DBF_INLINK
	    	          && ( !(plink->value.db_link.process_passive)
	                  && !(plink->value.db_link.maximize_sevr))
		          && pdbAddr->no_elements<=1) continue;

                   /*
                    *  Combine the lock sets of the current record with the
                    *    remote record pointed to by the link. (recursively)
                    */
		    newset = makeSameSet(pdbAddr,precord->lset);

                   /*
                    *  Perform an iteration of the while-loop again
                    *     if we find that the record pointed to by
                    *     the link has its lockset set earlier.  If
                    *      it has, set the current record's lockset to
                    *      that of the link's endpoint.
                    */
		    if (newset!=precord->lset) {
			if(precord->lset==maxnset && maxnset==nset) nset--;
			precord->lset = newset;
			again = TRUE;
			break;
		    }
		}
	    }
	    precord->pact = FALSE;
	}
    }
    dbScanLockInit(nset);
}

static short makeSameSet(struct dbAddr *paddr, short lset)
{
    struct dbCommon 	*precord = paddr->precord;
    short  		link;
    struct fldDes       *pfldDes;
    struct link		*plink;
    struct recTypDes	*precTypDes;
    struct recDes	*precDes;
    int			again;

   /*
    *  Use the process active flag to eliminate traversing
    *     cycles in the database "graph."  Effectively converts
    *     the arbitrary database "graph" (where edges are
    *     defined as links) into a tree structure.
    */
    if(precord->pact) return(((precord->lset<lset) ? precord->lset : lset));

   /*
    *  If the lock set of the link's endpoint is already set
    *    to the lockset we are setting it to, return...
    */
    if(lset == precord->lset) return(lset);

   /*
    *  If the record has an uninitialized lock set field,
    *    we set it here.
    */
    if(precord->lset == 0) precord->lset = lset;

   /*
    *  If the record is already in a lockset determined earlier,
    *     return that lock set.
    */
    if(precord->lset < lset) return(precord->lset);

    if(!(precDes = pdbBase->precDes)) return(0);

   /* set pact to prevent cycles */
    precord->lset = lset; precord->pact = TRUE; again = TRUE;
    while(again) {
	again = FALSE;
	precTypDes = precDes->papRecTypDes[paddr->record_type];
	for(link=0; link<precTypDes->no_links; link++) {
	    struct dbAddr	*pdbAddr;
	    short		newset;

	    pfldDes = precTypDes->papFldDes[precTypDes->link_ind[link]];
	    plink = (struct link *)((char *)precord + pfldDes->offset);
	    if(plink->type != DB_LINK) continue;
	    pdbAddr = (struct dbAddr *)(plink->value.db_link.pdbAddr);
	    if( pfldDes->field_type==DBF_INLINK
	    && ( !(plink->value.db_link.process_passive)
	    && !(plink->value.db_link.maximize_sevr) )
	    && pdbAddr->no_elements<=1) continue;
	    newset = makeSameSet(pdbAddr,precord->lset);
	    if(newset != precord->lset) {
		precord->lset = newset;
		again = TRUE;
		break;
	    }
	}
    }
    precord->pact = FALSE;
    return(precord->lset);
}

/*
 *  Process database records at initialization if
 *     their pini (process at init) field is set.
 */
static long initialProcess(void)
{
    short	i;
    struct recHeader	*precHeader;
    struct recLoc	*precLoc;
    RECNODE		*precNode;
    struct dbCommon	*precord;
    
    if(!(precHeader = pdbBase->precHeader)) return(0);
    for(i=0; i< (precHeader->number); i++) {
	if(!(precLoc = precHeader->papRecLoc[i]))continue;
	if(!precLoc->preclist) continue;
	for(precNode=(RECNODE *)ellFirst(precLoc->preclist);
	    precNode; precNode = (RECNODE *)ellNext(&precNode->node)) {
		precord = precNode->precord;
	        /* If NAME is null then skip this record*/
		if(!(precord->name[0])) continue;
		if(!precord->pini) continue;
		(void)dbProcess(precord);
	}
    }
    return(0);
}

#define MAX 128
#define SAME 0
static char    *cvt_str[] = {
    "DBF_STRING",
    "DBF_SHORT",
    "DBF_LONG",
    "DBF_FLOAT",
    "DBF_DOUBLE"
};
#define CVT_COUNT (sizeof(cvt_str) / sizeof(char*))
static long getResources(char  *fname)
{
    FILE            *fp;
    int             len;
    int             len2;
    int             lineNum = 0;
    int             i = 0;
    int             found = 0;
    int             cvType = 0;
    int		    epicsFlag;
    char            buff[MAX + 1];
    char            name[40];
    char            s1[MAX];
    char            s2[MAX];
    char            s3[MAX];
    char            message[100];
    long            rtnval = 0;
    SYM_TYPE	    type;
    char           *pSymAddr;
    short           n_short;
    long            n_long;
    float           n_float;
    double          n_double;
    if (!fname) return (0);
    if ((fp = fopen(fname, "r")) == 0) {
	errMessage(-1L, "getResources: No such Resource file");
	return (-1);
    }
    while ( fgets( buff, MAX, fp) != NULL) {
	len = strlen(buff);
	lineNum++;
	if (len < 2)
	    goto CLEAR;
	if (len >= MAX) {
	    sprintf(message,
		    "getResources: Line too long - line=%d", lineNum);
	    errMessage(-1L, message);
	    return (-1);
	}
	for (i = 0; i < len; i++) {
	    if (buff[i] == '!') {
		goto CLEAR;
	    }
	}
	/* extract the 3 fields as strings */
	if ((sscanf(buff, "%s %s %s", s1, s2, s3)) != 3) {
	    sprintf(message,
		    "getResources: Not enough fields - line=%d", lineNum);
	    errMessage(-1L, message);
	    return (-1);
	}
	found = 0;
	len2 = strlen(s2);
	for (i = 0; i < CVT_COUNT; i++) {


	    if ((strncmp(s2, cvt_str[i], len2)) == SAME) {
		found = 1;
		cvType = i;
		break;
	    }
	}
	if (!found) {
	    sprintf(message,
		    "getResources: Field 2 not defined - line=%d", lineNum);
	    errMessage(-1L, message);
	    return (-1);
	}
	strcpy(name, "_");
	strcat(name, s1);
	rtnval = symFindByName(sysSymTbl, name, &pSymAddr, &type);
	if (rtnval != OK || (type & N_TEXT == 0)) {
	    sprintf(message,
		  "getResources: Symbol name not found - line=%d", lineNum);
	    errMessage(-1L, message);
	    return (-1);
	}
	if ( (strncmp(s1,"EPICS_",6)) == SAME)
            epicsFlag = 1;
        else
            epicsFlag = 0;

	switch (cvType) {
	case 0:	/* DBF_STRING */
	    len = strlen(s3);
	    len2 = 20;
	    if (len >= len2) {
		sprintf(message,
			"getResources: Warning, string might exceed previous reserved space - line=%d",
			lineNum);
		errMessage(-1L, message);
	    }
            if ( epicsFlag )
                strncpy(pSymAddr+sizeof(void *), s3, len + 1);
            else
                strncpy(pSymAddr, s3, len + 1);
	    break;
	case 1:	/* DBF_SHORT */
	    if ((sscanf(s3, "%hd", &n_short)) != 1) {
		sprintf(message,
		      "getResources: conversion failed - line=%d", lineNum);
		errMessage(-1L, message);
	        return (-1);
	    }
            if ( epicsFlag ) {
                sprintf(message,
                       "getResources: EPICS_ type DBF_SHORT not supported - line =%d",
                        lineNum);
                errMessage(-1L, message);
            }
            else
                *(short *) pSymAddr = n_short;

	    break;
	case 2:	/* DBF_LONG */
	    if ((sscanf(s3, "%ld", &n_long)) != 1) {
		sprintf(message,
		      "getResources: conversion failed - line=%d", lineNum);
		errMessage(-1L, message);
	        return (-1);
	    }
            if ( epicsFlag ) {
                sprintf(message,
                       "getResources: EPICS_ type DBF_LONG not supported - line= %d",
                        lineNum);
                errMessage(-1L, message);
            }
            else
                *(long *) pSymAddr = n_long;
	    break;
	case 3:	/* DBF_FLOAT */
	    if ((sscanf(s3, "%e", &n_float)) != 1) {
		sprintf(message,
		      "getResources: conversion failed - line=%d", lineNum);
		errMessage(-1L, message);
	        return (-1);
	    }
            if ( epicsFlag ) {
                sprintf(message,
                       "getResources: EPICS_ type DBF_FLOAT not supported - line =%d",
                        lineNum);
                errMessage(-1L, message);
            }
            else
                *(float *) pSymAddr = n_float;

	    break;
	case 4:	/* DBF_DOUBLE */
	    if ((sscanf(s3, "%le", &n_double)) != 1) {
		sprintf(message,
		      "getResources: conversion failed - line=%d", lineNum);
		errMessage(-1L, message);
	        return (-1);
	    }
            if ( epicsFlag ) {
                sprintf(message,
                       "getResources: EPICS_ type DBF_DOUBLE not supported - line=%d",
                        lineNum);
                errMessage(-1L, message);
            }
            else
                *(double *) pSymAddr = n_double;

	    break;
	default:
	    sprintf(message,
		 "getResources: switch default reached - line=%d", lineNum);
	    errMessage(-1L, message);
	    return (-1);
	    break;
	}
CLEAR:	memset(buff, '\0',  MAX);
	memset(s1, '\0', MAX);
	memset(s2, '\0', MAX);
	memset(s3, '\0', MAX);
    }
    fclose(fp);
    return (0);
}

static gotSdrSum=FALSE;
static struct sdrSum sdrSum;
int dbLoad(char * pfilename)
{
    long	status;
    FILE	*fp;

    fp = fopen(pfilename,"r");
    if(!fp) {
	errMessage(-1,"dbLoad: Error opening file");
	return(-1);
    }
    if(pdbBase==NULL) pdbBase = dbAllocBase();
    status=dbRead(pdbBase, fp);
    fclose(fp);
    if(status!=0) {
	errMessage(status,"dbLoad aborting because dbRead failed");
	return(-1);
    }
    if(!gotSdrSum) {
	fp = fopen("default.sdrSum","r");
	if(!fp) {
	    errMessage(-1,"dbLoad: Error opening default.sdrSum");
	    return(-1);
	}
	fgets(sdrSum.allSdrSums,sizeof(sdrSum.allSdrSums),fp);
	fclose(fp);
	gotSdrSum = TRUE;
    }
    if(strncmp(pdbBase->psdrSum->allSdrSums,sdrSum.allSdrSums,
    strlen(pdbBase->psdrSum->allSdrSums))!=0) {
	errMessage(-1,"dbLoad: check sdrSum Error: Database out of date");
	return(-1);
    }
    return (0);
}
