/* iocInit.c	ioc initialization */ 
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
 *				
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<memLib.h>
#include	<lstLib.h>
#include	<sysLib.h>
#include	<symLib.h>
#include	<sysSymTbl.h>	/* for sysSymTbl*/
#include	<a_out.h>	/* for N_TEXT */
#include	<stdarg.h>
#include	<stdioLib.h>
#include	<string.h>
#include	<logLib.h>

#include	<sdrHeader.h>
#include	<fast_lock.h>
#include	<choice.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include	<taskwd.h>
#include	<callback.h>
#include	<dbCommon.h>
#include	<dbFldTypes.h>
#include 	<dbRecDes.h>
#include 	<dbRecType.h>
#include	<dbRecords.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<envDefs.h>
#include	<dbBase.h>
/*This module will declare and initilize module_type variables*/
#define MODULE_TYPES_INIT 1
#include        <module_types.h>

static initialized=FALSE;

/* The following is for use by interrupt routines */
int interruptAccept=FALSE;
extern short wakeup_init; /*old IO_EVENT_SCAN*/
struct dbBase *pdbBase=NULL;

/* define forward references*/
extern long dbRead();
long initDrvSup();
long initRecSup();
long initDevSup();
long finishDevSup();
long initDatabase();
long addToSet();
long initialProcess();
long getResources();

int iocInit(pfilename,pResourceFilename)
char * pfilename;
char * pResourceFilename;
{
    long status;
    char name[40];
    long rtnval;
    void (*pdbUserExit)();
    SYM_TYPE type;

    if(initialized) {
	logMsg("iocInit can only be called once\n");
	return(-1);
    }
    coreRelease();
    epicsSetEnvParams();
    status=dbRead(&pdbBase, pfilename);
    if(status!=0) {
	logMsg("iocInit aborting because dbRead failed\n");
	return(-1);
    }

    status=getResources(pResourceFilename);
    if(status!=0) {
	logMsg("iocInit aborting because getResources failed\n");
	return(-1);
    }
    status = iocLogInit();
    if(status!=0){
        logMsg("iocInit Failed to Initialize Ioc Log Client \n");
    }
    initialized = TRUE;
    taskwdInit();
    if(initDrvSup()!=0) logMsg("iocInit: Drivers Failed during Initialization\n");
    if(initRecSup()!=0) logMsg("iocInit: Record Support Failed during Initialization\n");
    if(initDevSup()!=0) logMsg("iocInit: Device Support Failed during Initialization\n");
    ts_init();
    if(initDatabase()!=0) logMsg("iocInit: Database Failed during Initialization\n");
    if(finishDevSup()!=0) logMsg("iocInit: Device Support Failed during Finalization\n");

    /* if user exit exists call it */
    strcpy(name,"_");
    strcat(name,"dbUserExit");
    rtnval = symFindByName(sysSymTbl,name,(void *)&pdbUserExit,&type);
    if(rtnval==OK && (type&N_TEXT!=0)) {
	(*pdbUserExit)();
	logMsg("User Exit was called\n");
    }
    callbackInit();
    scanInit();
    /* wait 1/2 second to make sure all tasks are started*/
    (void)taskDelay(sysClkRateGet()/2);
    interruptAccept=TRUE;
    wakeup_init=TRUE; /*old IO_EVENT_SCAN*/
    if(initialProcess()!=0) logMsg("iocInit: initialProcess Failed\n");
    rsrv_init();
    logMsg("iocInit: All initialization complete\n");

    return(0);
}

static long initDrvSup() /* Locate all driver support entry tables */
{
    char	*pname;
    char	name[40];
    int		i;
    SYM_TYPE	type;
    char	message[100];
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    
    if(!drvSup) {
	status = S_drv_noDrvSup;
	errMessage(status,"drvSup is NULL, i.e. No device drivers are defined");
	return(status);
    }
    for(i=0; i< (drvSup->number); i++) {
	if(!(pname = drvSup->drvetName[i])) continue;
	strcpy(name,"_");
	strcat(name,pname);
	vxstatus = symFindByName(sysSymTbl,name,(void *)&(drvSup->papDrvet[i]),&type);
	if( vxstatus!=OK || ( type&N_TEXT == 0) ) {
	    strcpy(message,"driver entry table not found for ");
	    strcat(message,pname);
	    status = S_drv_noDrvet;
	    errMessage(status,message);
	    continue;
	}
	if(!(drvSup->papDrvet[i]->init)) continue;
	rtnval = (*(drvSup->papDrvet[i]->init))();
	if(status==0) status = rtnval;
    }
    return(status);
}

static long initRecSup()
{
    char	name[40];
    int		i;
    SYM_TYPE	type;
    char	message[100];
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    int		nbytes;
    
    if(!dbRecType) {
	status = S_rectype_noRecs;
	errMessage(status,"dbRecType is NULL, i.e. no record types defined");
	return(status);
    }
    nbytes = sizeof(struct recSup) + dbRecType->number*sizeof(void *);
    recSup = calloc(1,nbytes);
    recSup->number = dbRecType->number;
    recSup->papRset = (void *)((long)recSup + (long)sizeof(struct recSup));

    /* added for Channel Access Links */
    dbCaLinkInit((int) 1);

    for(i=0; i< (recSup->number); i++) {
	if(dbRecType->papName[i] == NULL)continue;
	strcpy(name,"_");
	strcat(name,dbRecType->papName[i]);
	strcat(name,"RSET");
	vxstatus = symFindByName(sysSymTbl,name,
            (void *)(&recSup->papRset[i]),&type);
	if( vxstatus!=OK || ( type&N_TEXT == 0) ) {
	    strcpy(message,"record support entry table not found for ");
	    strcat(message,name);
	    status = S_rec_noRSET;
	    errMessage(status,message);
	    continue;
	}
	if(!(recSup->papRset[i]->init)) continue;
	else {
	    rtnval = (*(recSup->papRset[i]->init))();
	    if(status==0) status = rtnval;
	}
    }

    /* added for Channel Access Links */
    dbCaLinkInit((int) 2);

    return(status);
}

static long initDevSup() /* Locate all device support entry tables */
{
    char	*pname;
    char	name[40];
    int		i,j;
    SYM_TYPE	type;
    char	message[100];
    long	status=0;
    long	rtnval;
    STATUS	vxstatus;
    struct recLoc	*precLoc;
    struct devSup	*pdevSup;
    struct dbCommon	*precord;
    
    if(!devSup) {
	status = S_dev_noDevSup;
	errMessage(status,"devSup is NULL, i.e. No device support is defined");
	return(status);
    }
    for(i=0; i< (devSup->number); i++) {
	if((pdevSup = devSup->papDevSup[i]) == NULL) continue;
	for(j=0; j < (pdevSup->number); j++) {
	    if(!(pname = pdevSup->dsetName[j])) continue;
	    strcpy(name,"_");
	    strcat(name,pname);
	    vxstatus = (long)symFindByName(sysSymTbl,name,
		(void *)&(pdevSup->papDset[j]),&type);
	    if( vxstatus!=OK || ( type&N_TEXT == 0) ) {
                pdevSup->papDset[j]=NULL;
		strcpy(message,"device support entry table not found for ");
		strcat(message,pname);
		status = S_dev_noDSET;
		errMessage(status,message);
		continue;
	    }
	    if(!(pdevSup->papDset[j]->init)) continue;
	    rtnval = (*(pdevSup->papDset[j]->init))(0);
	    if(status==0) status = rtnval;
	}
    
	/* Now initialize dset for each record */
	if(!(precLoc = dbRecords->papRecLoc[i]))continue;
	if(!(pdevSup=GET_DEVSUP(i))) continue;
	for(j=0, ((char *)precord) = precLoc->pFirst;
	    j<precLoc->no_records;
	    j++, ((char *)precord) += precLoc->rec_size ) {
	        /* If NAME is null then skip this record*/
		if(!(precord->name[0])) continue;
		/* Init DSET NOTE that result may be NULL*/
		precord->dset=(struct dset *)GET_PDSET(pdevSup,precord->dtyp);
	}
    }
    return(status);
}

static long finishDevSup() 
{
    int		i,j;
    struct devSup	*pdevSup;
    
    if(!devSup) return(0);
    for(i=0; i< (devSup->number); i++) {
	if((pdevSup = devSup->papDevSup[i]) == NULL) continue;
	for(j=0; j < (pdevSup->number); j++) {
	    if(!(pdevSup->papDset[j])) continue;
	    if(!(pdevSup->papDset[j]->init)) continue;
	    (*(pdevSup->papDset[j]->init))(1);
	}
    
    }
    return(0);
}


static long initDatabase()
{
    char	name[PVNAME_SZ+FLDNAME_SZ+2];
    short	i,j,k;
    char	message[120];
    long	status=0;
    long	rtnval=0;
    short	nset=0;
    short	lookAhead;
    struct recLoc	*precLoc;
    struct rset		*prset;
    struct recTypDes	*precTypDes;
    struct fldDes	*pfldDes;
    struct dbCommon	*precord;
    struct dbAddr	dbAddr;
    struct link		*plink;
    
    if(!dbRecords) {
	status = S_record_noRecords;
	errMessage(status,"No database records are defined");
	return(status);
    }
    for(i=0; i< (dbRecords->number); i++) {
	if(!(precLoc = dbRecords->papRecLoc[i]))continue;
	if(!(prset=GET_PRSET(i))) {
	    strcpy(name,dbRecType->papName[i]);
	    strcat(name,"RSET");
	    strcpy(message,"record support entry table not found for ");
	    strcat(message,name);
	    status = S_rec_noRSET;
	    errMessage(status,message);
	    continue;
	}
	precTypDes = dbRecDes->papRecTypDes[i];
	for(j=0, ((char *)precord) = precLoc->pFirst;
	    j<precLoc->no_records;
	    j++, ((char *)precord) += precLoc->rec_size ) {
	        /* If NAME is null then skip this record*/
		if(!(precord->name[0])) continue;

		/*initialize fields rset*/
		(struct rset *)(precord->rset) = prset;

	        /* initialize mlok and mlis*/
		FASTLOCKINIT(&precord->mlok);
		lstInit(&(precord->mlis));
		precord->pact=FALSE;

		/* set lset=0 See determine lock set below*/
		precord->lset = 0;

		/* Convert all PV_LINKs to DB_LINKs or CA_LINKs*/
		for(k=0; k<precTypDes->no_links; k++) {
		    pfldDes = precTypDes->papFldDes[precTypDes->link_ind[k]];
		    plink = (struct link *)((char *)precord + pfldDes->offset);
		    if(plink->type == PV_LINK) {
			strncpy(name,plink->value.pv_link.pvname,PVNAME_SZ);
			name[PVNAME_SZ]=0;
			strcat(name,".");
			strncat(name,plink->value.pv_link.fldname,FLDNAME_SZ);
			if(dbNameToAddr(name,&dbAddr) == 0) {
			    /* show that refered to record has link. */
			    /* See determine lock set below.*/
			    if( pfldDes->field_type!=DBF_INLINK
				|| plink->value.pv_link.process_passive
				|| dbAddr.no_elements>1 )
			    	((struct dbCommon *)(dbAddr.precord))->lset= -1;
			    plink->type = DB_LINK;
			    plink->value.db_link.pdbAddr =
				calloc(1,sizeof(struct dbAddr));
			    *((struct dbAddr *)(plink->value.db_link.pdbAddr))=dbAddr;
			}
			else {
                            /* not a local pvar ... assuming a CA_LINK */
                            /* only supporting NPP, Input MS/NMS, and  */
                            /* Output NMS links ... checking here.     */

                            if (plink->value.db_link.process_passive
                                || (pfldDes->field_type == DBF_OUTLINK
                                    && plink->value.db_link.maximize_sevr))
                            {
                                /* link PP and/or Outlink MS ...    */
                                /* neither supported under CA_LINKs */
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

		/* call record support init_record routine */
		if(!(recSup->papRset[i]->init_record)) continue;
		rtnval = (*(recSup->papRset[i]->init_record))(precord);
		if(status==0) status = rtnval;
	}
    }

    /* Now determine lock sets*/
    /* When each record is examined lset has one of the following values
     * -1   Record is not in a set and at least one following record refers
     *      to this record unless INLINK && no_elements<=1 && !process_passive
     *  0   record is not in a set and no following records refer to it.
     * >0   Record is already in a set
     */
    for(i=0; i<dbRecords->number; i++) {
	if(!(precLoc = dbRecords->papRecLoc[i]))continue;
	precTypDes = dbRecDes->papRecTypDes[i];
	for(j=0, ((char *)precord) = precLoc->pFirst;
	    j<precLoc->no_records;
	    j++, ((char *)precord) += precLoc->rec_size ) {
	        /* If NAME is null then skip this record*/
		if(!(precord->name[0])) continue;
		if(precord->lset > 0) continue; /*already in a lock set */
		lookAhead = ( (precord->lset == -1) ? TRUE : FALSE);
		nset++;
		rtnval = addToSet(precord,i,lookAhead,i,j,nset);
		if(status==0) status=rtnval;
		if(rtnval) return(status); /*I really mean rtnval*/
	}
    }    
    dbScanLockInit(nset);
    return(status);
}

static long addToSet(precord,record_type,lookAhead,i,j,lset)
    struct dbCommon *precord;	/* record being added to lock set*/
    short  record_type;		/* record being added to lock set*/
    short lookAhead;		/*should following records be checked*/
    short  i;		/*record before 1st following: index into papRecLoc*/
    short  j;		/*record before 1st following: record number	*/
    short  lset;	/* current lock set		*/
{
    short  k,in,jn,j1st;
    long status;
    struct fldDes       *pfldDes;
    struct link		*plink;
    struct recTypDes	*precTypDes;
    struct recLoc	*precLoc;

    if(precord->lset > 0) {
	status = S_db_lsetLogic;
	errMessage(status,"Logic Error in iocInit(addToSet)");
	return(status);
    }
    precord->lset = lset;
   /* add all DB_LINKs in this record to the set */
   /* unless not process_passive or no_elements>1*/
    precTypDes = dbRecDes->papRecTypDes[record_type];
    for(k=0; k<precTypDes->no_links; k++) {
	struct dbAddr	*pdbAddr;
	struct dbCommon	*pk;

	pfldDes = precTypDes->papFldDes[precTypDes->link_ind[k]];
	plink = (struct link *)((char *)precord + pfldDes->offset);
	if(plink->type != DB_LINK) continue;
	pdbAddr = (struct dbAddr *)(plink->value.db_link.pdbAddr);
	if( pfldDes->field_type==DBF_INLINK
	    && !(plink->value.db_link.process_passive)
	    && pdbAddr->no_elements<=1) continue;
	pk = (struct dbCommon *)(pdbAddr->precord);
	if(pk->lset > 0){
		if(pk->lset == lset) continue; /*already in lock set*/
		status = S_db_lsetLogic;
		errMessage(status,"Logic Error in iocInit(addToSet)");
		return(status);
	}
	status = addToSet(pk,pdbAddr->record_type,TRUE,i,j,lset);
	if(status) return(status);
    }
    /* Now look for all later records that refer to this record*/
    /* unless not process_passive or no_elements>1*/
    /* remember that all earlier records already have lock set determined*/
    if(!lookAhead) return(0);
    j1st=j+1; 
    for(in=i; in<dbRecords->number; in++) {
	struct dbCommon	*pn;

	if(!(precLoc = dbRecords->papRecLoc[in])) continue;
	precTypDes = dbRecDes->papRecTypDes[in];
	for(jn=j1st,
	    (char *)pn= (char *)(precLoc->pFirst) + jn*(precLoc->rec_size);
	    jn<precLoc->no_records;
	    jn++, ((char *)pn) += precLoc->rec_size)  {
		/* If NAME is null then skip this record*/
                if(!(pn->name[0])) continue;
		for(k=0; k<precTypDes->no_links; k++) {
		    struct dbAddr	*pdbAddr;
		    struct dbCommon	*pk;

		    pfldDes = precTypDes->papFldDes[precTypDes->link_ind[k]];
		    plink = (struct link *)((char *)pn + pfldDes->offset);
		    if(plink->type != DB_LINK) continue;
		    pdbAddr = (struct dbAddr *)(plink->value.db_link.pdbAddr);
		    if( pfldDes->field_type==DBF_INLINK
		        && !(plink->value.db_link.process_passive)
			&& pdbAddr->no_elements<=1 ) continue;
		    pk = (struct dbCommon *)(pdbAddr->precord);
		    if(pk != precord) continue;
		    if(pn->lset > 0) {
			if(pn->lset == lset) continue;
			status = S_db_lsetLogic;
			errMessage(status,"Logic Error in iocInit(addToSet)");
			return(status);
		    }
		    status = addToSet(pn,in,TRUE,i,j,lset);
		    if(status) return(status);
		}
	}
	j1st = 0;
    }
    return(0);
}

static long initialProcess()
{
    short	i,j;
    struct recLoc	*precLoc;
    struct dbCommon	*precord;
    
    if(!dbRecords) return(0);
    for(i=0; i< (dbRecords->number); i++) {
	if(!(precLoc = dbRecords->papRecLoc[i]))continue;
	for(j=0, ((char *)precord) = precLoc->pFirst;
	    j<precLoc->no_records;
	    j++, ((char *)precord) += precLoc->rec_size ) {
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
static long getResources(fname) /* Resource Definition File interpreter */
    char           *fname;
{
    FILE            *fp;
    FILE            *fp2;
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
    UTINY           type;
    char           *pSymAddr;
    short           n_short;
    long            n_long;
    float           n_float;
    double          n_double;
     if (sdrSum) {
 	if ((fp2 = fopen("default.sdrSum", "r")) == 0) {
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "Can't open default.sdrSum file.  Please invoke getrel");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    return (-1);
 	}
 	fgets( buff, MAX, fp2);
 	fclose(fp2);
 	len2 = strlen(sdrSum->allSdrSums);
 
 	if ((strncmp(sdrSum->allSdrSums, buff, len2)) != SAME) {
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "THIS DATABASE IS OUT_OF_DATE");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    errMessage(-1L, "WARNING WARNING WARNING WARNING WARNING WARNING WARNING");
 	    return (-1);
 	}
     } else {
 	logMsg("Skipping Check for an out-of-date database\n");
     }
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
