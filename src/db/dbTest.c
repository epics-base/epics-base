/* dbTest.c */
/* share/src/db  $Id$ */
/*	database access test subroutines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            11-7-90
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
 * .01  08-13-91	mrk	Added extra NULL arg to dbGetField calls
 * .02  10-23-91	mrk	Changed dbior so it also reports device support
 * .03  11-26-91	jba	Fixed initializations and added hex print to printBuffer
 * .04  02-05-92	jba	Changed function arguments from paddr to precord 
 * .05  02-28-92        jba     ANSI C changes
 * .06  04-17-92        rcz     removed dbPvd for mrk
 * .07  05-19-92        mrk	Mods for changes to internal database structures
 */

/* Global Database Test Routines - All can be invoked via vxWorks shell
 *
 * dba(pname)			Print dbAddr info
 *	char	*pname		Pvname
 *
 * dbl(ptypeName)		list record names.
 *	char	*ptypeName;	Record type. If null all record types
 *
 * dbgf(pname)			get field
 *	char	*pname;
 *
 * dbpf(pname,pvalue)		put field
 *	char	*pname;
 *	char	*pvalue
 *
 * dbpr(pname)			print record
 *	char	*pname;
 *
 * dbtr(pname)			test record and print
 *	char	*pname;
 *
 * dbtgf(pname)			test get field
 *	char	*pname;
 *
 * dbtpf(pname,pvalue)		test put field
 *	char	*pname;
 *	char	*pvalue
 *
 * dbior(pname,type)		io_report
 *	char	*pname		Driver name. If null all drivers
 *	int	type		<0,1> => <short, full> report
 *
 * dblls(ptypeName)		list lock sets
 *	char	*ptypeName;	Record type. If null all record types
 *
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<string.h>
#include	<stdioLib.h>

#include        <lstLib.h>
#include	<fast_lock.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbBase.h>
#include	<dbRecType.h>
#include	<dbRecords.h>
#include	<dbCommon.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<choice.h>
#include	<special.h>
#include	<dbRecDes.h>
#include	<dbManipulate.h>

extern struct dbBase *pdbBase;

#define MAXLINE 80
struct msgBuff {		/* line output structure */
    char            out_buff[MAXLINE + 1];
    char           *pNext;
    char           *pLast;
    char           *pNexTab;
    char            message[128];
};
typedef struct msgBuff TAB_BUFFER;

#define MIN(x,y)        ((x < y)?x:y)
#define MAX(x,y)        ((x > y)?x:y)

/* Local Routines */
void	printDbAddr();
void	printBuffer();
int	dbpr_report();
void	dbpr_msgOut();
void	dbpr_init_msg();
void	dbpr_insert_msg();
void	dbpr_msg_flush();
void	dbprReportLink();
void	dbprReportCvtChoice();
int	dbprReportGblChoice();
void	dbprReportRecChoice();
void	dbprReportDevChoice();
struct fldDes *dbprGetFldRec();
struct recTypDes *dbprGetRecTypDes();


long dba(pname)	/* get and print dbAddr info */
	char	*pname;
{
    struct dbAddr 	addr;
    long		status;

    status=dbNameToAddr(pname,&addr);
    printDbAddr(status,&addr);
    if(status) return(1); else return(0);
}

long dbl(ptypeName)	/* list process variables for specified record type*/
	char	*ptypeName;
{
    int			rectype,beg,end;
    struct recLoc	*precLoc;
    struct dbCommon	*precord;
    char		*pstr;
    char		name[PVNAME_SZ+1];
    struct recType	*precType;
    struct recHeader	*precHeader;
    RECNODE		*precNode;

    if(!pdbBase) {
	printf("No database\n");
	return(0);
    }
    if(!(precType=pdbBase->precType)) return(0);
    if(!(precHeader = pdbBase->precHeader)) return(0);
    if(ptypeName==NULL) {
	beg=0;
	end=precHeader->number - 1;
    }
    else {
	for(rectype=0; rectype<precHeader->number; rectype++) {
	    if(!(pstr=GET_PRECNAME(precType,rectype))) continue;
	    if(strcmp(pstr,ptypeName)==0){
		beg=rectype;
		end=rectype;
		goto got_it;
	    }
	}
	printf("Illegal Record Type\n");
	return(1);
    }
got_it:
    for(rectype=beg; rectype<=end; rectype++) {
	if(!(precLoc=GET_PRECLOC(precHeader,rectype))) continue;
	for(precNode=(RECNODE *)lstFirst(precLoc->preclist);
	    precNode; precNode = (RECNODE *)lstNext(&precNode->next)) {
		precord = precNode->precord;
		if(precord->name[0] == 0) continue; /*deleted record*/
		strncpy(name,precord->name,PVNAME_SZ);
		name[PVNAME_SZ]=0;
		printf("%s\n",name);
	}
    }
    return(0);
}

long dbgf(pname)	/* get field value*/
	char	*pname;
{
    /* declare buffer long just to ensure correct alignment */
    long 		buffer[100];
    long 		*pbuffer=&buffer[0];
    struct dbAddr 	addr;
    long		status;
    long		options,no_elements;
    static TAB_BUFFER 	msg_Buff;
    TAB_BUFFER     	*pMsgBuff = &msg_Buff;
    char 		*pmsg = pMsgBuff->message;
    int             	tab_size;
    tab_size = 10;

    status=dbNameToAddr(pname,&addr);
    if(status) {
	printf("dbNameToAddr failed\n");
	return(1);
    }
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
    options=0;
    if(addr.dbr_field_type==DBR_ENUM) {
	status=dbGetField(&addr,DBR_STRING,pbuffer,&options,&no_elements,NULL);
	printBuffer(status,DBR_STRING,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    }
    else {
	status=dbGetField(&addr,addr.dbr_field_type,pbuffer,&options,&no_elements,NULL);
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    }
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}


/* vxWorks 4.x does not support strspn. Here it is*/
static int strspn(ps,pp)
char	*ps;	/* pointer to source*/
char	*pp;	/* pointer to pattern*/
{
	int i;
	char *p1=ps;
	char *p2;

	for (i=0; *p1; i++, p1++) {
		for (p2=pp; *p2; p2++) {
			if(*p1==*p2) goto ok;
		}
		return(i);
ok:
	continue;
	}
	return strlen(ps);
}

long dbpf(pname,pvalue)	/* put field value*/
	char	*pname;
	char	*pvalue;
{
    /* declare buffer long just to ensure correct alignment */
    struct dbAddr addr;
    long	  status;

    /* make sure value was passed*/
    if(pvalue==0) {
	errMessage(0L,"No value was specified");
	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
         errMessage(status,"dbNameToAddr error");
         return(1);
    }
    /* For enumerated types must allow for ENUM rather than string*/
    /* If entire field is digits then use DBR_ENUM else DBR_STRING*/
    if((addr.dbr_field_type==DBR_ENUM) && (*pvalue!=0)
    &&  (strspn(pvalue,"0123456789")==strlen(pvalue))) {
	    short value;

	    sscanf(pvalue,"%hu",&value);
	    status=dbPutField(&addr,DBR_ENUM,&value,1L);
    } else {
	status=dbPutField(&addr,DBR_STRING,pvalue,1L);
    }
    if(status) {
         errMessage(status,"dbPutField error");
         return(1);
    }
    if(status!=0) errPrint(status);
    else status=dbgf(pname);
    return(status);
}

long dbpr(pname, interest_level)	/* print record */
    char           *pname;
    int             interest_level;
{
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER       *pMsgBuff = &msg_Buff;
    struct dbAddr    addr;
    long             status;
    char            *pmsg;
    int              tab_size;

    pmsg = pMsgBuff->message;
    tab_size = 20;

    status = dbNameToAddr(pname, &addr);
    if(status) {
	printf("dbNameToAddr failed\n");
	return(1);
    }
    if (dbpr_report(pname, &addr, interest_level, pMsgBuff, tab_size))
	return (1);
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return (0);
}

long dbtr(pname)	/* test record and print*/
	char	*pname;
{
    struct dbAddr addr;
    long 	     status;
    struct dbCommon  *precord;

    status=dbNameToAddr(pname,&addr);
    if(status) {
	printf("dbNameToAddr failed\n");
	return(1);
    }
    precord=(struct dbCommon*)(addr.precord);
    if (precord->pact) {
        printf("record active\n");
        return(1);
    }
    if(FASTLOCKTEST(&precord->mlok)) {
        printf("record locked\n");
        return(1);
    }
    status=dbProcess(precord);
    if(!(RTN_SUCCESS(status)))
	recGblRecSupError(S_db_noSupport,&addr,"dbtr","process");
    dbpr(pname,3);
    return(0);
}

long dbtgf(pname)	/* test all options for dbGetField */
	char	*pname;
{
    /* declare buffer long just to ensure correct alignment */
    long              buffer[400];
    long              *pbuffer=&buffer[0];
    struct dbAddr     addr;
    long	      status;
    long	      req_options,ret_options,no_elements;
    short	      dbr_type;
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER        *pMsgBuff = &msg_Buff;
    char              *pmsg = pMsgBuff->message;
    int               tab_size;

    tab_size = 10;

    status=dbNameToAddr(pname,&addr);
    if(status) {
	printf("dbNameToAddr failed\n");
	return(1);
    }
    /* try all options first */
    req_options=0xffffffff;
    ret_options=req_options;
    no_elements=0;
    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
	&ret_options,&no_elements,NULL);
    printBuffer(status,addr.dbr_field_type,pbuffer, req_options,ret_options,no_elements,pMsgBuff,tab_size);
    /* Now try all request types */
    ret_options=0;
    dbr_type=DBR_STRING;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/MAX_STRING_SIZE));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_CHAR;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(char)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_UCHAR;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(unsigned char)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_SHORT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_USHORT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(unsigned short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_LONG;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(long)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_ULONG;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(unsigned long)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_FLOAT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(float)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_DOUBLE;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(double)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_ENUM;
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/sizeof(unsigned short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

long dbtpf(pname,pvalue)/* test all options for dbPutField */
	char	*pname;
	char	*pvalue;
{
    /* declare buffer long just to ensure correct alignment */
    long          buffer[100];
    long          *pbuffer=&buffer[0];
    struct dbAddr addr;
    long	  status;
    long	  options,no_elements;
    char 	  cvalue;
    unsigned char ucvalue;
    short	  svalue;
    unsigned short usvalue;
    long	  lvalue;
    unsigned long ulvalue;
    float	  fvalue;
    double	  dvalue;
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER     *pMsgBuff = &msg_Buff;
    char          *pmsg = pMsgBuff->message;
    int           tab_size;

    tab_size = 10;

    status=dbNameToAddr(pname,&addr);
    if(status) {
	printf("dbNameToAddr failed\n");
	return(1);
    }
    /* DBR_STRING */
    status=dbPutField(&addr,DBR_STRING,pvalue,1L);
    if(status!=0) errPrint(status);
    else {
	printf("DBR_STRING ok\n");
	no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	options=0;
	status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    }
    /* DBR_CHAR */
    if(sscanf(pvalue,"%hd",&svalue)==1) {
	cvalue = (char)svalue;
	status=dbPutField(&addr,DBR_CHAR,&cvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_UCHAR ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_CHAR\n");
    /* DBR_UCHAR */
    if(sscanf(pvalue,"%hu",&usvalue)==1) {
	ucvalue = (unsigned char)usvalue;
	status=dbPutField(&addr,DBR_UCHAR,&ucvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_UCHAR ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_UCHAR\n");
    /* DBR_SHORT */
    if(sscanf(pvalue,"%hd",&svalue)==1) {
	status=dbPutField(&addr,DBR_SHORT,&svalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_SHORT ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_SHORT\n");
    /* DBR_USHORT */
    if(sscanf(pvalue,"%hu",&usvalue)==1) {
	status=dbPutField(&addr,DBR_USHORT,&usvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_USHORT ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_USHORT\n");
    /* DBR_LONG */
    if(sscanf(pvalue,"%ld",&lvalue)==1) {
	status=dbPutField(&addr,DBR_LONG,&lvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_LONG ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_LONG\n");
    /* DBR_ULONG */
    if(sscanf(pvalue,"%lu",&ulvalue)==1) {
	status=dbPutField(&addr,DBR_ULONG,&ulvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_ULONG ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_ULONG\n");
    /* DBR_FLOAT */
    if(sscanf(pvalue,"%e",&fvalue)==1) {
	status=dbPutField(&addr,DBR_FLOAT,&fvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_FLOAT ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_FLOAT\n");
    /* DBR_DOUBLE */
    if(sscanf(pvalue,"%le",&dvalue)==1) {
	status=dbPutField(&addr,DBR_DOUBLE,&dvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_DOUBLE ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_DOUBLE\n");
    /* DBR_ENUM */
    if(sscanf(pvalue,"%hu",&svalue)==1) {
	status=dbPutField(&addr,DBR_ENUM,&svalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_ENUM ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_SHORT\n");
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

long dbior(pdrvName,type)
    char	*pdrvName;
    int		type;
{
    int		 	i,j;
    char		*pname;
    struct drvSup	*pdrvSup;
    struct recDevSup	*precDevSup;
    struct drvet	*pdrvet;

    if(!pdbBase) {
	printf("No database\n");
	return(0);
    }
    if(!(pdrvSup= pdbBase->pdrvSup)) {
	printf("No drivers\n");
	return(0);
    }
    for (i=0; i<pdrvSup->number; i++) {
	if((pname=pdrvSup->papDrvName[i])==NULL) continue;
	if(pdrvName!=NULL && (strcmp(pdrvName,pname)!=0)) continue;
	if((pdrvet=pdrvSup->papDrvet[i])==NULL) {
		printf("No driver entry table is present for %s\n",pname);
		continue;
	}
	if(pdrvet->report==NULL)
	    printf("Driver: %s No report available\n",pname);
	else {
	    printf("Driver: %s\n",pname);
	    (*pdrvet->report)(type);
	}
    }
    /* now check devSup reports */
    if(!(precDevSup=pdbBase->precDevSup)) {
	printf("No device support\n");
	return(0);
    }
    for (i=0; i<precDevSup->number; i++) {
	struct devSup	*pdevSup;

	if(!(pdevSup=GET_PDEVSUP(precDevSup,i))) continue;
	for(j=0; j<pdevSup->number; j++) {
	    struct dset	*pdset;

	    if(!(pdset=GET_PDSET(pdevSup,j))) continue;
	    if((pname=pdevSup->papDsetName[j])==NULL) continue;
	    if(pdrvName!=NULL && (strcmp(pdrvName,pname)!=0)) continue;
	    if(pdset->report!=NULL) {
		printf("Device Support: %s\n",pname);
	    		(*pdset->report)(type);
	    }
	}
    }
    return(0);
}

long dblls(lockset)	/* list lock set for specified record type*/
	int	lockset;
{
    int			rectype,beg,end,recnum;
    struct recLoc	*precLoc;
    struct dbCommon	*precord;
    char		*pstr;
    char		name[PVNAME_SZ+1];
    struct recType      *precType;
    struct recHeader    *precHeader;
    RECNODE             *precNode;

    if(!(precType=pdbBase->precType)) return(0);
    if(!(precHeader = pdbBase->precHeader)) return(0);
    beg=0;
    end=precHeader->number - 1;
    printf(" lset  lcnt  disv  disa  pact\n");
    for(rectype=beg; rectype<=end; rectype++) {
        if(!(precLoc=GET_PRECLOC(precHeader,rectype))) continue;
        for(precNode=(RECNODE *)lstFirst(precLoc->preclist);
            precNode; precNode = (RECNODE *)lstNext(&precNode->next)) {
                precord = precNode->precord;
		if(precord->name[0] == 0) continue; /*deleted record*/
		if(lockset>0 && lockset!=precord->lset) continue;
		strncpy(name,precord->name,PVNAME_SZ);
		name[PVNAME_SZ]=0;
		printf("%5.5d %5.5d %5.5d %5.5d %5.5d %s\n",
			precord->lset,
			precord->lcnt,
			precord->disv,
			precord->disa,
			precord->pact,
			name);
	}
    }
    return(0);
}

static char *dbf[DBF_NTYPES]={
	"STRING","CHAR","UCHAR","SHORT","USHORT","LONG","ULONG",
	"FLOAT","DOUBLE","ENUM","GBLCHOICE","CVTCHOICE","RECCHOICE",
	"DEVCHOICE","INLINK","OUTLINK","FWDLINK","NOACCESS"};

static char *dbr[DBR_ENUM+2]={
	"STRING","CHAR","UCHAR","SHORT","USHORT","LONG","ULONG",
	"FLOAT","DOUBLE","ENUM","NOACCESS"};

static void printDbAddr(status,paddr)
    long	  status;
    struct dbAddr *paddr;
{
    char	*pstr;
    short	field_type;
    short	dbr_field_type;

    if(status!=0) {
	errMessage(status,"dbNameToAddr error");
    }
    printf("Record Address: 0x%x",paddr->precord);
    printf(" Field Address: 0x%x",paddr->pfield);
    printf(" Field Description: 0x%x\n",paddr->pfldDes);
    printf("   No Elements: %ld\n",paddr->no_elements);
    if (!(pstr=GET_PRECNAME(pdbBase->precType,paddr->record_type))) 
    	printf("   Record Type: %d\n",paddr->record_type);
    else
	printf("   Record Type: %s\n",pstr);
    printf("     FieldType: DBF_");
    field_type = paddr->field_type;
    if(field_type<0 || field_type>DBR_NOACCESS)
	printf(" Illegal = %d\n",field_type);
    else
	printf("%s\n",dbf[field_type]);
    printf("    Field Type: %d\n",paddr->field_type);
    printf("    Field Size: %d\n",paddr->field_size);
    printf("       Special: %d\n",paddr->special);
    printf("    Choice Set: %d\n",paddr->choice_set);
    printf("DBR Field Type: DBR_");
    dbr_field_type = paddr->dbr_field_type;
    if(dbr_field_type==DBR_NOACCESS)dbr_field_type=DBR_ENUM + 1;
    if(dbr_field_type<0 || dbr_field_type>(DBR_ENUM+1))
	printf(" Illegal = %d\n",dbr_field_type);
    else
	printf("%s\n",dbr[dbr_field_type]);
}


static void printBuffer(status, dbr_type, pbuffer, reqOptions,
		 retOptions, no_elements, pMsgBuff, tab_size)
    long            status;
    short           dbr_type;
    char           *pbuffer;
    long            reqOptions;
    long            retOptions;
    long            no_elements;
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{
    unsigned short  stat,
                    severity;
    long            precision;
    unsigned long   epoch_seconds;
    unsigned long   nano_seconds;
    unsigned long   no_strs;
    short           svalue;
    unsigned short  usarr[6];
    unsigned short  usvalue;
    char           *pmsg;
    int             i,
                    len;
    pmsg = pMsgBuff->message;

    if (reqOptions & DBR_STATUS) {
	if (retOptions & DBR_STATUS) {
	    stat = *((unsigned short *) pbuffer);
	    severity = *((unsigned short *) (pbuffer + 2));
	    printf("status=%u severity=%u\n", stat, severity);
	} else
	    printf("status and severity not returned\n");
	pbuffer += dbr_status_size;
    }
    if (reqOptions & DBR_UNITS) {
	if (retOptions & DBR_UNITS)
	    printf("units=%-8s\n", pbuffer);
	else
	    printf("units not returned\n");
	pbuffer += dbr_units_size;
    }
    if (reqOptions & DBR_PRECISION) {
	precision = *((long *) pbuffer);
	if (retOptions & DBR_PRECISION)
	    printf("precision=%ld\n", precision);
	else
	    printf("precision not returned\n");
	pbuffer += dbr_precision_size;
    }
    if (reqOptions & DBR_TIME) {
	epoch_seconds = *((unsigned long *) pbuffer);
	nano_seconds = *((unsigned long *) (pbuffer + 4));
	if (retOptions & DBR_TIME)
	    printf("time=%lu %lu\n", epoch_seconds, nano_seconds);
	else
	    printf("time not returned\n");
	pbuffer += dbr_time_size;
    }
    if (reqOptions & DBR_ENUM_STRS) {
	if (retOptions & DBR_ENUM_STRS) {
	    no_strs = *((unsigned long *) pbuffer);
	    printf("no_strs=%lu\n", no_strs);
	    for (i = 0; i < *((unsigned long *) pbuffer); i++)
		printf("%s\n", (pbuffer + 8 + i * 26));
	} else
	    printf("enum strings not returned\n");
	pbuffer += dbr_enumStrs_size;
    }
    if (reqOptions & DBR_GR_LONG) {
	if (retOptions & DBR_GR_LONG)
	    printf("grLong: %ld %ld\n",
		   *(long *) (pbuffer),
		   *(long *) (pbuffer + 4));
	else
	    printf("DBRgrLong not returned\n");
	pbuffer += dbr_grLong_size;
    }
    if (reqOptions & DBR_GR_DOUBLE) {
	if (retOptions & DBR_GR_DOUBLE)
	    printf("grDouble: %lg %lg\n",
		   *(double *) (pbuffer),
		   *(double *) (pbuffer + 8));
	else
	    printf("DBRgrDouble not returned\n");
	pbuffer += dbr_grDouble_size;
    }
    if (reqOptions & DBR_CTRL_LONG) {
	if (retOptions & DBR_CTRL_LONG)
	    printf("ctrlLong: %ld %ld\n",
		   *(long *) (pbuffer),
		   *(long *) (pbuffer + 4));
	else
	    printf("DBRctrlLong not returned\n");
	pbuffer += dbr_ctrlLong_size;
    }
    if (reqOptions & DBR_CTRL_DOUBLE) {
	if (retOptions & DBR_CTRL_DOUBLE)
	    printf("ctrlDouble: %lg %lg\n",
		   *(double *) (pbuffer),
		   *(double *) (pbuffer + 8));
	else
	    printf("DBRctrlDouble not returned\n");
	pbuffer += dbr_ctrlDouble_size;
    }
    if (reqOptions & DBR_AL_LONG) {
	if (retOptions & DBR_AL_LONG)
	    printf("alLong: %ld %ld %ld %ld\n",
		   *(long *) (pbuffer),
		   *(long *) (pbuffer + 4),
		   *(long *) (pbuffer + 8),
		   *(long *) (pbuffer + 12));
	else
	    printf("DBRalLong not returned\n");
	pbuffer += dbr_alLong_size;
    }
    if (reqOptions & DBR_AL_DOUBLE) {
	if (retOptions & DBR_AL_DOUBLE)
	    printf("alDouble: %lg %lg %lg %lg\n",
		   *(double *) (pbuffer),
		   *(double *) (pbuffer + 8),
		   *(double *) (pbuffer + 16),
		   *(double *) (pbuffer + 24));
	else
	    printf("DBRalDouble not returned\n");
	pbuffer += dbr_alDouble_size;
    }
    /* Now print values */
    if (no_elements == 0) return;
    switch (dbr_type) {
    case (DBR_STRING):
	if (status != 0) {
	    sprintf(pmsg, "DBR_STRING: failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	sprintf(pmsg, "DBR_STRING:");
	dbpr_msgOut(pMsgBuff, tab_size);
	for(i=0; i<no_elements; i++) {
	    len = strlen(pbuffer);
	    if (len > 0) {
		sprintf(pmsg, " %s", pbuffer);
		dbpr_msgOut(pMsgBuff, tab_size);
	    }
	    pbuffer += MAX_STRING_SIZE;
	}
	break;
    case (DBR_CHAR):
	sprintf(pmsg, "DBR_CHAR:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    svalue = *(char *) pbuffer;
	    sprintf(pmsg, "%-9d 0x%-9x", svalue,svalue);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 1;
	}
	break;
    case (DBR_UCHAR):
	sprintf(pmsg, "DBR_UCHAR:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    usvalue = *(unsigned char *) pbuffer;
	    sprintf(pmsg, "%-9d 0x%-9x", usvalue,usvalue);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 1;
	}
	break;
    case (DBR_SHORT):
	sprintf(pmsg, "DBR_SHORT:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    sprintf(pmsg, "%-9d 0x%-9x", *((short *) pbuffer), *((short *) pbuffer));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 2;
	}
	break;
    case (DBR_USHORT):
	sprintf(pmsg, "DBR_USHORT:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    sprintf(pmsg, "%-9u 0x%-9x",*((unsigned short *) pbuffer),*((unsigned short *) pbuffer));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 2;
	}
	break;
    case (DBR_LONG):
	sprintf(pmsg, "DBR_LONG:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    sprintf(pmsg, "%-9ld 0x%-9lx", *((long *) pbuffer), *((long *) pbuffer));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 4;
	}
	break;
    case (DBR_ULONG):
	sprintf(pmsg, "DBR_ULONG:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    sprintf(pmsg, "%-9ld 0x%-9lx",*((unsigned long *) pbuffer),*((unsigned long *) pbuffer));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 4;
	}
	break;
    case (DBR_FLOAT):
	sprintf(pmsg, "DBR_FLOAT:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    sprintf(pmsg, "%-13.6g", *((float *) pbuffer));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 4;
	}
	break;
    case (DBR_DOUBLE):
	sprintf(pmsg, "DBR_DOUBLE:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    sprintf(pmsg, "%-13.6g", *((double *) pbuffer));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 8;
	}
	break;
    case (DBR_ENUM):
	sprintf(pmsg, "DBR_ENUM:  ");
	dbpr_msgOut(pMsgBuff, tab_size);
	if (status != 0) {
	    sprintf(pmsg, " failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	for (i = 0; i < no_elements; i++) {
	    sprintf(pmsg, "%-9u", *((unsigned short *) pbuffer));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    pbuffer += 2;
	}
	break;
    default:
	printf(" illegal request type.");
	break;
    }
    dbpr_msg_flush(pMsgBuff, tab_size);
    return;
}

static int dbpr_report(pname, paddr, interest_level, pMsgBuff, tab_size)
    char           *pname;
    struct dbAddr  *paddr;	/* requested PV name */
    int             interest_level;
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{

    char           *pmsg;
    char           *pName;
    char           *pfname;
    struct fldDes  *pfldDes;
    struct recTypDes *precTypDes;
    struct dbAddr  *pLaddr;
    struct dbAddr   Laddr;	/* local address */
    short           fldNum;	/* field number		 */
    char           *pRecordName;
    char            RecordName[PVNAME_SZ + 2];	/* pv record name */
    char           *pfield_name;
    char            field_name[FLDNAME_SZ + 1];	/* pv field name */
    char           *pPvName=NULL;
    char            PvName[PVNAME_SZ + FLDNAME_SZ + 4];	/* recordname.<pvname> */
    short           n,n2;
    long            status;
    long            buffer[100];
    void       	   *pbuffer;
    long            options;
    long            nRequest;
    int             sv_tab_size;
    pmsg = pMsgBuff->message;

    /* extract record name */
    pbuffer = (char *) buffer;
    pRecordName = &RecordName[0];
    pName = pname;
    n = 0;
    while (*pName && (*pName != '.') && (n < PVNAME_SZ)) {
	*pRecordName = *pName;
	pName++;
	pRecordName++;
	n++;
    }
    *pRecordName = 0;
    /* get ptr to record type descriptor */
    if (!(precTypDes = (struct recTypDes *)
	  dbprGetRecTypDes(paddr->record_type)))
	return (-1);

    for (n2 = 0; n2 <= precTypDes->no_fields - 1; n2++) {
        fldNum = precTypDes->sortFldInd[n2];

	/* get ptr to field descriptor */
	if (!(pfldDes = (struct fldDes *)
	      dbprGetFldRec(paddr->record_type, fldNum))) {
	    return (-1);
	}
	/* extract field name */
	pfield_name = field_name;
	/* blank pad the field name */
	pfname = pfldDes->fldname;
	n = 0;
	while (*(pfname) && (n < FLDNAME_SZ)) {
	    *pfield_name = *(pfname);
	    pfield_name++;
	    pfname++;
	    n++;
	}
	while (n < FLDNAME_SZ) {
	    *pfield_name = ' ';
	    pfield_name++;
	    n++;
	}
	*pfield_name = 0;

	pPvName = PvName;
	pRecordName = RecordName;
	pfield_name = field_name;

	/* use complete PV name */
	strcpy((char *) pPvName, (char *) pRecordName);
	strcat(pPvName, ".");
	strcat(pPvName, pfield_name);

	status = dbNameToAddr(pPvName, &Laddr);
	if(status) {
	    printf("dbNameToAddr failed\n");
	    return(1);
	}
	if (pfldDes->interest > interest_level )
	    continue;
	pLaddr = &Laddr;
	switch (pfldDes->field_type) {
	case DBF_STRING:
	    sprintf(pmsg, "%s: %s", pfield_name, pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_USHORT:
	    sprintf(pmsg, "%s: 0x%-8X", pfield_name,
		    *(unsigned short *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_ENUM:
	    sprintf(pmsg, "%s: %d", pfield_name,
		    *(unsigned short *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_FLOAT:
	    sprintf(pmsg, "%s: %-12.4G", pfield_name,
		    *(float *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_CHAR:
	    sprintf(pmsg, "%s: %d", pfield_name, *(char *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_UCHAR:
	    sprintf(pmsg, "%s: %d", pfield_name,
		    *(unsigned char *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_NOACCESS:
	    { /* lets just print field in hex */
		char * pchar = (char *)(pLaddr->pfield);
		char   temp_buf[42];
		char *ptemp_buf = &temp_buf[0];
		short n = pLaddr->field_size;
		short i;
		unsigned long value;

		if(n>(sizeof(temp_buf)-2)/2) n = (sizeof(temp_buf)-2)/2;
		for (i=0; i<n; i++, (ptemp_buf+=2), pchar++) {
			value = *((unsigned char *)pchar);
			sprintf(ptemp_buf,"%02x",value);
		}
		*ptemp_buf = 0;
		sprintf(pmsg, "%s: %s", pfield_name,temp_buf);
		dbpr_msgOut(pMsgBuff, tab_size);
	    }
	    break;
	case DBF_SHORT:
	    sprintf(pmsg, "%s: %d", pfield_name, *(short *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_LONG:
	    sprintf(pmsg, "%s: 0x%-8X", pfield_name, *(long *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_ULONG:
	    sprintf(pmsg, "%s: 0x%-8X", pfield_name,
		    *(unsigned long *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_DOUBLE:
	    sprintf(pmsg, "%s: %-12.4G", pfield_name,
		    *(double *) pLaddr->pfield);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_INLINK:
	    dbprReportLink(pMsgBuff, pfield_name,
	     (struct link *) pLaddr->pfield, pfldDes->field_type, tab_size);
	    break;
	case DBF_OUTLINK:
	    dbprReportLink(pMsgBuff, pfield_name,
	     (struct link *) pLaddr->pfield, pfldDes->field_type, tab_size);
	    break;
	case DBF_FWDLINK:
	    dbprReportLink(pMsgBuff, pfield_name,
	     (struct link *) pLaddr->pfield, pfldDes->field_type, tab_size);
	    break;
	case DBF_CVTCHOICE:
	    dbprReportCvtChoice(pMsgBuff, pfield_name,
			      *(unsigned short *) pLaddr->pfield, tab_size);
	    break;
	case DBF_RECCHOICE:
	    dbprReportRecChoice(pMsgBuff, pLaddr->precord, pfield_name,
			      *(unsigned short *) pLaddr->pfield, tab_size);
	    break;
	case DBF_DEVCHOICE:
	    dbprReportDevChoice(pMsgBuff, pLaddr, pfield_name, tab_size);
	    break;
	case DBF_GBLCHOICE:
	    if (dbprReportGblChoice(pMsgBuff, pLaddr->precord, pfield_name,
				    *(short *) pLaddr->pfield, tab_size))
		return (-1);
	    break;
	default:
	    sprintf(pmsg, "%s: dbpr: Unknown field_type", pfield_name);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
    }
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    /* flush buffer and change tab size */
    sv_tab_size = tab_size;
    tab_size = 10;
    strcpy((char *) pPvName, (char *) pRecordName);
    strcat(pPvName, ".VAL");
    status = dbNameToAddr(pPvName, &Laddr);
    if (status)
	return (1);
    pLaddr = &Laddr;
    pfldDes = (struct fldDes *) Laddr.pfldDes;
    if (pLaddr->field_type != pfldDes->field_type
	    || pLaddr->no_elements > 1) {
	options = 0;
	nRequest = MIN(pLaddr->no_elements,((sizeof(buffer))/pLaddr->field_size));
	status = dbGetField(&Laddr, Laddr.dbr_field_type, pbuffer,
			    &options, &nRequest,NULL);
	printBuffer(status, Laddr.dbr_field_type, pbuffer,
		    0L, 0L, nRequest, pMsgBuff, tab_size);
	tab_size = sv_tab_size;
	/* flush buffer and restore tab size */
	pmsg[0] = '\0';
	dbpr_msgOut(pMsgBuff, tab_size);
    }
    return (0);
}

static void dbpr_msgOut(pMsgBuff, tab_size)
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{
    int             len;
    int             err = 0;
    char           *pmsg = pMsgBuff->message;
    static int      last_tabsize;

    if (!((tab_size == 10) || (tab_size == 20))) {
	printf("tab_size not 10 or 20 - dbpr_msgOut()\n");
	return;
    }
    /* init if first time */
    if (!(pMsgBuff->pNext))
	dbpr_init_msg(pMsgBuff, tab_size);
    if (tab_size != last_tabsize)
	pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;

    last_tabsize = tab_size;
    /* flush output if NULL string command */
    if (*pmsg == 0) {
	dbpr_msg_flush(pMsgBuff, tab_size);
	return;
    }
    /* truncate if too long */
    if ((len = strlen(pmsg)) > MAXLINE)
	err = 1;
    len = ((len > MAXLINE) ? MAXLINE : len);

    dbpr_insert_msg(pMsgBuff, len, tab_size);

    /* warn if msg gt 80 */
    if (err == 1) {
	len = strlen(pmsg);
	sprintf(pmsg,
		"dbpr_msgOut: ERROR - msg length=%d limit=%d ",
		len,
		MAXLINE);
	dbpr_insert_msg(pMsgBuff, len, tab_size);
	return;
    }
    return;
}

static void dbpr_init_msg(pMsgBuff, tab_size)
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pLast = pMsgBuff->out_buff + MAXLINE;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
    return;
}

static void dbpr_insert_msg(pMsgBuff, len, tab_size)
    TAB_BUFFER     *pMsgBuff;
    int             len;
    int             tab_size;
{
    int             current_len;
    int             n;
    int             tot_line;
    char           *pmsg = pMsgBuff->message;
    current_len = strlen(pMsgBuff->out_buff);
    tot_line = current_len + len;

    /* flush buffer if overflow would occor */
    if (tot_line > MAXLINE)
	dbpr_msg_flush(pMsgBuff, tab_size);

    /* append message to buffer */
    n = 0;
    while ((*pmsg) && (n < len)) {
	*pMsgBuff->pNext++ = *pmsg++;

	/* position to next tab stop */
	if (*(pMsgBuff->pNexTab - 1) != '\0')
	    pMsgBuff->pNexTab = pMsgBuff->pNexTab + tab_size;
	n++;
    }

    /* fill spaces to next tab stop */
    while (*(pMsgBuff->pNexTab - 1) != ' '
	    && pMsgBuff->pNext < pMsgBuff->pLast) {
	*pMsgBuff->pNext++ = ' ';
    }
    return;

}

static void dbpr_msg_flush(pMsgBuff, tab_size)
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{
    /* skip print if buffer empty */
    if (pMsgBuff->pNext != pMsgBuff->out_buff)
	printf("%s\n", pMsgBuff->out_buff);
    memset(pMsgBuff->out_buff,'\0', (int) MAXLINE + 1);
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
    return;
}

static void dbprReportLink(pMsgBuff,pfield_name,plink,field_type, tab_size)
    TAB_BUFFER     *pMsgBuff;
    char           *pfield_name;
    struct link    *plink;
    short           field_type;
    int             tab_size;
{
    char *pmsg = pMsgBuff->message;
    switch(plink->type) {
    case CONSTANT:
	sprintf(pmsg,"%4s: %-12.4G", pfield_name, plink->value.value);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case PV_LINK:
	if(field_type != DBF_FWDLINK) {
	sprintf(pmsg,"%4s: PV_LINK pp=%1d ms=%1d %s.%.4s",
	    pfield_name,
	    plink->value.pv_link.process_passive,
	    plink->value.pv_link.maximize_sevr,
	    plink->value.pv_link.pvname,
	    plink->value.pv_link.fldname);
	} else {
	sprintf(pmsg,"%4s: PV_LINK %s.%.4s",
	    pfield_name,
	    plink->value.pv_link.pvname,
	    plink->value.pv_link.fldname);
	}
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case VME_IO:
	sprintf(pmsg,"%4s: VME card=%2d signal=%2d",
	    pfield_name,
	    plink->value.vmeio.card,plink->value.vmeio.signal);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case CAMAC_IO:
	sprintf(pmsg,"%4s: CAMAC branch=%2d crate=%2d slot=%2d channel=%2d",
	    pfield_name,
	    plink->value.camacio.branch,plink->value.camacio.crate,
	    plink->value.camacio.slot,plink->value.camacio.channel);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case AB_IO:
	sprintf(pmsg,"%4s: ABIO link=%2d adaptor=%2d card=%2d signal=%2d flag=%1d",
	    pfield_name,
	    plink->value.abio.link,plink->value.abio.adapter,
	    plink->value.abio.card,plink->value.abio.signal,
	    plink->value.abio.plc_flag);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case GPIB_IO:
	sprintf(pmsg,"%4s: GPIB link=%2d addr=%2d parm=%s",
	    pfield_name,
	    plink->value.gpibio.link, plink->value.gpibio.addr,
	    &plink->value.gpibio.parm[0]);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case BITBUS_IO:
	sprintf(pmsg,"%4s: BITBUS link=%2d node=%2d port=%2d signal=%2d parm=%s",
	    pfield_name,
	    plink->value.bitbusio.link,plink->value.bitbusio.node,
	    plink->value.bitbusio.port,plink->value.bitbusio.signal,
	    &plink->value.bitbusio.parm[0]);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case BBGPIB_IO:
	sprintf(pmsg,"%4s: BBGPIBIO link=%2d bbaddr=%2d gpibio=%2d parm=%s",
	    pfield_name,
	    plink->value.bbgpibio.link,plink->value.bbgpibio.bbaddr,
	    plink->value.bbgpibio.gpibaddr,
	    &plink->value.bbgpibio.parm[0]);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case INST_IO:
	sprintf(pmsg,"%4s: INSTIO  parm=%s",
	    pfield_name,
	    &plink->value.instio.string[0]);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case DB_LINK:
	if(field_type != DBF_FWDLINK) {
	sprintf(pmsg,"%4s: DB_LINK pp=%1d ms=%1d %.32s",
	    pfield_name,
	    plink->value.db_link.process_passive,
	    plink->value.db_link.maximize_sevr,
	    ((struct dbCommon *)(
		((struct dbAddr *)plink->value.db_link.pdbAddr)
		->precord))->name);
	}else{
	sprintf(pmsg,"%4s: DB_LINK %.32s",
	    pfield_name,
	    ((struct dbCommon *)(
		((struct dbAddr *)plink->value.db_link.pdbAddr)
		->precord))->name);
	}
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
   case CA_LINK:
	sprintf(pmsg,"%4s: CA_LINK",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    default:
	sprintf(pmsg,"%4s: dbprReportLink: Illegal link.type",
	    pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    }
    return;
}

static void dbprReportCvtChoice(pMsgBuff,pfield_name,choice_value,tab_size)
    TAB_BUFFER     *pMsgBuff;
    char           *pfield_name;
    unsigned short  choice_value;
    int             tab_size;
{
    char *pchoice;
    char *pmsg = pMsgBuff->message;

    if(!(pchoice=GET_CHOICE(pdbBase->pchoiceCvt,choice_value))) {
	sprintf(pmsg,"%4s Illegal Choice",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
    }
    else {
	sprintf(pmsg,"%4s: %s",pfield_name,pchoice);
	dbpr_msgOut(pMsgBuff,tab_size);
    }
    return;
}

static int dbprReportGblChoice(pMsgBuff,precord,pfield_name,choice_value, tab_size)
    TAB_BUFFER     *pMsgBuff;
    struct dbCommon *precord;
    char	    *pfield_name;
    unsigned short  choice_value;
    int             tab_size;
{
    char	     name[PVNAME_SZ+1+FLDNAME_SZ+1];
    struct dbAddr    dbAddr;
    struct choiceSet *pchoiceSet;
    char	     *pchoice;
    char *pmsg = pMsgBuff->message;

    strncpy(name,precord->name,PVNAME_SZ);
    name[PVNAME_SZ]=0;
    strcat(name,".");
    strncat(name,pfield_name,FLDNAME_SZ);
    if(dbNameToAddr(name,&dbAddr)) {
	sprintf(pmsg,"%4s dbNameToAddr failed?",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
	return(0);
    }
    if( !(pchoiceSet=GET_PCHOICE_SET(pdbBase->pchoiceGbl,dbAddr.choice_set))
    ||  !(pchoice=GET_CHOICE(pchoiceSet,choice_value))) {
	sprintf(pmsg,"%4s Cant find Choice",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
    }
    else {
	sprintf(pmsg,"%4s: %s",pfield_name,pchoice);
	dbpr_msgOut(pMsgBuff,tab_size);
    }
    return(0);
}

static void dbprReportRecChoice(pMsgBuff,precord,pfield_name,choice_value,tab_size)
    TAB_BUFFER     *pMsgBuff;
    struct dbCommon *precord;
    char	    *pfield_name;
    unsigned short  choice_value;
    int             tab_size;
{
    char	        name[PVNAME_SZ+1+FLDNAME_SZ+1];
    struct dbAddr       dbAddr;
    struct arrChoiceSet *parrChoiceSet;
    struct choiceSet    *pchoiceSet;
    char	        *pchoice;
    char *pmsg = pMsgBuff->message;

    strncpy(name,precord->name,PVNAME_SZ);
    name[PVNAME_SZ]=0;
    strcat(name,".");
    strncat(name,pfield_name,FLDNAME_SZ);
    if(dbNameToAddr(name,&dbAddr)) {
	sprintf(pmsg,"%4s: dbNameToAddr failed?",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
	return;
    }
    if( !(parrChoiceSet=GET_PARR_CHOICE_SET(pdbBase->pchoiceRec,dbAddr.record_type)) 
    ||  !(pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,dbAddr.choice_set))
    ||  !(pchoice=GET_CHOICE(pchoiceSet,choice_value))) {
	sprintf(pmsg,"%4s: Cant find Choice",pfield_name);
    }
    else {
	sprintf(pmsg,"%4s: %s",pfield_name,pchoice);
    }
	dbpr_msgOut(pMsgBuff,tab_size);
    return;
}

static void dbprReportDevChoice(pMsgBuff,paddr,pfield_name, tab_size)
    TAB_BUFFER     *pMsgBuff;
    struct dbAddr  *paddr;
    char           *pfield_name;
    int             tab_size;
{
    short      		choice_ind= *((short*)paddr->pfield);
    struct devChoiceSet *pdevChoiceSet;
    struct devChoice	*pdevChoice;
    long		no_elements= paddr->no_elements;
    char                *pmsg = pMsgBuff->message;
    char buff[MAX_STRING_SIZE+1];

    if(no_elements!=1){
	sprintf(pmsg,
           "%4s: Only one element allowed - dbprReportDevChoice",
            pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
        return;
    }
    if((!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbBase->pchoiceDev,paddr->record_type)))
    || (!(pdevChoice=GET_DEV_CHOICE(pdevChoiceSet,choice_ind))) ) {
	sprintf(pmsg, "%4s: NULL",
            pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
        return;
    }
    strncpy(buff,pdevChoice->pchoice,MAX_STRING_SIZE);
    buff[MAX_STRING_SIZE] = 0;
    sprintf(pmsg,"%4s: %s",
            pfield_name,
            buff);
	dbpr_msgOut(pMsgBuff,tab_size);
    return;
}

static struct fldDes * dbprGetFldRec(type, fldNum)
    short           type;	/* record type		 */
    short           fldNum;	/* field number		 */
{
    struct fldDes  *pfield;
    struct recDes  *precDes;
    struct recTypDes *precTypDes;

    if (!(precDes=pdbBase->precDes)) return (NULL);
    if ((type < 0) || (type >= precDes->number)) return (NULL);
    /* get and check pointer to this record's field definitions */
    precTypDes = precDes->papRecTypDes[type];
    if (!precTypDes)
	return (NULL);
    pfield = precTypDes->papFldDes[fldNum];
    return (pfield);
}


static struct recTypDes * dbprGetRecTypDes(type)
    short           type;	/* record type		 */
{
    struct recDes  *precDes;
    struct recTypDes *precTypDes;

    if (!(precDes=pdbBase->precDes)) return (NULL);
    if ((type < 0) || (type >= precDes->number)) return (NULL);
    /* get and check pointer to this record's field definitions */
    precTypDes = precDes->papRecTypDes[type];
    if (!precTypDes)
	return (NULL);
    return (precTypDes);
}
