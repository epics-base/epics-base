/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbTest.c */
/* base/src/db  $Id$ */
/*	database access test subroutines */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "epicsStdio.h"
#include "dbDefs.h"
#include "errlog.h"
#include "ellLib.h"
#include "epicsMutex.h"
#include "ellLib.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "link.h"
#include "dbFldTypes.h"
#include "recSup.h"
#include "devSup.h"
#include "drvSup.h"
#include "dbCommon.h"
#include "special.h"
#include "db_field_log.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbLock.h"
#include "dbAccessDefs.h"
#include "recGbl.h"
#include "dbEvent.h"
#include "callback.h"
#include "dbTest.h"

#define MAXLINE 80
struct msgBuff {		/* line output structure */
    char            out_buff[MAXLINE + 1];
    char           *pNext;
    char           *pLast;
    char           *pNexTab;
    char            message[128];
};
typedef struct msgBuff TAB_BUFFER;

#ifndef MIN
#   define MIN(x,y)  (((x) < (y)) ? (x) : (y))
#endif
#ifndef MAX
#   define MAX(x,y)  (((x) < (y)) ? (x) : (y))
#endif

/* Local Routines */
static void printDbAddr(long status,DBADDR *paddr);
static void printBuffer(
	long status,short dbr_type,void *pbuffer,long reqOptions,
	long retOptions,long no_elements,TAB_BUFFER *pMsgBuff,int tab_size);
static int dbpr_report(
	const char *pname,DBADDR *paddr,int interest_level,
	TAB_BUFFER *pMsgBuff,int tab_size);
static void dbpr_msgOut(TAB_BUFFER *pMsgBuff,int tab_size);
static void dbpr_init_msg(TAB_BUFFER *pMsgBuff,int tab_size);
static void dbpr_insert_msg(TAB_BUFFER *pMsgBuff,int len,int tab_size);
static void dbpr_msg_flush(TAB_BUFFER *pMsgBuff,int tab_size);

long epicsShareAPI dba(const char*pname)
{
    DBADDR 	addr;
    long		status;

    if (pname==0 || *pname==0) {
    	printf("\nusage \"pv name\"\n");
    	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
        printf("dbNameToAddr failed\n");
        return 0;
    }
    printDbAddr(status,&addr);
    return 0;
}

long epicsShareAPI dbl(
    const char *precordTypename,const char *fields)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    int		nfields = 0;
    int		ifield;
    char	*fieldnames = 0;
    char 	**papfields = 0;

    if (fields && (*fields == '\0'))
        fields = NULL;
    if(fields) {
	char *pnext;

	fieldnames = calloc(strlen(fields) + 1,sizeof(char));
	strcpy(fieldnames,fields);
	nfields=1;
	pnext=fieldnames;
	while(*pnext && (pnext = strchr(pnext,' '))) {
	    nfields++;
	    while(*pnext == ' ') pnext++;
	}
	papfields = dbCalloc(nfields,sizeof(char *));
	pnext = fieldnames;
	for(ifield=0; ifield<nfields; ifield++) {
	    papfields[ifield] = pnext;
	    if(ifield<nfields-1) {
		pnext = strchr(pnext,' ');
		*pnext++ = 0;
		while(*pnext == ' ') pnext++;
	    }
	}
    }
    if(!pdbbase) {
        fprintf(stderr,"no database has been loaded\n");
        return(0);
    }
    dbInitEntry(pdbbase,pdbentry);
    if(!precordTypename)
	status = dbFirstRecordType(pdbentry);
    else
	status = dbFindRecordType(pdbentry,precordTypename);
    if(status) {
	fprintf(stdout,"No record description\n");
    }
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    fprintf(stdout,"%s",dbGetRecordName(pdbentry));
	    for(ifield=0; ifield<nfields; ifield++) {
		char *pvalue;
		status = dbFindField(pdbentry,papfields[ifield]);
		if(status) continue;
		pvalue = dbGetString(pdbentry);
		if(pvalue) {
		    fprintf(stdout,",\"%s\"",pvalue);
		}
	    }
	    fprintf(stdout,"\n");
            status = dbNextRecord(pdbentry);
	}
	if(precordTypename) break;
	status = dbNextRecordType(pdbentry);
    }
    if(nfields>0) {
	free((void *)papfields);
	free((void *)fieldnames);
    }
    dbFinishEntry(pdbentry);
    return(0);
}

long epicsShareAPI dbnr(int verbose)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    int		nrecords;
    int		total=0;

    if(!pdbbase) {
        fprintf(stderr,"no database has been loaded\n");
        return(0);
    }
    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecordType(pdbentry);
    if(status) printf("No record description\n");
    while(!status) {
	nrecords = dbGetNRecords(pdbentry);
	total += nrecords;
	if(verbose || nrecords)
	    printf("%.4d %s\n",nrecords,dbGetRecordTypeName(pdbentry));
	status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    printf("Total Records: %d\n",total);
    return(0);
}

static int specified_by(const char *ptest, const char *pspec)
{
    short               inx;
    short               wild_card_start;
 
        /* check if the specification begins with a wild card */
        if (*pspec == '*') wild_card_start = TRUE;
        else wild_card_start = FALSE;
 
        /* check for specification */
        while (TRUE) {
                /* skip any wild cards */
                while (*pspec == '*') pspec++;
 
                /* find the specification chars to compare */
                inx = 0;
                while ( (*(pspec+inx) != '*') && (*(pspec+inx)) )
                        inx++;
 
                /* check for specification ending with wildcard */
                if (inx == 0) return(TRUE);
 
                /* find the spec chars in the test string */
                while ((strlen(ptest) >= inx)
                  && (strncmp(ptest,pspec,inx) != 0) ) {
 
                        /* check variable beginning */
                        if (!wild_card_start) return(FALSE);
                        else ptest++;
                }
 
                /* check segment found */
                if (strlen(ptest) < inx) return(FALSE);
 
                /* adjust pointers and wild card indication */
                wild_card_start = TRUE;
                ptest += inx;
                pspec += inx;
 
                /* check for end of specification */
                if (*pspec == 0) {
                        if (*ptest == 0) return(TRUE);
                        else return(FALSE);
                }
        }
}
 
long epicsShareAPI dbgrep(const char *pmask)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    char	*pname;

    if (pmask==0 || *pmask==0 ) {
    	printf("\nusage \"reg\"\n");
    	return(1);
    }
    if(!pdbbase) {
        fprintf(stderr,"no database has been loaded\n");
        return(0);
    }
    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecordType(pdbentry);
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    pname = dbGetRecordName(pdbentry);
            if (specified_by(pname, pmask))  printf("%s\n", pname);
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return(0);
}

long epicsShareAPI dbgf(const char *pname)
{
    /* declare buffer long just to ensure correct alignment */
    long 		buffer[100];
    long 		*pbuffer=&buffer[0];
    DBADDR 	addr;
    long		status;
    long		options,no_elements;
    static TAB_BUFFER 	msg_Buff;
    TAB_BUFFER     	*pMsgBuff = &msg_Buff;
    char 		*pmsg = pMsgBuff->message;
    int             	tab_size;
    tab_size = 10;

    if (pname==0 || *pname==0) {
    	printf("\nusage \"pv name\"\n");
    	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
	errMessage(status," dbNameToAddr failed");
	return(status);
    }
    no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
    options=0;
    if(addr.dbr_field_type==DBR_ENUM) {
	status=dbGetField(&addr,DBR_STRING,pbuffer,&options,&no_elements,NULL);
	printBuffer(status,DBR_STRING,pbuffer,0L,0L,no_elements,
	   pMsgBuff,tab_size);
    }
    else {
	status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
	    &options,&no_elements,NULL);
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,
	    no_elements,pMsgBuff,tab_size);
    }
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

long epicsShareAPI dbpf(const char *pname,const char *pvalue)
{
    /* declare buffer long just to ensure correct alignment */
    DBADDR addr;
    long	  status;

    if (pname==0 || *pname==0) {
    	printf("\nusage \"pv name\" \"value\"\n");
    	return(1);
    }
    /* make sure value was passed*/
    if(pvalue==0) {
	errMessage(0L,"No value was specified");
	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
         errMessage(status,"dbNameToAddr error");
         return(status);
    }
    /* For enumerated types must allow for ENUM rather than string*/
    /* If entire field is digits then use DBR_ENUM else DBR_STRING*/
    if((addr.dbr_field_type==DBR_ENUM) && (*pvalue!=0)
    &&  (strspn(pvalue,"0123456789")==strlen(pvalue))) {
	    unsigned short value;

	    sscanf(pvalue,"%hu",&value);
	    status=dbPutField(&addr,DBR_ENUM,&value,1L);
    } else {
	status=dbPutField(&addr,DBR_STRING,pvalue,1L);
    }
    if(status) {
         errMessage(status,"dbPutField error");
         return(status);
    }
    status=dbgf(pname);
    return(status);
}

long epicsShareAPI dbpr(const char *pname,int interest_level)
{
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER       *pMsgBuff = &msg_Buff;
    DBADDR    addr;
    long             status;
    char            *pmsg;
    int              tab_size;

    if (pname==0 || *pname==0) {
    	printf("\nusage \"pv name\"\n");
    	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
        printf("dbNameToAddr failed\n");
        return 0;
    }
    pmsg = pMsgBuff->message;
    tab_size = 20;

    status = dbNameToAddr(pname, &addr);
    if(status) {
	errMessage(status," dbNameToAddr failed");
	return(status);
    }
    if (dbpr_report(pname, &addr, interest_level, pMsgBuff, tab_size))
	return (1);
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return (0);
}

long epicsShareAPI dbtr(const char *pname)
{
    DBADDR addr;
    long 	     status;
    struct dbCommon  *precord;

    if (pname==0 || *pname==0) {
    	printf("\nusage \"pv name\"\n");
    	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
        printf("dbNameToAddr failed\n");
        return 0;
    }
    precord=(struct dbCommon*)(addr.precord);
    if (precord->pact) {
        printf("record active\n");
        return(1);
    }
    if(epicsMutexTryLock(precord->mlok)!=epicsMutexLockOK) {
        printf("record locked\n");
        return(1);
    }
    status=dbProcess(precord);
    epicsMutexUnlock(precord->mlok);
    if(status)
        recGblRecordError(status,precord,"dbtr(dbProcess)");
    dbpr(pname,3);
    return(0);
}

long epicsShareAPI dbtgf(const char *pname)
{
    /* declare buffer long just to ensure correct alignment */
    long              buffer[400];
    long              *pbuffer=&buffer[0];
    DBADDR     addr;
    long	      status;
    long	      req_options,ret_options,no_elements;
    short	      dbr_type;
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER        *pMsgBuff = &msg_Buff;
    char              *pmsg = pMsgBuff->message;
    int               tab_size;

    tab_size = 10;

    if (pname==0 || *pname==0) {
    	printf("\nusage \"pv name\"\n");
    	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
        printf("dbNameToAddr failed\n");
        return 0;
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

long epicsShareAPI dbtpf(const char *pname,const char *pvalue)
{
    /* declare buffer long just to ensure correct alignment */
    long          buffer[100];
    long          *pbuffer=&buffer[0];
    DBADDR addr;
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
    int		validNumber;

    tab_size = 10;

    if (pname==0 || *pname==0) {
    	printf("\nusage \"pv name\"\n");
    	return(1);
    }
    status=dbNameToAddr(pname,&addr);
    if(status) {
        printf("dbNameToAddr failed\n");
        return 0;
    }
    /* DBR_STRING */
    status=dbPutField(&addr,DBR_STRING,pvalue,1L);
    if(status!=0) errMessage(status,"DBR_STRING Failed");
    else {
	printf("DBR_STRING ok\n");
	no_elements=MIN(addr.no_elements,((sizeof(buffer))/addr.field_size));
	options=0;
	status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements,NULL);
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    }
    /* DBR_CHAR */
    validNumber=(strspn(pvalue,"0123456789+-eE. ")==strlen(pvalue))?TRUE:FALSE;
    if(validNumber && sscanf(pvalue,"%hd",&svalue)==1) {
	cvalue = (char)svalue;
	status=dbPutField(&addr,DBR_CHAR,&cvalue,1L);
	if(status!=0) errMessage(status,"DBR_UCHAR failed");
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
    if(validNumber && sscanf(pvalue,"%hu",&usvalue)==1) {
	ucvalue = (unsigned char)usvalue;
	status=dbPutField(&addr,DBR_UCHAR,&ucvalue,1L);
	if(status!=0) errMessage(status,"DBR_UCHAR failed");
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
    if(validNumber && sscanf(pvalue,"%hd",&svalue)==1) {
	status=dbPutField(&addr,DBR_SHORT,&svalue,1L);
	if(status!=0) errMessage(status,"DBR_SHORT failed");
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
    if(validNumber && sscanf(pvalue,"%hu",&usvalue)==1) {
	status=dbPutField(&addr,DBR_USHORT,&usvalue,1L);
	if(status!=0) errMessage(status,"DBR_USHORT failed");
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
    if(validNumber && sscanf(pvalue,"%ld",&lvalue)==1) {
	status=dbPutField(&addr,DBR_LONG,&lvalue,1L);
	if(status!=0) errMessage(status,"DBR_LONG failed");
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
    if(validNumber && sscanf(pvalue,"%lu",&ulvalue)==1) {
	status=dbPutField(&addr,DBR_ULONG,&ulvalue,1L);
	if(status!=0) errMessage(status,"DBR_ULONG failed");
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
    if(validNumber && sscanf(pvalue,"%e",&fvalue)==1) {
	status=dbPutField(&addr,DBR_FLOAT,&fvalue,1L);
	if(status!=0) errMessage(status,"DBR_FLOAT failed");
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
    if(validNumber && sscanf(pvalue,"%le",&dvalue)==1) {
	status=dbPutField(&addr,DBR_DOUBLE,&dvalue,1L);
	if(status!=0) errMessage(status,"DBR_DOUBLE failed");
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
    if(validNumber && sscanf(pvalue,"%hu",&usvalue)==1) {
	status=dbPutField(&addr,DBR_ENUM,&usvalue,1L);
	if(status!=0) errMessage(status,"DBR_ENUM failed");
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

long epicsShareAPI dbior(const char *pdrvName,int interest_level)
{
    char	 *pname;
    drvSup	 *pdrvSup;
    struct drvet *pdrvet;
    dbRecordType *pdbRecordType;
    devSup	 *pdevSup;
    struct dset	 *pdset;

    if(!pdbbase) {
        fprintf(stderr,"no database has been loaded\n");
        return(0);
    }
    for(pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList); pdrvSup;
    pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
	pname = pdrvSup->name;
	if(pdrvName!=NULL && *pdrvName!='\0' && (strcmp(pdrvName,pname)!=0)) continue;
	pdrvet = pdrvSup->pdrvet ;
	if(pdrvet==NULL) {
		printf("No driver entry table is present for %s\n",pname);
		continue;
	}
	if(pdrvet->report==NULL)
	    printf("Driver: %s No report available\n",pname);
	else {
	    printf("Driver: %s\n",pname);
	    (*pdrvet->report)(interest_level);
	}
    }
    /* now check devSup reports */
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList); pdevSup;
	pdevSup = (devSup *)ellNext(&pdevSup->node)) {
	    if(!(pdset = pdevSup->pdset)) continue;
	    if(!(pname = pdevSup->name)) continue;
	    if(pdrvName!=NULL && *pdrvName!='\0' && (strcmp(pdrvName,pname)!=0)) continue;
	    if(pdset->report!=NULL) {
		printf("Device Support: %s\n",pname);
		(*pdset->report)(interest_level);
	    }
	}
    }
    return(0);
}

int epicsShareAPI dbhcr(void)
{
    if(!pdbbase) {
        fprintf(stderr,"no database has been loaded\n");
        return(0);
    }
    dbReportDeviceConfig(pdbbase,stdout);
    return(0);
}

static char *dbf[DBF_NTYPES]={
	"STRING","CHAR","UCHAR","SHORT","USHORT","LONG","ULONG",
	"FLOAT","DOUBLE","ENUM","MENU","DEVICE",
	"INLINK","OUTLINK","FWDLINK","NOACCESS"};

static char *dbr[DBR_ENUM+2]={
	"STRING","CHAR","UCHAR","SHORT","USHORT","LONG","ULONG",
	"FLOAT","DOUBLE","ENUM","NOACCESS"};

static void printDbAddr(long status,DBADDR *paddr)
{
    short	field_type;
    short	dbr_field_type;
    dbFldDes	*pdbFldDes = (dbFldDes *)paddr->pfldDes;;

    if(status!=0) {
	errMessage(status,"dbNameToAddr error");
    }
    printf("Record Address: %p",(void *)paddr->precord);
    printf(" Field Address: %p",paddr->pfield);
    printf(" Field Description: %p\n",(void *)pdbFldDes);
    printf("   No Elements: %ld\n",paddr->no_elements);
    printf("   Record Type: %s\n",pdbFldDes->pdbRecordType->name);
    printf("     FieldType: DBF_");
    field_type = paddr->field_type;
    if(field_type<0 || field_type>DBR_NOACCESS)
	printf(" Illegal = %d\n",field_type);
    else
	printf("%s\n",dbf[field_type]);
    printf("    Field Type: %d\n",paddr->field_type);
    printf("    Field Size: %d\n",paddr->field_size);
    printf("       Special: %d\n",paddr->special);
    printf("DBR Field Type: DBR_");
    dbr_field_type = paddr->dbr_field_type;
    if(dbr_field_type==DBR_NOACCESS)dbr_field_type=DBR_ENUM + 1;
    if(dbr_field_type<0 || dbr_field_type>(DBR_ENUM+1))
	printf(" Illegal = %d\n",dbr_field_type);
    else
	printf("%s\n",dbr[dbr_field_type]);
}


static void printBuffer(
	long status,short dbr_type,void *pbuffer,long reqOptions,
	long retOptions,long no_elements,TAB_BUFFER *pMsgBuff,int tab_size)
{
    long            precision;
    short           svalue;
    unsigned short  usvalue;
    char           *pmsg;
    int             i,
                    len;
    pmsg = pMsgBuff->message;

    if (reqOptions & DBR_STATUS) {
	if (retOptions & DBR_STATUS) {
	    struct dbr_status *pdbr_status = (void *)pbuffer;

	    printf("status=%u severity=%u\n",
		pdbr_status->status, pdbr_status->severity);
	} else
	    printf("status and severity not returned\n");
	pbuffer = (char *)pbuffer + dbr_status_size;
    }
    if (reqOptions & DBR_UNITS) {
	if (retOptions & DBR_UNITS) {
	    struct dbr_units *pdbr_units = (void *)pbuffer;

	    printf("units=%-8s\n", &pdbr_units->units[0]);
	}else{
	    printf("units not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_units_size;
    }
    if (reqOptions & DBR_PRECISION) {
	precision = *((long *) pbuffer);
	if (retOptions & DBR_PRECISION){
	    struct dbr_precision *pdbr_precision = (void *)pbuffer;

	    printf("precision=%d\n", pdbr_precision->precision);
	}else{
	    printf("precision not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_precision_size;
    }
    if (reqOptions & DBR_TIME) {
	if (retOptions & DBR_TIME) {
	    struct dbr_time *pdbr_time =  (void *)pbuffer;

	    printf("time=%u %u\n", pdbr_time->time.secPastEpoch,
		pdbr_time->time.nsec);
	}else{
	    printf("time not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_time_size;
    }
    if (reqOptions & DBR_ENUM_STRS) {
	if (retOptions & DBR_ENUM_STRS) {
	    struct dbr_enumStrs *pdbr_enumStrs = (void *)pbuffer;

	    printf("no_strs=%u\n", pdbr_enumStrs->no_str);
	    for (i = 0; i <pdbr_enumStrs->no_str; i++) 
		printf("%s\n",&pdbr_enumStrs->strs[i][0]);
	} else
	    printf("enum strings not returned\n");
	pbuffer = (char *)pbuffer + dbr_enumStrs_size;
    }
    if (reqOptions & DBR_GR_LONG) {
	if (retOptions & DBR_GR_LONG) {
	    struct dbr_grLong *pdbr_grLong = (void *)pbuffer;

	    printf("grLong: %d %d\n",pdbr_grLong->upper_disp_limit,
		pdbr_grLong->lower_disp_limit);
	}else{
	    printf("DBRgrLong not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_grLong_size;
    }
    if (reqOptions & DBR_GR_DOUBLE) {
	if (retOptions & DBR_GR_DOUBLE) {
	    struct dbr_grDouble *pdbr_grDouble = (void *)pbuffer;

	    printf("grDouble: %g %g\n",pdbr_grDouble->upper_disp_limit,
		pdbr_grDouble->lower_disp_limit);
	}else{
	    printf("DBRgrDouble not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_grDouble_size;
    }
    if (reqOptions & DBR_CTRL_LONG) {
	if (retOptions & DBR_CTRL_LONG){
	    struct dbr_ctrlLong *pdbr_ctrlLong = (void *)pbuffer;

	    printf("ctrlLong: %d %d\n",pdbr_ctrlLong->upper_ctrl_limit,
		pdbr_ctrlLong->lower_ctrl_limit);
	}else{
	    printf("DBRctrlLong not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_ctrlLong_size;
    }
    if (reqOptions & DBR_CTRL_DOUBLE) {
	if (retOptions & DBR_CTRL_DOUBLE) {
	    struct dbr_ctrlDouble *pdbr_ctrlDouble = (void *)pbuffer;

	    printf("ctrlDouble: %g %g\n",pdbr_ctrlDouble->upper_ctrl_limit,
		pdbr_ctrlDouble->lower_ctrl_limit);
	}else{
	    printf("DBRctrlDouble not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_ctrlDouble_size;
    }
    if (reqOptions & DBR_AL_LONG) {
	if (retOptions & DBR_AL_LONG) {
	    struct dbr_alLong *pdbr_alLong = (void *)pbuffer;

	    printf("alLong: %d %d %d %d\n",
		pdbr_alLong->upper_alarm_limit,pdbr_alLong->upper_warning_limit,
		pdbr_alLong->lower_warning_limit,pdbr_alLong->lower_alarm_limit);
	}else{
	    printf("DBRalLong not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_alLong_size;
    }
    if (reqOptions & DBR_AL_DOUBLE) {
	if (retOptions & DBR_AL_DOUBLE) {
	    struct dbr_alDouble *pdbr_alDouble = (void *)pbuffer;

	    printf("alDouble: %g %g %g %g\n",
		pdbr_alDouble->upper_alarm_limit,pdbr_alDouble->upper_warning_limit,
		pdbr_alDouble->lower_warning_limit,pdbr_alDouble->lower_alarm_limit);
	}else{
	    printf("DBRalDouble not returned\n");
	}
	pbuffer = (char *)pbuffer + dbr_alDouble_size;
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
		sprintf(pmsg, " %s", (char *)pbuffer);
		dbpr_msgOut(pMsgBuff, tab_size);
	    }
	    pbuffer = (char *)pbuffer + MAX_STRING_SIZE;
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
	    pbuffer = (char *)pbuffer + 1;
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
	    pbuffer = (char *)pbuffer + 1;
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
	    pbuffer = (char *)pbuffer + sizeof(short);
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
	    pbuffer = (char *)pbuffer + sizeof(unsigned short);
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
	    pbuffer = (char *)pbuffer + sizeof(long);
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
	    pbuffer = (char *)pbuffer + sizeof(unsigned long);
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
	    pbuffer = (char *)pbuffer + sizeof(float);
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
	    pbuffer = (char *)pbuffer + sizeof(double);
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
	    pbuffer = (char *)pbuffer + sizeof(unsigned short);
	}
	break;
    default:
	printf(" illegal request type.");
	break;
    }
    dbpr_msg_flush(pMsgBuff, tab_size);
    return;
}

static int dbpr_report(
	const char *pname,DBADDR *paddr,int interest_level,
	TAB_BUFFER *pMsgBuff,int tab_size)
{

    char	*pmsg;
    dbFldDes  	*pdbFldDes = paddr->pfldDes;
    dbRecordType    *pdbRecordType = pdbFldDes->pdbRecordType;
    short	n2;
    void	*pfield;
    char	*pfield_name;
    char	*pfield_value;
    DBENTRY	dbentry;
    DBENTRY	*pdbentry = &dbentry;
    long	status;

    dbInitEntry(pdbbase,pdbentry);
    status = dbFindRecord(pdbentry,pname);
    if(status) {
	errMessage(status,pname);
	return(-1);
    }
    pmsg = pMsgBuff->message;
    for (n2 = 0; n2 <= pdbRecordType->no_fields - 1; n2++) {
	pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->sortFldInd[n2]];
	pfield_name = pdbFldDes->name;
	pfield = ((char *)paddr->precord) + pdbFldDes->offset;
	if (pdbFldDes->interest > interest_level )
	    continue;
	switch (pdbFldDes->field_type) {
	case DBF_STRING:
	case DBF_USHORT:
	case DBF_ENUM:
	case DBF_FLOAT:
	case DBF_CHAR:
	case DBF_UCHAR:
	case DBF_SHORT:
	case DBF_LONG:
	case DBF_ULONG:
	case DBF_DOUBLE:
	case DBF_MENU:
	case DBF_DEVICE:
	    status = dbFindField(pdbentry,pfield_name);
            pfield_value = dbGetString(pdbentry);
	    sprintf(pmsg, "%s: %s", pfield_name,
                (pfield_value ? pfield_value : "<nil>"));
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		DBLINK	*plink = (DBLINK *)pfield;
		int	ind;

		status = dbFindField(pdbentry,pfield_name);
		for(ind=0; ind<LINK_NTYPES; ind++) {
		    if(pamaplinkType[ind].value == plink->type) break;
		}
		if(ind>=LINK_NTYPES) {
		    sprintf(pmsg,"%4s: Illegal Link Type", pfield_name);
		} else {
		    sprintf(pmsg,"%s:%s %s", pfield_name,
		        pamaplinkType[ind].strvalue,dbGetString(pdbentry));
		}
		dbpr_msgOut(pMsgBuff, tab_size);
	    }
	    break;
	case DBF_NOACCESS:
	    { /* lets just print field in hex */
		char * pchar = (char *)(pfield);
		char   temp_buf[42];
		char *ptemp_buf = &temp_buf[0];
		short n = pdbFldDes->size;
		short i;
                unsigned int value;

		strcpy(ptemp_buf,"0x"); ptemp_buf+=2;
		if(n>(sizeof(temp_buf)-2)/2) n = (sizeof(temp_buf)-2)/2;
		for (i=0; i<n; i++, (ptemp_buf+=2), pchar++) {
                        value = (unsigned int)(*(unsigned char *)pchar);
			sprintf(ptemp_buf,"%02x",value);
		}
		*ptemp_buf = 0;
		sprintf(pmsg, "%s: %s", pfield_name,temp_buf);
		dbpr_msgOut(pMsgBuff, tab_size);
	    }
	    break;
	default:
	    sprintf(pmsg, "%s: dbpr: Unknown field_type", pfield_name);
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
    }
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    dbFinishEntry(pdbentry);
    return (0);
}

static void dbpr_msgOut(TAB_BUFFER *pMsgBuff,int tab_size)
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

static void dbpr_init_msg(TAB_BUFFER *pMsgBuff,int tab_size)
{
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pLast = pMsgBuff->out_buff + MAXLINE;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
    return;
}

static void dbpr_insert_msg(TAB_BUFFER *pMsgBuff,int len,int tab_size)
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

static void dbpr_msg_flush(TAB_BUFFER *pMsgBuff,int tab_size)
{
    /* skip print if buffer empty */
    if (pMsgBuff->pNext != pMsgBuff->out_buff)
	printf("%s\n", pMsgBuff->out_buff);
    memset(pMsgBuff->out_buff,'\0', (int) MAXLINE + 1);
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
    return;
}
