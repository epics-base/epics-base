
/* iocInit.c	ioc initialization */ 
/* share/src/db $Id$ */

#include	<vxWorks.h>
#include	<types.h>
#include	<lstLib.h>
#include	<memLib.h>
#include	<sysLib.h>
#include	<symLib.h>
#include	<sysSymTbl.h>	/* for sysSymTbl*/
#include	<a_out.h>	/* for N_TEXT */
#include	<stdioLib.h>
#include	<strLib.h>

#include	<fast_lock.h>
#include	<choice.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbCommon.h>
#include	<dbFldTypes.h>
#include 	<dbRecDes.h>
#include 	<dbRecType.h>
#include	<dbRecords.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>

static initialized=FALSE;

/* define forward references*/
extern long initBusController();
extern long sdrLoad();
extern long initDrvSup();
extern long initRecSup();
extern long initDevSup();
extern long initDatabase();
extern long addToSet();


iocInit(pfilename)
char * pfilename;
{
    long status;

    if(initialized) {
	logMsg("iocInit can only be called once\n");
	return(-1);
    }
    if(status=initBusController()) {
	logMsg("iocInit aborting because initBusController failed\n");
	return(-1);
    }
    if(status=sdrLoad(pfilename)) {
	logMsg("iocInit aborting because sdrLoad failed\n");
	return(-1);
    }
    initialized = TRUE;
    logMsg("sdrLoad completed\n");
    /* enable interrupt level 5 and 6 */
    sysIntEnable(5);
    sysIntEnable(6);
    if(initDrvSup()==0) logMsg("Drivers Initialized\n");
    if(initRecSup()==0) logMsg("Record Support Initialized\n");
    if(initDevSup()==0) logMsg("Device Support Initialized\n");
    ts_init(); logMsg("Time Stamp Driver Initialized\n");
    if(initDatabase()==0) logMsg("Database Initialized\n");
    scan_init(); logMsg("Scanners Initialized\n");
    rsrv_init(); logMsg("Channel Access Servers Initialized\n");
    logMsg("iocInit: All initialization complete\n");

    return(0);
}

#include	<module_types.h>

long initBusController(){ /*static */
    char	ctemp;

    /* initialize the  Xycom SRM010 bus controller card */
    ctemp = XY_LED;
    if (vxMemProbe(SRM010_ADDR, WRITE,1,&ctemp) == -1) {
    	logMsg("Xycom SRM010 Bus Controller Not Present\n");
    	return(-1);
    }
    return(0);
}

long initDrvSup() /* Locate all driver support entry tables */
{
    char	*pname;
    char	name[40];
    int		i;
    UTINY	type;
    char	message[100];
    long	status;
    long	rtnval=0;
    
    if(!drvSup) {
	status = S_drv_noDrvSup;
	errMessage(status,"drvSup is NULL, i.e. No device drivers are defined");
	return(status);
    }
    for(i=0; i< (drvSup->number); i++) {
	if(!(pname = drvSup->drvetName[i])) continue;
	strcpy(name,"_");
	strcat(name,pname);
	rtnval = symFindByName(sysSymTbl,name,&(drvSup->papDrvet[i]),&type);
	if( rtnval!=OK || ( type&N_TEXT == 0) ) {
	    strcpy(message,"driver entry table not found for ");
	    strcat(message,pname);
	    status = S_drv_noDrvet;
	    errMessage(-1L,message);
	    if(rtnval==OK) rtnval=status;
	    continue;
	}
	status = (*(drvSup->papDrvet[i]->init))();
	if(rtnval==OK) rtnval=status;
    }
    return(rtnval);
}

long initRecSup()
{
    char	name[40];
    int		i;
    UTINY	type;
    char	message[100];
    long	status;
    long	rtnval=0; /*rtnval will be 0 or first error found*/
    int		nbytes;
    
    if(!dbRecType) {
	status = S_rectype_noRecs;
	errMessage(status,"dbRecType is NULL, i.e. no record types defined");
	return(status);
    }
    nbytes = sizeof(struct recSup) + dbRecType->number*sizeof(caddr_t);
    recSup = (struct recSup *)calloc(1,nbytes);
    recSup->number = dbRecType->number;
    (long)recSup->papRset = (long)recSup + (long)sizeof(struct recSup);
    for(i=0; i< (recSup->number); i++) {
	if(dbRecType->papName[i] == NULL)continue;
	strcpy(name,"_");
	strcat(name,dbRecType->papName[i]);
	strcat(name,"RSET");
	rtnval = symFindByName(sysSymTbl,name,&(recSup->papRset[i]),&type);
	if( rtnval!=OK || ( type&N_TEXT == 0) ) {
	    strcpy(message,"record support entry table not found for ");
	    strcat(message,name);
	    status = S_rec_noRSET;
	    errMessage(-1L,message);
	    if(rtnval==OK)rtnval=status;
	    continue;
	}
	if(!(recSup->papRset[i]->init)) continue;
	else {
	    status = (*(recSup->papRset[i]->init))();
	    if(rtnval==OK)rtnval=status;
	}
    }
    return(rtnval);
}

long initDevSup() /* Locate all device support entry tables */
{
    char	*pname;
    char	name[40];
    int		i,j;
    UTINY	type;
    char	message[100];
    long	status;
    long	rtnval=0; /*rtnval will be 0 or first error found*/
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
	    rtnval = (long)symFindByName(sysSymTbl,name,
		&(pdevSup->papDset[j]),&type);
	    if( rtnval!=OK || ( type&N_TEXT == 0) ) {
		strcpy(message,"device support entry table not found for ");
		strcat(message,pname);
		status = S_dev_noDSET;
		errMessage(-1L,message);
		if(rtnval==OK)rtnval=status;
		continue;
	    }
	    if(!(pdevSup->papDset[j]->init)) continue;
	    status = (*(pdevSup->papDset[j]->init))();
	    if(rtnval==OK)rtnval=status;
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
    return(rtnval);
}

long initDatabase()
{
    char	name[PVNAME_SZ+FLDNAME_SZ+2];
    short	i,j,k;
    char	message[120];
    long	status;
    long	rtnval=0; /*rtnval will be 0 or first error found*/
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
	    errMessage(-1L,message);
	    if(rtnval==OK) rtnval = status;
	    continue;
	}
	precTypDes = dbRecDes->papRecTypDes[i];
	for(j=0, ((char *)precord) = precLoc->pFirst;
	    j<precLoc->no_records;
	    j++, ((char *)precord) += precLoc->rec_size ) {
	        /* If NAME is null then skip this record*/
		if(!(precord->name[0])) continue;

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
			    ((struct dbCommon *)(dbAddr.precord))->lset = -1;
			    plink->type = DB_LINK;
			    plink->value.db_link.pdbAddr =
				(caddr_t)calloc(1,sizeof(struct dbAddr));
			    *((struct dbAddr *)(plink->value.db_link.pdbAddr))=dbAddr;
			}
			else {
			    /*This will be replaced by channel access call*/
			    strncpy(message,precord->name,PVNAME_SZ);
			    message[PVNAME_SZ]=0;
			    strcat(message,".");
			    strncat(message,pfldDes->fldname,FLDNAME_SZ);
			    strcat(message,": link process variable =");
			    strcat(message,name);
			    strcat(message," not found");
			    status = S_db_notFound;
			    errMessage(-1L,message);
			    if(rtnval==OK) rtnval=status;
			}
		    }
		}

		/* call record support init_record routine */
		if(!(recSup->papRset[i]->init_record)) continue;
		status = (*(recSup->papRset[i]->init_record))(precord);
		if(rtnval==OK)rtnval=status;
	}
    }

    /* Now determine lock sets*/
    /* When each record is examined lset has one of the following values
     * -1   Record is not in a set and at least one following record refers
     *      to this record
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
		status = addToSet(precord,i,lookAhead,i,j,nset);
		if(status) return(status);
	}
    }    
    dbScanLockInit(nset);
    return(rtnval);
}

long addToSet(precord,record_type,lookAhead,i,j,lset)
    struct dbCommon *precord;	/* record being added to lock set*/
    short  record_type;		/* record being added to lock set*/
    short lookAhead;		/*should following records be checked*/
    short  i;		/*record before 1st following: index into papRecLoc*/
    short  j;		/*record before 1st following: record number	*/
    short  lset;	/* current lock set		*/
{
    short  k,in,itemp,jn,j1st;
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
    precTypDes = dbRecDes->papRecTypDes[record_type];
    for(k=0; k<precTypDes->no_links; k++) {
	struct dbCommon	*pk;

	pfldDes = precTypDes->papFldDes[precTypDes->link_ind[k]];
	plink = (struct link *)((char *)precord + pfldDes->offset);
	if(plink->type != DB_LINK) continue;
	pk = (struct dbCommon *)
		(((struct dbAddr *)(plink->value.db_link.pdbAddr))->precord);
	if(pk->lset > 0){
		if(pk->lset == lset) continue; /*already in lock set*/
		status = S_db_lsetLogic;
		errMessage(status,"Logic Error in iocInit(addToSet)");
		return(status);
	}
	status = addToSet(pk,
		((struct dbAddr *)(plink->value.db_link.pdbAddr))->record_type,
		TRUE,i,j,lset);
	if(status) return(status);
    }
    /* Now look for all later records that refer to this record*/
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
		    struct dbCommon	*pk;

		    pfldDes = precTypDes->papFldDes[precTypDes->link_ind[k]];
		    plink = (struct link *)((char *)pn + pfldDes->offset);
		    if(plink->type != DB_LINK) continue;
		    pk = (struct dbCommon *)
			(((struct dbAddr *)(plink->value.db_link.pdbAddr))->precord);
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
