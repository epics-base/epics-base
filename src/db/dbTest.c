
/* dbTest.c */
/* share/src/db $Id$ */

/* dbTest.c
 *
 *	database access test subroutines
 *
 *	Author:	Bob Dalesio(LANL) and Marty Kraimer(ANL)
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Bob Dalesio, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-3414
 *	E-mail: dalesio@luke.lanl.gov
 *
 * Modification Log:
 * -----------------
 * .xx mm-dd-yy         mrk	Comment
 *
 */

/* Global Database Test Routines - All can be invoked via vxWorks shell
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
 * dbior(pname)			io_report
 *	char	*pname		Driver name. If null all drivers
 *
 * dblls(ptypeName)		list lock sets
 *	char	*ptypeName;	Record type. If null all record types
*###############NEW BELOW rz1
* The following are the report record processing rouitines
*
* int dbprReportLink(pMsgBuff,pfield_name,plink,tab_size)
*     TAB_BUFFER     *pMsgBuff;
*     char           *pfield_name;
*     struct link    *plink;
*     int             tab_size;
*
* int dbprReportCvtChoice(pMsgBuff,pfield_name,choice_value,tab_size)
*     TAB_BUFFER     *pMsgBuff;
*     char		*pfield_name;
*     unsigned short	choice_value;
*     int             tab_size;
*
* int dbprReportGblChoice(pMsgBuff,precord,pfield_name,choice_value,tab_size)
*     TAB_BUFFER     *pMsgBuff;
*     struct dbCommon 	*precord;
*     char	    	*pfield_name;
*     unsigned short	choice_value;
*     int             tab_size;
*
* int dbprReportRecChoice(pMsgBuff,precord,pfield_name,choice_value,tab_size)
*     TAB_BUFFER     *pMsgBuff;
*     struct dbCommon 	*precord;
*     char	    	*pfield_name;
*     unsigned short	choice_value;
*     int             tab_size;

* int dbprReportDevChoice(pMsgBuff,paddr,pfield_name, tab_size)
*     TAB_BUFFER     *pMsgBuff;
*     struct dbAddr  *paddr;
*     char           *pfield_name;
*     int             tab_size;
################NEW ABOVE rz2
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<strLib.h>
#include	<stdioLib.h>

#include	<fast_lock.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbRecType.h>
#include	<dbRecords.h>
#include	<dbCommon.h>
#include	<recSup.h>
#include	<drvSup.h>

/*###############NEW BELOW rz1*/

#include	<choice.h>
#include	<special.h>
#include	<dbDefs.h>
#include	<dbRecDes.h>
#include	<dbPvd.h>
#include	<link.h>

#define BFSIZE 50
#define MAXLINE 80
struct msgBuff {		/* line output structure */
    char            out_buff[MAXLINE + 1];
    char           *pNext;
    char           *pLast;
    char           *pNexTab;
    char            message[128];
};
typedef struct msgBuff TAB_BUFFER;
/*################NEW ABOVE rz2*/
#define MIN(x,y)        ((x < y)?x:y)
#define MAX(x,y)        ((x > y)?x:y)

/* Local Routines */
void printDbAddr();
void printBuffer();
/*###############NEW BELOW rz1*/
static int      dbpr_report();
struct fldDes  *dbprGetFldRec();
struct recTypDes *dbprGetRecTypDes();
/*################NEW ABOVE rz2*/

long dbl(ptypeName)	/* list process variables for specified record type*/
	char	*ptypeName;
{
    int			rectype,beg,end,recnum;
    struct recLoc	*precLoc;
    struct dbCommon	*precord;
    char		*pstr;
    char		name[PVNAME_SZ+1];

    if(dbRecType==NULL || dbRecords==NULL) return(1);
    if(ptypeName==NULL) {
	beg=0;
	end=dbRecords->number - 1;
    }
    else {
	for(rectype=0; rectype<dbRecType->number; rectype++) {
	    if(!(pstr=GET_PRECTYPE(rectype))) continue;
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
	if(!(precLoc=GET_PRECLOC(rectype))) continue;
	for(recnum=0; precord=(struct dbCommon *)(GET_PRECORD(precLoc,recnum));
	recnum++) {
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
    long buffer[100];
    long *pbuffer=&buffer[0];
    struct dbAddr addr;
    long	  status;
    long	  options,no_elements;
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER     *pMsgBuff = &msg_Buff;
    char *pmsg = pMsgBuff->message;
    int             tab_size;
    tab_size = 10;

    status=dbNameToAddr(pname,&addr);
    printDbAddr(status,&addr);
    if(status) return(1);
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
    options=0;
    if(addr.dbr_field_type==DBR_ENUM) {
	status=dbGetField(&addr,DBR_STRING,pbuffer,&options,&no_elements);
	printBuffer(status,DBR_STRING,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    }
    else {
	status=dbGetField(&addr,addr.dbr_field_type,pbuffer,&options,&no_elements);
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    }
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

long dbpf(pname,pvalue)	/* put field value*/
	char	*pname;
	char	*pvalue;
{
    /* declare buffer long just to ensure correct alignment */
    long buffer[100];
    long *pbuffer=&buffer[0];
    struct dbAddr addr;
    long	  status;
    long	  options,no_elements;

    status=dbNameToAddr(pname,&addr);
    if(status) {
         errMessage(status,"dbNameToAddr error");
         return(1);
    }
    status=dbPutField(&addr,DBR_STRING,pvalue,1L);
    if(status) {
         errMessage(status,"dbPutField error");
         return(1);
    }
    if(status!=0) errPrint(status);
    else return(dbgf(pname));
}

/*###############NEW BELOW rz1*/
long
dbpr(pname, interest_level)	/* print record */
    char           *pname;
    int             interest_level;
{
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER     *pMsgBuff = &msg_Buff;
    struct dbAddr   addr;
    long            status;
    long            options,
                    no_elements;
    char           *pmsg;
    struct rset    *prset;
    int             tab_size;
    pmsg = pMsgBuff->message;
    tab_size = 20;

    status = dbNameToAddr(pname, &addr);
    printDbAddr(status, &addr);

    if (status)
	return (1);
    if (!(prset = GET_PRSET(addr.record_type))) {
	printf("No record Support for this record type\n");
	return (1);
    }
    if (dbpr_report(pname, &addr, interest_level, pMsgBuff,
	       addr.field_type, tab_size))
	return (1);
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return (0);
}
/*################NEW ABOVE rz2*/
#if 0  /************************************************/
long dbpr(pname)	/* print record */
	char	*pname;
{
    struct dbAddr addr;
    long	  status;
    long	  options,no_elements;
    struct rset   *prset;
    long int      (*report)()=NULL;

    status=dbNameToAddr(pname,&addr);
    printDbAddr(status,&addr);
    if(status) return(1);
    if(!(prset=GET_PRSET(addr.record_type))) {
	printf("No record Support for this record type\n");
	return(1);
    }
#if 0
    if(!(report = (prset->report))) {
	printf("No report routine in RSET\n");
	return(1);
    }
    status=(*report)(stdout,&addr);
    fflush(stdout);
    if(!RTN_SUCCESS(status))
	recGblRecSupError(S_db_noSupport,&addr,"dbpr","report");
#endif
    return(0);
}
#endif /************************************************/

long dbtr(pname)	/* test record and print*/
	char	*pname;
{
    struct dbAddr addr;
    long	  status;
    long	  options,no_elements;
    struct rset   *prset;
    long int      (*process)()=NULL;
    struct dbCommon	*precord;

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
    status=dbProcess(&addr);
    if(!RTN_SUCCESS(status))
	recGblRecSupError(S_db_noSupport,&addr,"dbtr","process");
    dbpr(pname,0);
    return(0);
}

long dbtgf(pname)	/* test all options for dbGetField */
	char	*pname;
{
    /* declare buffer long just to ensure correct alignment */
    long buffer[300];
    long *pbuffer=&buffer[0];
    struct dbAddr addr;
    long	  status;
    long	  req_options,ret_options,no_elements;
    short	  dbr_type;
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER     *pMsgBuff = &msg_Buff;
    char *pmsg = pMsgBuff->message;
    int             tab_size;
    tab_size = 10;

    status=dbNameToAddr(pname,&addr);
    printDbAddr(status,&addr);
    if(status)return;
    /* try all options first */
    req_options=0xffffffff;
    ret_options=req_options;
    no_elements=0;
    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
	&ret_options,&no_elements);
    printBuffer(status,addr.dbr_field_type,pbuffer, req_options,ret_options,no_elements,pMsgBuff,tab_size);
    /* Now try all request types */
    ret_options=0;
    dbr_type=DBR_STRING;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/MAX_STRING_SIZE));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_CHAR;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(char)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_UCHAR;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned char)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_SHORT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_USHORT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_LONG;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(long)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_ULONG;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned long)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_FLOAT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(float)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_DOUBLE;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(double)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    dbr_type=DBR_ENUM;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

long dbtpf(pname,pvalue)/* test all options for dbPutField */
	char	*pname;
{
    /* declare buffer long just to ensure correct alignment */
    long buffer[100];
    long *pbuffer=&buffer[0];
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
    char *pmsg = pMsgBuff->message;
    int             tab_size;
    tab_size = 10;

    status=dbNameToAddr(pname,&addr);
    printDbAddr(status,&addr);
    if(status) return(1);
    /* DBR_STRING */
    status=dbPutField(&addr,DBR_STRING,pvalue,1L);
    if(status!=0) errPrint(status);
    else {
	printf("DBR_STRING ok\n");
	no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	options=0;
	status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
    }
    /* DBR_CHAR */
    if(sscanf(pvalue,"%hd",&svalue)==1) {
	cvalue = (char)svalue;
	status=dbPutField(&addr,DBR_CHAR,&cvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_UCHAR ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
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
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_UCHAR\n");
    /* DBR_SHORT */
    if(sscanf(pvalue,"%hd",&svalue)==1) {
	status=dbPutField(&addr,DBR_SHORT,&svalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_SHORT ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_SHORT\n");
    /* DBR_USHORT */
    if(sscanf(pvalue,"%hu",&usvalue)==1) {
	status=dbPutField(&addr,DBR_USHORT,&usvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_USHORT ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_USHORT\n");
    /* DBR_LONG */
    if(sscanf(pvalue,"%ld",&lvalue)==1) {
	status=dbPutField(&addr,DBR_LONG,&lvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_LONG ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_LONG\n");
    /* DBR_ULONG */
    if(sscanf(pvalue,"%lu",&ulvalue)==1) {
	status=dbPutField(&addr,DBR_ULONG,&ulvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_ULONG ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_ULONG\n");
    /* DBR_FLOAT */
    if(sscanf(pvalue,"%e",&fvalue)==1) {
	status=dbPutField(&addr,DBR_FLOAT,&fvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_FLOAT ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_FLOAT\n");
    /* DBR_DOUBLE */
    if(sscanf(pvalue,"%le",&dvalue)==1) {
	status=dbPutField(&addr,DBR_DOUBLE,&dvalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_DOUBLE ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_DOUBLE\n");
    /* DBR_ENUM */
    if(sscanf(pvalue,"%hu",&svalue)==1) {
	status=dbPutField(&addr,DBR_ENUM,&svalue,1L);
	if(status!=0) errPrint(status);
	else {
	    printf("DBR_ENUM ok\n");
	    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
	    options=0;
	    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
		&options,&no_elements);
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);
	}
    } else printf("sscanf failed for DBR_SHORT\n");
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

static void printDbAddr(status,paddr)
    long	  status;
    struct dbAddr *paddr;
{

    if(status!=0) {
	errMessage(status,"dbNameToAddr error");
    }
    printf("Record Address: 0x%x",paddr->precord);
    printf(" Field Address: 0x%x",paddr->pfield);
    printf(" Field Description: 0x%x\n",paddr->pfldDes);
    printf("   No Elements: %ld\n",paddr->no_elements);
    printf("   Record Type: %d\n",paddr->record_type);
    printf("    Field Type: %d\n",paddr->field_type);
    printf("    Field Size: %d\n",paddr->field_size);
    printf("       Special: %d\n",paddr->special);
    printf("    Choice Set: %d\n",paddr->choice_set);
    printf("DBR Field Type: %d\n",paddr->dbr_field_type);
}

/*###############NEW BELOW rz1*/
void
printBuffer(status, dbr_type, pbuffer, reqOptions, retOptions, no_elements, pMsgBuff, tab_size)
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
    short           sarr[6];
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
		printf("%s\n", (pbuffer + 4 + i * 26));
	} else
	    printf("enum strings not returned\n");
	pbuffer += dbr_enumStrs_size;
    }
    if (reqOptions & DBR_GR_UCHAR) {
	usarr[0] = *((unsigned char *) pbuffer);
	usarr[1] = *((unsigned char *) (pbuffer + 1));
	usarr[2] = *((unsigned char *) (pbuffer + 2));
	usarr[3] = *((unsigned char *) (pbuffer + 3));
	usarr[4] = *((unsigned char *) (pbuffer + 4));
	usarr[5] = *((unsigned char *) (pbuffer + 5));
	if (retOptions & DBR_GR_UCHAR)
	    printf("grUchar: %u %u %u %u %u %u\n",
		usarr[0], usarr[1], usarr[2], usarr[3], usarr[4], usarr[5]);
	else
	    printf("DBRgrUchar not returned\n");
	pbuffer += dbr_grUchar_size;
    }
    if (reqOptions & DBR_GR_SHORT) {
	if (retOptions & DBR_GR_SHORT)
	    printf("grShort: %d %d %d %d %d %d\n",
		   *(short *) (pbuffer),
		   *(short *) (pbuffer + 2),
		   *(short *) (pbuffer + 4),
		   *(short *) (pbuffer + 6),
		   *(short *) (pbuffer + 8),
		   *(short *) (pbuffer + 10));
	else
	    printf("DBRgrShort not returned\n");
	pbuffer += dbr_grShort_size;
    }
    if (reqOptions & DBR_GR_LONG) {
	if (retOptions & DBR_GR_LONG)
	    printf("grLong: %ld %ld %ld %ld %ld %ld\n",
		   *(long *) (pbuffer),
		   *(long *) (pbuffer + 4),
		   *(long *) (pbuffer + 8),
		   *(long *) (pbuffer + 12),
		   *(long *) (pbuffer + 16),
		   *(long *) (pbuffer + 20));
	else
	    printf("DBRgrLong not returned\n");
	pbuffer += dbr_grLong_size;
    }
    if (reqOptions & DBR_GR_ULONG) {
	if (retOptions & DBR_GR_ULONG)
	    printf("grUlong: %lu %lu %lu %lu %lu %lu\n",
		   *(unsigned long *) (pbuffer),
		   *(unsigned long *) (pbuffer + 4),
		   *(unsigned long *) (pbuffer + 8),
		   *(unsigned long *) (pbuffer + 12),
		   *(unsigned long *) (pbuffer + 16),
		   *(unsigned long *) (pbuffer + 20));
	else
	    printf("DBRgrUlong not returned\n");
	pbuffer += dbr_grUlong_size;
    }
    if (reqOptions & DBR_GR_FLOAT) {
	if (retOptions & DBR_GR_FLOAT)
	    printf("grFloat: %g %g %g %g %g %g\n",
		   *(float *) (pbuffer),
		   *(float *) (pbuffer + 4),
		   *(float *) (pbuffer + 8),
		   *(float *) (pbuffer + 12),
		   *(float *) (pbuffer + 16),
		   *(float *) (pbuffer + 20));
	else
	    printf("DBRgrFloat not returned\n");
	pbuffer += dbr_grFloat_size;
    }
    if (reqOptions & DBR_GR_DOUBLE) {
	if (retOptions & DBR_GR_DOUBLE)
	    printf("grDouble: %lg %lg %lg %lg %lg %lg\n",
		   *(double *) (pbuffer),
		   *(double *) (pbuffer + 8),
		   *(double *) (pbuffer + 16),
		   *(double *) (pbuffer + 24),
		   *(double *) (pbuffer + 32),
		   *(double *) (pbuffer + 40));
	else
	    printf("DBRgrDouble not returned\n");
	pbuffer += dbr_grDouble_size;
    }
    if (reqOptions & DBR_CTRL_UCHAR) {
	usarr[0] = *((unsigned char *) pbuffer);
	usarr[1] = *((unsigned char *) (pbuffer + 1));
	if (retOptions & DBR_CTRL_UCHAR)
	    printf("ctrlUchar: %u %u\n",
		   usarr[0], usarr[1]);
	else
	    printf("DBRctrlUchar not returned\n");
	pbuffer += dbr_ctrlUchar_size;
    }
    if (reqOptions & DBR_CTRL_SHORT) {
	if (retOptions & DBR_CTRL_SHORT)
	    printf("ctrlShort: %d %d\n",
		   *(short *) (pbuffer),
		   *(short *) (pbuffer + 2));
	else
	    printf("DBRctrlShort not returned\n");
	pbuffer += dbr_ctrlShort_size;
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
    if (reqOptions & DBR_CTRL_ULONG) {
	if (retOptions & DBR_CTRL_ULONG)
	    printf("ctrlUlong: %lu %lu\n",
		   *(unsigned long *) (pbuffer),
		   *(unsigned long *) (pbuffer + 4));
	else
	    printf("DBRctrlUlong not returned\n");
	pbuffer += dbr_ctrlUlong_size;
    }
    if (reqOptions & DBR_CTRL_FLOAT) {
	if (retOptions & DBR_CTRL_FLOAT)
	    printf("ctrlFloat: %g %g\n",
		   *(float *) (pbuffer),
		   *(float *) (pbuffer + 4));
	else
	    printf("DBRctrlFloat not returned\n");
	pbuffer += dbr_ctrlFloat_size;
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
    /* Now print values */
    if (no_elements == 0)
	return;
    switch (dbr_type) {
    case (DBR_STRING):
	if (status != 0) {
	    sprintf(pmsg, "DBR_STRING: failed.");
	    dbpr_msgOut(pMsgBuff, tab_size);
	    break;
	}
	len = strlen(pbuffer);
	if (len > 0) {
	    sprintf(pmsg, "DBR_STRING: %s", pbuffer);
	    dbpr_msgOut(pMsgBuff, tab_size);
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
	    sprintf(pmsg, "%-9d", svalue);
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
	    sprintf(pmsg, "%-9d", usvalue);
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
	    sprintf(pmsg, "%-9d", *((short *) pbuffer));
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
	    sprintf(pmsg, "%-3u", *((unsigned short *) pbuffer));
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
	    sprintf(pmsg, "%-9ld", *((long *) pbuffer));
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
	    sprintf(pmsg, "0x%-8lx", *((unsigned long *) pbuffer));
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
	    sprintf(pmsg, "%-9.2g", *((float *) pbuffer));
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
	    sprintf(pmsg, "%-9.2g", *((double *) pbuffer));
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
	}
	break;
    default:
	printf(" illegal request type.");
	break;
    }
    return;
}
/*################NEW ABOVE rz2*/

#if 0  /************************************************/

static void printBuffer(status,dbr_type,pbuffer,reqOptions,retOptions,no_elements)
    long	  status;
    short	  dbr_type;
    char	  *pbuffer;
    long	  reqOptions;
    long	  retOptions;
    long	  no_elements;
{
    unsigned short stat,severity;
    long	   precision;
    unsigned long  epoch_seconds;
    unsigned long  nano_seconds;
    unsigned long  no_strs;
    short 	   sarr[6];
    short 	   svalue;
    unsigned short usarr[6];
    unsigned short usvalue;
    int		   i;

    if(status) {
        errMessage(status,"dbGetField error");
    }
    if(reqOptions&DBR_STATUS) {
	if(retOptions&DBR_STATUS) {
	    stat = *((unsigned short*)pbuffer);
	    severity = *((unsigned short*)(pbuffer+2));
	    printf("status=%u severity=%u\n",stat,severity);
	} else printf("status and severity not returned\n");
	pbuffer += dbr_status_size;
    }
    if(reqOptions&DBR_UNITS) {
	if(retOptions&DBR_UNITS) printf("units=%-8s\n",pbuffer);
	else printf("units not returned\n");
	pbuffer += dbr_units_size;
    }
    if(reqOptions&DBR_PRECISION) {
	precision = *((long*)pbuffer);
	if(retOptions&DBR_PRECISION) printf("precision=%ld\n",precision);
	else printf("precision not returned\n");
	pbuffer += dbr_precision_size;
    }
    if(reqOptions&DBR_TIME) {
	epoch_seconds = *((unsigned long*)pbuffer);
	nano_seconds = *((unsigned long*)(pbuffer+4));
	if(retOptions&DBR_TIME)
		printf("time=%lu %lu\n",epoch_seconds,nano_seconds);
	else printf("time not returned\n");
	pbuffer += dbr_time_size;
    }
    if(reqOptions&DBR_ENUM_STRS) {
	if(retOptions&DBR_ENUM_STRS) {
	    no_strs = *((unsigned long*)pbuffer);
	    printf("no_strs=%lu\n",no_strs);
	    for(i=0; i < *((unsigned long *)pbuffer); i++)
		printf("%s\n",(pbuffer + 4 + i*26));
	} else printf("enum strings not returned\n");
	pbuffer += dbr_enumStrs_size;
    }
    if(reqOptions&DBR_GR_UCHAR) {
	usarr[0]=*((unsigned char *)pbuffer);
	usarr[1]=*((unsigned char *)(pbuffer+1));
	usarr[2]=*((unsigned char *)(pbuffer+2));
	usarr[3]=*((unsigned char *)(pbuffer+3));
	usarr[4]=*((unsigned char *)(pbuffer+4));
	usarr[5]=*((unsigned char *)(pbuffer+5));
	if(retOptions&DBR_GR_UCHAR)
	    printf("grUchar: %u %u %u %u %u %u\n",
		usarr[0],usarr[1],usarr[2],usarr[3],usarr[4],usarr[5]);
	else printf("DBRgrUchar not returned\n");
	pbuffer += dbr_grUchar_size;
    }
    if(reqOptions&DBR_GR_SHORT) {
	if(retOptions&DBR_GR_SHORT)
	    printf("grShort: %d %d %d %d %d %d\n",
		*(short *)(pbuffer),
		*(short *)(pbuffer+2),
		*(short *)(pbuffer+4),
		*(short *)(pbuffer+6),
		*(short *)(pbuffer+8),
		*(short *)(pbuffer+10));
	else printf("DBRgrShort not returned\n");
	pbuffer += dbr_grShort_size;
    }
    if(reqOptions&DBR_GR_LONG) {
	if(retOptions&DBR_GR_LONG)
	    printf("grLong: %ld %ld %ld %ld %ld %ld\n",
		*(long *)(pbuffer),
		*(long *)(pbuffer+4),
		*(long *)(pbuffer+8),
		*(long *)(pbuffer+12),
		*(long *)(pbuffer+16),
		*(long *)(pbuffer+20));
	else printf("DBRgrLong not returned\n");
	pbuffer += dbr_grLong_size;
    }
    if(reqOptions&DBR_GR_ULONG) {
	if(retOptions&DBR_GR_ULONG)
	    printf("grUlong: %lu %lu %lu %lu %lu %lu\n",
		*(unsigned long *)(pbuffer),
		*(unsigned long *)(pbuffer+4),
		*(unsigned long *)(pbuffer+8),
		*(unsigned long *)(pbuffer+12),
		*(unsigned long *)(pbuffer+16),
		*(unsigned long *)(pbuffer+20));
	else printf("DBRgrUlong not returned\n");
	pbuffer += dbr_grUlong_size;
    }
    if(reqOptions&DBR_GR_FLOAT) {
	if(retOptions&DBR_GR_FLOAT)
	    printf("grFloat: %g %g %g %g %g %g\n",
		*(float *)(pbuffer),
		*(float *)(pbuffer+4),
		*(float *)(pbuffer+8),
		*(float *)(pbuffer+12),
		*(float *)(pbuffer+16),
		*(float *)(pbuffer+20));
	else printf("DBRgrFloat not returned\n");
	pbuffer += dbr_grFloat_size;
    }
    if(reqOptions&DBR_GR_DOUBLE) {
	if(retOptions&DBR_GR_DOUBLE)
	    printf("grDouble: %lg %lg %lg %lg %lg %lg\n",
		*(double *)(pbuffer),
		*(double *)(pbuffer+8),
		*(double *)(pbuffer+16),
		*(double *)(pbuffer+24),
		*(double *)(pbuffer+32),
		*(double *)(pbuffer+40));
	else printf("DBRgrDouble not returned\n");
	pbuffer += dbr_grDouble_size;
    }
    if(reqOptions&DBR_CTRL_UCHAR) {
	usarr[0]=*((unsigned char *)pbuffer);
	usarr[1]=*((unsigned char *)(pbuffer+1));
	if(retOptions&DBR_CTRL_UCHAR)
	    printf("ctrlUchar: %u %u\n",
		usarr[0],usarr[1]);
	else printf("DBRctrlUchar not returned\n");
	pbuffer += dbr_ctrlUchar_size;
    }
    if(reqOptions&DBR_CTRL_SHORT) {
	if(retOptions&DBR_CTRL_SHORT)
	    printf("ctrlShort: %d %d\n",
		*(short *)(pbuffer),
		*(short *)(pbuffer+2));
	else printf("DBRctrlShort not returned\n");
	pbuffer += dbr_ctrlShort_size;
    }
    if(reqOptions&DBR_CTRL_LONG) {
	if(retOptions&DBR_CTRL_LONG)
	    printf("ctrlLong: %ld %ld\n",
		*(long *)(pbuffer),
		*(long *)(pbuffer+4));
	else printf("DBRctrlLong not returned\n");
	pbuffer += dbr_ctrlLong_size;
    }
    if(reqOptions&DBR_CTRL_ULONG) {
	if(retOptions&DBR_CTRL_ULONG)
	    printf("ctrlUlong: %lu %lu\n",
		*(unsigned long *)(pbuffer),
		*(unsigned long *)(pbuffer+4));
	else printf("DBRctrlUlong not returned\n");
	pbuffer += dbr_ctrlUlong_size;
    }
    if(reqOptions&DBR_CTRL_FLOAT) {
	if(retOptions&DBR_CTRL_FLOAT)
	    printf("ctrlFloat: %g %g\n",
		*(float *)(pbuffer),
		*(float *)(pbuffer+4));
	else printf("DBRctrlFloat not returned\n");
	pbuffer += dbr_ctrlFloat_size;
    }
    if(reqOptions&DBR_CTRL_DOUBLE) {
	if(retOptions&DBR_CTRL_DOUBLE)
	    printf("ctrlDouble: %lg %lg\n",
		*(double *)(pbuffer),
		*(double *)(pbuffer+8));
	else printf("DBRctrlDouble not returned\n");
	pbuffer += dbr_ctrlDouble_size;
    }
    /*Now print values*/
    if(no_elements==0)return;
    switch(dbr_type) {
	case(DBR_STRING):
    		if(status!=0) {printf("DBR_STRING: failed."); break;}
		printf("DBR_STRING: %s",pbuffer);
		break;
	case(DBR_CHAR):
		printf("DBR_CHAR:  ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			svalue=*(char*)pbuffer;
			printf("%-3d ",svalue);
			if (i%10 == 9) printf("\n");
			pbuffer += 1;
		}
		break;
	case(DBR_UCHAR):
		printf("DBR_UCHAR:  ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			usvalue=*(unsigned char*)pbuffer;
			printf("%-3d ",usvalue);
			if (i%10 == 9) printf("\n");
			pbuffer += 1;
		}
		break;
	case(DBR_SHORT):
		printf("DBR_SHORT:  ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			printf("%-6d ",*((short *)pbuffer));
			if (i%10 == 9) printf("\n");
			pbuffer += 2;
		}
		break;
	case(DBR_USHORT):
		printf("DBR_USHORT:  ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			printf("%-6u ",*((unsigned short *)pbuffer));
			if (i%10 == 9) printf("\n");
			pbuffer += 2;
		}
		break;
	case(DBR_LONG):
		printf("DBR_LONG:   ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			printf("%-10ld ",*((long *)pbuffer));
			if (i%5 == 4) printf("\n");
			pbuffer += 4;
		}
		break;
	case(DBR_ULONG):
		printf("DBR_ULONG:  ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			printf("0x%-8lx ",*((unsigned long *)pbuffer));
			if (i%5 == 4) printf("\n");
			pbuffer += 4;
		}
		break;
	case(DBR_FLOAT):
		printf("DBR_FLOAT:  ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			printf("%-9.2g ",*((float *)pbuffer));
			if (i%10 == 9) printf("\n");
			pbuffer += 4;
		}
		break;
	case(DBR_DOUBLE):
		printf("DBR_DOUBLE: ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			printf("%-9.2g ",*((double *)pbuffer));
			if (i%10 == 9) printf("\n");
			pbuffer += 8;
		}
		break;
	case(DBR_ENUM):
		printf("DBR_ENUM:   ");
    		if(status!=0) {printf(" failed."); break;}
		for(i=0; i<no_elements; i++) {
			printf("%-6u ",*((unsigned short *)pbuffer));
			if (i%10 == 9) printf("\n");
		}
		break;
	default:
    		printf(" illegal request type.");
		break;
    }
    printf("\n");
    return;
}
#endif /************************************************/

long dbior(pdrvName)
    char	*pdrvName;
{
    int		 i;
    char	 *pname;
    struct drvet *pdrvet;
    int printIt;

    if(!drvSup) {
	printf("No drivers\n");
	return(0);
    }
    for (i=0; i<drvSup->number; i++) {
	if((pname=drvSup->drvetName[i])==NULL) continue;
	if(pdrvName==NULL)
	    printIt=TRUE;
	else {
	    printIt = (strcmp(pdrvName,pname)==0 ? TRUE : FALSE);
	    if(!printIt) continue;
	}
	if((pdrvet=drvSup->papDrvet[i])==NULL) {
		printf("No driver entry table is present for %s\n",pname);
		continue;
	}
	if(pdrvet->report==NULL) {
		if(printIt&&(pdrvName!=NULL)) printf("No report available\n");	
	}
	else {
	    printf("Driver: %s\n",pname);
	    (*pdrvet->report)();
	}
	if(pdrvName!=NULL) break;
    }
    return(0);
}

long dblls(ptypeName)	/* list lock set for specified record type*/
	char	*ptypeName;
{
    int			rectype,beg,end,recnum;
    struct recLoc	*precLoc;
    struct dbCommon	*precord;
    char		*pstr;
    char		name[PVNAME_SZ+1];

    if(dbRecType==NULL || dbRecords==NULL) return(1);
    if(ptypeName==NULL) {
	beg=0;
	end=dbRecords->number - 1;
    }
    else {
	for(rectype=0; rectype<dbRecType->number; rectype++) {
	    if(!(pstr=GET_PRECTYPE(rectype))) continue;
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
	if(!(precLoc=GET_PRECLOC(rectype))) continue;
	for(recnum=0; precord=(struct dbCommon *)(GET_PRECORD(precLoc,recnum));
	recnum++) {
		if(precord->name[0] == 0) continue; /*deleted record*/
		strncpy(name,precord->name,PVNAME_SZ);
		name[PVNAME_SZ]=0;
		printf("%4.4d %4.4d %4.4d %4.4d %4.4d %s\n",
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
/*###############NEW BELOW rz1*/
/**/
static int
dbpr_report(pname, paddr, interest_level, pMsgBuff, field_type, tab_size)
    char           *pname;
    struct dbAddr  *paddr;	/* requested PV name */
    int             interest_level;
    TAB_BUFFER     *pMsgBuff;
    short           field_type;
    int             tab_size;
{

    char           *pmsg;
    char           *pName;
    char           *pfname;
    struct fldDes  *pfldDes;
    struct recTypDes *precTypDes;
    struct dbAddr  *pLaddr;
    struct dbAddr   Laddr;	/* local address */
    short           no_fields;
    short           fldNum;	/* field number		 */
    char           *pRecordName;
    char            RecordName[PVNAME_SZ + 2];	/* pv record name */
    char           *pfield_name;
    char            field_name[FLDNAME_SZ + 1];	/* pv field name */
    char           *pPvName;
    char            PvName[PVNAME_SZ + FLDNAME_SZ + 4];	/* recordname.<pvname> */
    short           n;
    long            status;
    int             i;
    int             debug = 0;
    long            buffer[BFSIZE];	/* limit array buffer to 1st 50 */
    caddr_t         pbuffer;
    long            options;
    long            nRequest;
    int             sv_tab_size;
    pmsg = pMsgBuff->message;

    /* extract record name */
    pbuffer = (char *) buffer;
    pRecordName = &RecordName[0];
    pName = pname;
    n = 0;
    while (*pName && (*pName != '.') && n < sizeof(RecordName)) {
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

    for (fldNum = 0; fldNum <= precTypDes->no_fields - 1; fldNum++) {
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
	if (status)
	    return (1);
	if (pfldDes->interest < interest_level )
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
	    debug++;
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
	nRequest = MIN(BFSIZE, pLaddr->no_elements);
	status = dbGetField(&Laddr, Laddr.dbr_field_type, pbuffer,
			    &options, &nRequest);
	printBuffer(status, Laddr.dbr_field_type, pbuffer,
		    0L, 0L, nRequest, pMsgBuff, tab_size);
	tab_size = sv_tab_size;
	/* flush buffer and restore tab size */
	pmsg[0] = '\0';
	dbpr_msgOut(pMsgBuff, tab_size);
    }
    return (0);
}
/*################NEW ABOVE rz2*/
/*###############NEW BELOW rz1*/
/**/
static
dbpr_msgOut(pMsgBuff, tab_size)
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{
    int             len;
    int             err = 0;
    char           *pmsg = pMsgBuff->message;
    static int      last_tabsize;
    if (!((tab_size == 10) || (tab_size == 20))) {
	printf("tab_size not 10 or 20 - dbpr_msgOut()\n");
	return (1);
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
	return (0);
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
	dbpr_insert_msg(pMsgBuff, len);
	return (1);
    }
    return (0);
}
/*################NEW ABOVE rz2*/

/*###############NEW BELOW rz1*/
static
dbpr_init_msg(pMsgBuff, tab_size)
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pLast = pMsgBuff->out_buff + MAXLINE;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
    return;
}
/**/
static
dbpr_insert_msg(pMsgBuff, len, tab_size)
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
#if 0
    /* output if line is full */
    if (*(pMsgBuff->pLast - 1) == ' ')
	dbpr_msg_flush(pMsgBuff, tab_size);
#endif
    return;

}

static
dbpr_msg_flush(pMsgBuff, tab_size)
    TAB_BUFFER     *pMsgBuff;
    int             tab_size;
{
    /* skip print if buffer empty */
    if (pMsgBuff->pNext != pMsgBuff->out_buff)
	printf("%s\n", pMsgBuff->out_buff);
    bzero(pMsgBuff->out_buff, (int) MAXLINE + 1);
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
    return;
}
/*################NEW ABOVE rz2*/
/*###############NEW BELOW rz1*/

int dbprReportLink(pMsgBuff,pfield_name,plink,field_type, tab_size)
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
	sprintf(pmsg,"%4s: GPIB link=%2d taddr=%2d laddr=%2d signal=%3d",
	    pfield_name,
	    plink->value.gpibio.link, plink->value.gpibio.taddr,
	    plink->value.gpibio.laddr,plink->value.gpibio.signal);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    case BITBUS_IO:
	sprintf(pmsg,"%4s: BITBUS link=%2d addr=%2d signal=%3d",
	    pfield_name,
	    plink->value.bitbusio.link,plink->value.bitbusio.addr,
	    plink->value.bitbusio.signal);
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
	sprintf(pmsg,"%4s: CA_LINK pp=%1d ms=%1d Not Yet Implemented",
	    pfield_name,
	    plink->value.ca_link.process_passive,
	    plink->value.ca_link.maximize_sevr);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    default:
	sprintf(pmsg,"%4s: dbprReportLink: Illegal link.type",
	    pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
	break;
    }
    return(0);
}

int dbprReportCvtChoice(pMsgBuff,pfield_name,choice_value,tab_size)
    TAB_BUFFER     *pMsgBuff;
    char           *pfield_name;
    unsigned short  choice_value;
    int             tab_size;
{
    char *pchoice;
    char *pmsg = pMsgBuff->message;

    if(!(pchoice=GET_CHOICE(choiceCvt,choice_value))) {
	sprintf(pmsg,"%4s Illegal Choice",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
    }
    else {
	sprintf(pmsg,"%4s: %s",pfield_name,pchoice);
	dbpr_msgOut(pMsgBuff,tab_size);
    }
    return(0);
}

int dbprReportGblChoice(pMsgBuff,precord,pfield_name,choice_value, tab_size)
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
    strcat(name,".");
    strncat(name,pfield_name,FLDNAME_SZ);
    if(dbNameToAddr(name,&dbAddr)) {
	sprintf(pmsg,"%4s dbNameToAddr failed?",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
	return(0);
    }
    if( !(pchoiceSet=GET_PCHOICE_SET(choiceGbl,dbAddr.choice_set))
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

int dbprReportRecChoice(pMsgBuff,precord,pfield_name,choice_value,tab_size)
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
    strcat(name,".");
    strncat(name,pfield_name,FLDNAME_SZ);
    if(dbNameToAddr(name,&dbAddr)) {
	sprintf(pmsg,"%4s: dbNameToAddr failed?",pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
	return(0);
    }
    if( !(parrChoiceSet=GET_PARR_CHOICE_SET(choiceRec,dbAddr.record_type)) 
    ||  !(pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,dbAddr.choice_set))
    ||  !(pchoice=GET_CHOICE(pchoiceSet,choice_value))) {
	sprintf(pmsg,"%4s: Cant find Choice",pfield_name);
    }
    else {
	sprintf(pmsg,"%4s: %s",pfield_name,pchoice);
    }
	dbpr_msgOut(pMsgBuff,tab_size);
    return(0);
}

int dbprReportDevChoice(pMsgBuff,paddr,pfield_name, tab_size)
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
        return(S_db_onlyOne);
    }
    if((!(pdevChoiceSet=GET_PDEV_CHOICE_SET(choiceDev,paddr->record_type)))
    || (!(pdevChoice=GET_DEV_CHOICE(pdevChoiceSet,choice_ind))) ) {
	sprintf(pmsg, "%4s: NULL",
            pfield_name);
	dbpr_msgOut(pMsgBuff,tab_size);
        return(S_db_badChoice);
    }
    strncpy(buff,pdevChoice->pchoice,MAX_STRING_SIZE);
	sprintf(pmsg,"%4s: %s",
            pfield_name,
            buff);
	dbpr_msgOut(pMsgBuff,tab_size);
    return(0);
}
/*################NEW ABOVE rz2*/

/*###############NEW BELOW rz1*/
/**/
static struct fldDes *
dbprGetFldRec(type, fldNum)
    short           type;	/* record type		 */
    short           fldNum;	/* field number		 */
{
    struct fldDes  *pfield;
    struct recTypDes *precTypDes;
    /* verify that pvd and recDes are available	 */
    if (!dbPvd || !dbRecDes)
	return (NULL);
    /* verify that type is valid			 */
    if ((type < 0) || (type >= dbRecDes->number))
	return (NULL);
    /* get and check pointer to this record's field definitions */
    precTypDes = dbRecDes->papRecTypDes[type];
    if (!precTypDes)
	return (NULL);
    pfield = precTypDes->papFldDes[fldNum];
    return (pfield);
}
static struct recTypDes *
dbprGetRecTypDes(type)
    short           type;	/* record type		 */
{
    struct recTypDes *precTypDes;
    /* verify that pvd and recDes are available	 */
    if (!dbPvd || !dbRecDes)
	return (NULL);
    /* verify that type is valid			 */
    if ((type < 0) || (type >= dbRecDes->number))
	return (NULL);
    /* get and check pointer to this record's field definitions */
    precTypDes = dbRecDes->papRecTypDes[type];
    if (!precTypDes)
	return (NULL);
    return (precTypDes);
}
/*################NEW ABOVE rz2*/
/*#############JUNK*************/
