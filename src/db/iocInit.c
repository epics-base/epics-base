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
 * .26	02-09-94	jbk	changed to new time stamp support software ts_init()
 * .27	03-18-94	mcn	added comments
 * .28	03-23-94	mrk	Added asInit
 * .29	04-04-94	mcn	added code for uninitialized conversions (link conversion field)
 * .30	01-10-95	joh	Fixed no quoted strings in resource.def problem
 * .31	02-10-95	joh	static => LOCAL 
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<string.h>
#include 	<errno.h>

#include	<sysLib.h>
#include	<symLib.h>
#include	<sysSymTbl.h>	/* for sysSymTbl*/
#include	<logLib.h>
#include	<taskLib.h>
#include	<envLib.h>
#include	<errnoLib.h>

#include	<ellLib.h>
#include	<fast_lock.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include	<taskwd.h>
#include	<callback.h>
#include	<dbCommon.h>
#include	<dbBase.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<envDefs.h>
#include	<dbStaticLib.h>
#include	<initHooks.h>
#include	<drvTS.h>
#include	<asLib.h>
#include	<epicsPrint.h>

/*This module will declare and initilize module_type variables*/
#define MODULE_TYPES_INIT 1
#include        <module_types.h>

#define LOCAL

LOCAL int initialized=FALSE;

/* The following is for use by interrupt routines */
volatile int interruptAccept=FALSE;

struct dbBase *pdbBase=NULL;

/* define forward references*/
LOCAL long initDrvSup(void);
LOCAL long initRecSup(void);
LOCAL long initDevSup(void);
LOCAL long finishDevSup(void);
LOCAL long initDatabase(void);
LOCAL void createLockSets(void);
LOCAL void createLockSetsExtraPass(int *anyChange);
LOCAL short makeSameSet(struct dbAddr *paddr,short set,int *anyChange);
LOCAL long initialProcess(void);
LOCAL long getResources(char  *fname);
LOCAL int getResourceToken(FILE *fp, char *pToken, unsigned maxToken);
LOCAL int getResourceTokenInternal(FILE *fp, char *pToken, unsigned maxToken);


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
	logMsg("iocInit can only be called once\n",
	    0,0,0,0,0,0);
	return(-1);
    }

    if (!pdbBase) {
	logMsg("iocInit aborting because No database loaded by dbAsciiRead\n",
	    0,0,0,0,0,0);
	return(-1);
    }

    errInit(); /*Initialize errPrintf task*/

   /* Setup initialization hooks, if  initHooks routine has been defined.  */
    strcpy(name, "_");
    strcat(name, "initHooks");
    rtnval = symFindByName(sysSymTbl, name, (void *) &pinitHooks, &type);
    if (pinitHooks) (*pinitHooks)(INITHOOKatBeginning);

    coreRelease();
    status = getResources(pResourceFilename); 
    if (pinitHooks) (*pinitHooks)(INITHOOKafterGetResources);

    status = iocLogInit();
    if (status!=0) {
        logMsg("iocInit Failed to Initialize Ioc Log Client \n",0,0,0,0,0,0);
    }
    if (pinitHooks) (*pinitHooks)(INITHOOKafterLogInit);


   /* After this point, further calls to iocInit() are disallowed.  */
    initialized = TRUE;

   /*
    *  Initialize the watchdog task.  These is different from vxWorks watchdogs,
    *    this task is assigned to watching and reporting if the vxWorks tasks
    *    (that it has been told to watch) are suspended.
    */
    taskwdInit();

    callbackInit();

   /* Wait 1/10 second for above initializations to complete*/
    (void)taskDelay(sysClkRateGet()/10);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterCallbackInit);

   /* Initialize Channel Access Link mechanism.  Pass #1 */
    dbCaLinkInit((int) 1);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterCaLinkInit1);

    if (initDrvSup() != 0)
         logMsg("iocInit: Drivers Failed during Initialization\n",0,0,0,0,0,0);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitDrvSup);

    if (initRecSup() != 0)
         logMsg("iocInit: Record Support Failed during Initialization\n",
	    0,0,0,0,0,0);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitRecSup);

    if (initDevSup() != 0)
         logMsg("iocInit: Device Support Failed during Initialization\n",
	    0,0,0,0,0,0);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitDevSup);

    TSinit(); /* new time stamp driver (jbk) */
    if (pinitHooks) (*pinitHooks)(INITHOOKafterTS_init);

   /* initialize database records */
    if (initDatabase() != 0)
         logMsg("iocInit: Database Failed during Initialization\n",0,0,0,0,0,0);

    createLockSets();
    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitDatabase);

    dbCaLinkInit((int) 2);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterCaLinkInit2);

    if (finishDevSup() != 0)
         logMsg("iocInit: Device Support Failed during Finalization\n",
	    0,0,0,0,0,0);
    if (pinitHooks) (*pinitHooks)(INITHOOKafterFinishDevSup);

    scanInit();
    asInit();
    (void)taskDelay(sysClkRateGet()/2);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterScanInit);

   /* Enable scan tasks and some driver support functions.  */
    interruptAccept=TRUE;

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInterruptAccept);

   /*
    *  Process all records that have their "process at initialization"
    *      field set (pini).
    */
    if (initialProcess() != 0)
          logMsg("iocInit: initialProcess Failed\n",0,0,0,0,0,0);

    if (pinitHooks) (*pinitHooks)(INITHOOKafterInitialProcess);

   /*  Start up CA server */
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
LOCAL long initDrvSup(void) /* Locate all driver support entry tables */
{
    char	name[40];
    SYM_TYPE	type;
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    drvSup	*pdrvSup;
    struct drvet *pdrvet;

   /*
    *  For every driver support module, look up the name
    *    for that function in the vxWorks symbol table.  If
    *    a driver entry table doesn't exist, report that
    *    fact.
    */
    for(pdrvSup = (drvSup *)ellFirst(&pdbBase->drvList); pdrvSup;
    pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
	strcpy(name,"_");
	strcat(name,pdrvSup->name);
	vxstatus = symFindByName(sysSymTbl, name,
		(void *) &pdrvet, &type);
	if (vxstatus != OK) {
	    status = S_drv_noDrvet;
	    errPrintf(status,__FILE__,__LINE__,"%s",pdrvSup->name);
	    continue;
	}
	pdrvSup->pdrvet = pdrvet;
       /*
        *   If an initialization routine is defined (not NULL),
        *      for the driver support call it.
        */
	if (!pdrvet->init) continue;
	rtnval = (*(pdrvet->init))();
	if (status == 0) status = rtnval;
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
LOCAL long initRecSup(void)
{
    char	name[60];
    SYM_TYPE	type;
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    dbRecDes	*pdbRecDes;
    struct rset *prset;
    
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	strcpy(name,"_");
	strcat(name,pdbRecDes->name);
	strcat(name,"RSET");
	vxstatus = symFindByName(sysSymTbl, name,
            (void *)&pdbRecDes->prset, &type);
	if (vxstatus != OK) {
	    status = S_rec_noRSET;
	    errPrintf(status,__FILE__,__LINE__,"%s",pdbRecDes->name);
	    continue;
	}
	prset = pdbRecDes->prset;
       /* If an initialization routine exists for a record type, execute it */
	if(!prset->init) {
            continue;
	} else {
	    rtnval = (*prset->init)();
	    if (status==0)
                status = rtnval;
	}
    }
    return(status);
}

/*  Initialize Device Support
 *      Locate all device support entry tables.
 *      Call the initialization routine (init) for each
 *        device type (First Pass).
 */
LOCAL long initDevSup(void)
{
    char	*pname;
    char	name[40];
    SYM_TYPE	type;
    long	status=0;
    long	rtnval = 0;
    STATUS	vxstatus;
    dbRecDes	*pdbRecDes;
    devSup	*pdevSup;
    struct dset *pdset;
    
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	for(pdevSup = (devSup *)ellFirst(&pdbRecDes->devList); pdevSup;
	pdevSup = (devSup *)ellNext(&pdevSup->node)) {
	    if(!(pname = pdevSup->name)) continue;
	    strcpy(name, "_");
	    strcat(name, pname);
	    vxstatus = (long) symFindByName(sysSymTbl, name,
		(void *) &pdset, &type);
	    if (vxstatus != OK) {
		status = S_dev_noDSET;
		errPrintf(status,__FILE__,__LINE__,"%s",pdevSup->name);
		continue;
	    }
	    pdevSup->pdset = pdset;
	    if(!(pdset->init)) continue;
	    rtnval = (*pdset->init)(0);
	    if (status == 0) status = rtnval;
	}
    }
    return(status);
}

/*  Calls the second pass for each device support
 *    initialization routine.  The second pass is made
 *    after the database records have been initialized and
 *    placed into lock sets.
 */
LOCAL long finishDevSup(void) 
{
    dbRecDes	*pdbRecDes;
    devSup	*pdevSup;
    struct dset *pdset;

    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	for(pdevSup = (devSup *)ellFirst(&pdbRecDes->devList); pdevSup;
	pdevSup = (devSup *)ellNext(&pdevSup->node)) {
	    if(!(pdset = pdevSup->pdset)) continue;
	    if(pdset->init) (*pdset->init)(1);
	}
    
    }
    return(0);
}

LOCAL long initDatabase(void)
{
    long		status=0;
    long		rtnval=0;
    dbRecDes		*pdbRecDes;
    dbFldDes		*pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    devSup		*pdevSup;
    struct rset		*prset;
    struct dset		*pdset;
    dbCommon		*precord;
    DBADDR		dbaddr;
    DBLINK		*plink;
    int			j;
   
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	prset = pdbRecDes->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecDes->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    if(!prset) break;
           /* Find pointer to record instance */
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    precord->rset = prset;
	    FASTLOCKINIT(&precord->mlok);
	    ellInit(&(precord->mlis));

           /* Reset the process active field */
	    precord->pact=FALSE;

	    /* Init DSET NOTE that result may be NULL */
	    pdevSup = (devSup *)ellNth(&pdbRecDes->devList,precord->dtyp+1);
	    pdset = (pdevSup ? pdevSup->pdset : 0);
	    precord->dset = pdset;
           /* Initialize dbCommon - First pass (pass=0) */
	    rtnval = dbCommonInit(precord,0);
	    if(!prset->init_record) continue;
	    rtnval = (*prset->init_record)(precord,0);
	    if (status==0) status = rtnval;
	}
    }

   /*
    *  Second pass to resolve links
    */
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	prset = pdbRecDes->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecDes->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
            /*
             *  Convert all PV_LINKs to DB_LINKs or CA_LINKs
             *    Figures out what type of link to use.  A
             *    database link local to the IOC, or a channel
             *    access link across the network.
             */
            /* For all the links in the record type... */
	    for(j=0; j<pdbRecDes->no_links; j++) {
		pdbFldDes = pdbRecDes->papFldDes[pdbRecDes->link_ind[j]];
		plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		if (plink->type == PV_LINK) {
                    /*
                     *  Lookup record name in database
                     *    If a record is _not_ local to the IOC, it is a
                     *    channel access link, otherwise it is a
                     *    database link.
                     */
		    if(dbNameToAddr(plink->value.pv_link.pvname,&dbaddr) == 0) {
			DBADDR	*pdbAddr;

			plink->type = DB_LINK;
			pdbAddr = dbCalloc(1,sizeof(struct dbAddr));
			*pdbAddr = dbaddr; /*structure copy*/;
			plink->value.db_link.pdbAddr = pdbAddr;
                        /*Initialize conversion to "uninitialized" conversion */
                        plink->value.db_link.conversion = cvt_uninit;
		    } else {
			/*
			 *  Not a local process variable ... assuming a CA_LINK
                         *  Only supporting Non Process Passive links,
			 *  Input Maximize Severity/No Maximize Severity(MS/NMS)
			 * , and output NMS
                         *    links ... The following code checks for this.
                        */
                        if (errVerbose &&
			(plink->value.db_link.process_passive
                        || (pdbFldDes->field_type == DBF_OUTLINK
                        && plink->value.db_link.maximize_sevr))) {
			    status = S_db_badField;
			    errPrintf(status,__FILE__,__LINE__,"%s.%s %s",
				precord->name,pdbFldDes->name,
				" PP and/or MS illegal");
			    status = 0;
                        }
		    }
		}
	    }
	}
    }

    /* Call record support init_record routine - Second pass */
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	prset = pdbRecDes->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecDes->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    if(!prset) break;
           /* Find pointer to record instance */
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    precord->rset = prset;
	    if(!prset->init_record) continue;
	    rtnval = (*prset->init_record)(precord,1);
	    if (status==0) status = rtnval;
	}
    }
    return(status);
}

LOCAL void createLockSets(void)
{
    int			link;
    dbRecDes		*pdbRecDes;
    dbFldDes		*pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    dbCommon		*precord;
    DBLINK		*plink;
    short		nset,maxnset,newset;
    int			again;
    int			anyChange;
    
    nset = 0;
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecDes->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
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
    		for(link=0; (link<pdbRecDes->no_links&&!again) ; link++) {
		    DBADDR	*pdbAddr;

		    pdbFldDes = pdbRecDes->papFldDes[pdbRecDes->link_ind[link]];
		    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		    if(plink->type != DB_LINK) continue;

		    pdbAddr = (DBADDR *)(plink->value.db_link.pdbAddr);

                   /* The current record is in a different lockset -IF-
                    *    1. Input link
                    *    2. Not Process Passive
                    *    3. Not Maximize Severity
                    *    4. Not An Array Operation - single element only
                    */
		    if (pdbFldDes->field_type==DBF_INLINK
	    	          && ( !(plink->value.db_link.process_passive)
	                  && !(plink->value.db_link.maximize_sevr))
		          && pdbAddr->no_elements<=1) continue;

                   /*
                    *  Combine the lock sets of the current record with the
                    *    remote record pointed to by the link. (recursively)
                    */
		    newset = makeSameSet(pdbAddr,precord->lset,&anyChange);

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
    anyChange = TRUE;
    while(anyChange) {
	anyChange = FALSE;
	createLockSetsExtraPass(&anyChange);
    }
    dbScanLockInit(nset);
}

LOCAL void createLockSetsExtraPass(int *anyChange)
{
    int			link;
    dbRecDes		*pdbRecDes;
    dbFldDes		*pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    dbCommon		*precord;
    DBLINK		*plink;
    int			again;
    
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecDes->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    /*Prevent cycles in database graph*/
	    precord->pact = TRUE; again = TRUE;
	    while(again) {
		short newset;

		again = FALSE;
    		for(link=0; (link<pdbRecDes->no_links&&!again) ; link++) {
		    DBADDR	*pdbAddr;

		    pdbFldDes = pdbRecDes->papFldDes[pdbRecDes->link_ind[link]];
		    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		    if(plink->type != DB_LINK) continue;
		    pdbAddr = (DBADDR *)(plink->value.db_link.pdbAddr);
		    if(pdbFldDes->field_type==DBF_INLINK
	    	          && ( !(plink->value.db_link.process_passive)
	                  && !(plink->value.db_link.maximize_sevr))
		          && pdbAddr->no_elements<=1) continue;

		    newset = makeSameSet(pdbAddr,precord->lset,anyChange);
		    if (newset!=precord->lset) {
			precord->lset = newset;
			*anyChange = TRUE;
			again = TRUE;
			break;
		    }
		}
	    }
	    precord->pact = FALSE;
	}
    }
}

LOCAL short makeSameSet(struct dbAddr *paddr, short lset,int *anyChange)
{
    dbCommon 		*precord = paddr->precord;
    short  		link;
    dbRecDes		*pdbRecDes;
    dbFldDes		*pdbFldDes;
    DBLINK		*plink;
    int			again;

    /*Prevent cycles in database graph*/
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
   /* set pact to prevent cycles */
    precord->lset = lset; precord->pact = TRUE; again = TRUE;
    while(again) {
	again = FALSE;
	pdbRecDes = ((dbFldDes *)paddr->pfldDes)->pdbRecDes;
	for(link=0; link<pdbRecDes->no_links; link++) {
	    DBADDR	*pdbAddr;
	    short	newset;

	    pdbFldDes = pdbRecDes->papFldDes[pdbRecDes->link_ind[link]];
	    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
	    if(plink->type != DB_LINK) continue;
	    pdbAddr = (DBADDR *)(plink->value.db_link.pdbAddr);
	    if( pdbFldDes->field_type==DBF_INLINK
	    && ( !(plink->value.db_link.process_passive)
	    && !(plink->value.db_link.maximize_sevr) )
	    && pdbAddr->no_elements<=1) continue;
	    newset = makeSameSet(pdbAddr,precord->lset,anyChange);
	    if(newset != precord->lset) {
		precord->lset = newset;
		again = TRUE;
		*anyChange = TRUE;
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
LOCAL long initialProcess(void)
{
    dbRecDes		*pdbRecDes;
    dbRecordNode 	*pdbRecordNode;
    dbCommon		*precord;
    
    for(pdbRecDes = (dbRecDes *)ellFirst(&pdbBase->recDesList); pdbRecDes;
    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node)) {
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecDes->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    if(!precord->pini) continue;
	    (void)dbProcess(precord);
	}
    }
    return(0);
}

#define MAX 256 
#define SAME 0

enum resType {
	resDBF_STRING,
	resDBF_SHORT,
	resDBF_LONG,
	resDBF_FLOAT,
	resDBF_DOUBLE,
	resInvalid}; 
LOCAL char    *cvt_str[] = {
    "DBF_STRING",
    "DBF_SHORT",
    "DBF_LONG",
    "DBF_FLOAT",
    "DBF_DOUBLE",
    "Invalid",
};

#define EPICS_ENV_PREFIX "EPICS_"

long getResources(char  *fname)
{
    char            s1[MAX];
    char            s2[MAX];
    char            s3[MAX];
    char            name[40];
    FILE            *fp;
    enum resType    cvType = resInvalid;
    int		    epicsFlag;
    SYM_TYPE	    type;
    char            *pSymAddr;
    short           n_short;
    long            n_long;
    float           n_float;
    double          n_double;
    int		    status;

    if (!fname) return (0);

    if ((fp = fopen(fname, "r")) == 0) {
	errPrintf(
		-1L,
		__FILE__, 
		__LINE__,
		"No such Resource file - %s",
		fname);
	return (-1);
    }

    while (TRUE) {
	status = getResourceToken (fp, s1, sizeof(s1));
	if (status<0) {
		/*
		 * EOF
		 */
		break; 
	}
	status = getResourceToken (fp, s2, sizeof(s2));
	if (status<0) {
		errPrintf (
			-1L,
			__FILE__,
			__LINE__,
	"Missing resource data type field for resource=%s in file=%s",
			s1,
			fname);
		break; 
	}
	status = getResourceToken (fp, s3, sizeof(s3));
	if (status<0) {
		errPrintf (
			-1L,
			__FILE__,
			__LINE__,
	"Missing resource value field for resource=%s data type=%s file=%s",
			s1,
			s2,
			fname);
		break; /* EOF */
	}

	for (cvType = 0; cvType < resInvalid; cvType++) {
	    if (strcmp(s2, cvt_str[cvType]) == SAME) {
		break;
	    }
	}


	strcpy(name, "_");
	strcat(name, s1);
	status = symFindByName(sysSymTbl, name, &pSymAddr, &type);
	if (status!= OK) {
	    errPrintf (
		-1L,
		__FILE__,
		__LINE__,
	"Matching Symbol name not found for resource=%s",
		s1);
	    continue;
	}

	status = strncmp (
			s1,
			EPICS_ENV_PREFIX,
			strlen (EPICS_ENV_PREFIX));
	if (status == SAME) {
            	epicsFlag = 1;
		if (cvType != resDBF_STRING) {
			errPrintf (
				-1L,
				__FILE__,
				__LINE__,
"%s should be set with type DBF_STRING not type %s",
				s1,
				s2);
			continue;
		}
	}
	else {
		epicsFlag = 0;
	}

	switch (cvType) {
	case resDBF_STRING:
            if ( epicsFlag ) {
		char		*pEnv;

		/*
		 * space for two strings, an '=' character,
		 * and a null termination
		 */
		pEnv = malloc (strlen (s3) + strlen (s1) + 2);	
		if (!pEnv) {
			errPrintf(
				-1L,
				__FILE__,
				__LINE__,
"Failed to set environment parameter \"%s\" to \"%s\" because \"%s\"\n",
				s1,
				s3,
				strerror (errnoGet()));
			break;
		}
		strcpy (pEnv, s1);
		strcat (pEnv, "=");
		strcat (pEnv, s3);
		status = putenv (pEnv);
		if (status<0) {
			errPrintf(
				-1L,
				__FILE__,
				__LINE__,
"Failed to set environment parameter \"%s\" to \"%s\" because \"%s\"\n",
				s1,
				s3,
				strerror (errnoGet()));
		}
		/*
		 * vxWorks copies into a private buffer
		 * (this does not match UNIX behavior)
		 */
		free (pEnv);
	    }
            else{
                strcpy(pSymAddr, s3);
	    }
	    break;

	case resDBF_SHORT:	
	    if (sscanf(s3, "%hd", &n_short) != 1) {
	    	errPrintf (
			-1L,
			__FILE__,
			__LINE__,
		      "Resource=%s value=%s conversion to %s failed", 
			s1,
			s3,
			cvt_str[cvType]);
	        continue;
	    }
            *(short *) pSymAddr = n_short;
	    break;

	case resDBF_LONG:
	    if (sscanf(s3, "%ld", &n_long) != 1) {
	    	errPrintf (
			-1L,
			__FILE__,
			__LINE__,
		      "Resource=%s value=%s conversion to %s failed", 
			s1,
			s3,
			cvt_str[cvType]);
	        continue;
	    }
            *(long *) pSymAddr = n_long;
	    break;

	case resDBF_FLOAT:
	    if (sscanf(s3, "%e", &n_float) != 1) {
	    	errPrintf (
			-1L,
			__FILE__,
			__LINE__,
		      "Resource=%s value=%s conversion to %s failed", 
			s1,
			s3,
			cvt_str[cvType]);
	        continue;
	    }
            *(float *) pSymAddr = n_float;
	    break;

	case resDBF_DOUBLE:
	    if (sscanf(s3, "%le", &n_double) != 1) {
	    	errPrintf (
			-1L,
			__FILE__,
			__LINE__,
		      "Resource=%s value=%s conversion to %s failed", 
			s1,
			s3,
			cvt_str[cvType]);
	        continue;
	    }
            *(double *) pSymAddr = n_double;
	    break;

	default:
		errPrintf (
			-1L,
			__FILE__,
			__LINE__,
	"Invalid data type field=%s for resource=%s", 
			s2,
			s1);
	    continue;
	}
    }
    fclose(fp);
    return (0);
}



/*
 * getResourceToken
 */
LOCAL int getResourceToken(FILE *fp, char *pToken, unsigned maxToken)
{
	int status;

	/*
	 * keep reading until we get a token
	 * (and comments have been stripped)
	 */
	while (TRUE) {
		status = getResourceTokenInternal (fp, pToken, maxToken);
		if (status < 0) {
			return status;
		}

		if (pToken[0] != '\0') {
			return status;
		}
	}
}


/*
 * getResourceTokenInternal
 */
LOCAL int getResourceTokenInternal(FILE *fp, char *pToken, unsigned maxToken)
{
        char    formatString[32];
        char    quoteCharString[2];
        int     status;

        quoteCharString[0] = '\0';
        status = fscanf (fp, " %1[\"`'!]", quoteCharString);
	if (status<0) {
		return status;
	}

	switch (quoteCharString[0]) {
	/*
	 * its a comment 
	 * (consume everything up to the next new line) 
	 */
	case '!':
	{
		char tmp[MAX];

                sprintf(formatString, "%%%d[^\n\r\v\f]", sizeof(tmp)-1);
                status = fscanf (fp, "%[^\n\r\v\f]",tmp);
		pToken[0] = '\0';
		if (status<0) {
			return status;
		}
		break;
	}

	/*
	 * its a plain token
	 */
	case '\0':
                sprintf(formatString, " %%%ds", maxToken-1);

                status = fscanf (fp, formatString, pToken);
                if (status!=1) {
			if (status < 0){
				pToken[0] = '\0';
				return status;
			}
                }
		break;

	/*
	 * it was a quoted string
	 */
	default:
                sprintf(
			formatString, 
			"%%%d[^%c]", 
			maxToken-1, 
			quoteCharString[0]);
                status = fscanf (fp, formatString, pToken);
		if (status!=1) {
			if (status < 0){
				pToken[0] = '\0';
				return status;
			}
		}
                sprintf(formatString, "%%1[%c]", quoteCharString[0]);
                status = fscanf (fp, formatString, quoteCharString);
		if (status!=1) {
			errPrintf (
				-1L,
				__FILE__,
				__LINE__,
	"Resource file syntax error: unterminated string \"%s\"",
				pToken);
			pToken[0] = '\0';
			if (status < 0){
				return status;
			}
		}
		break;
        }
	return 0;
}

int dbLoadAscii(char *filename)
{
    if(pdbBase) {
	epicsPrintf("Ascii file was already loaded\n");
	return(-1);
    }
    return(dbAsciiRead(&pdbBase,filename));
}
