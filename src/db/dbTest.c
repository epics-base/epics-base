
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

#define MIN(x,y)        ((x < y)?x:y)
#define MAX(x,y)        ((x > y)?x:y)

/* Local Routines */
void printDbAddr();
void printBuffer();

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

    status=dbNameToAddr(pname,&addr);
    printDbAddr(status,&addr);
    if(status) return(1);
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/addr.field_size));
    options=0;
    if(addr.dbr_field_type==DBR_ENUM) {
	status=dbGetField(&addr,DBR_STRING,pbuffer,&options,&no_elements);
	printBuffer(status,DBR_STRING,pbuffer,0L,0L,no_elements);
    }
    else {
	status=dbGetField(&addr,addr.dbr_field_type,pbuffer,&options,&no_elements);
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
    }
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

    status=dbNameToAddr(pname,&addr);
    printDbAddr(status,&addr);
    if(status)return;
    /* try all options first */
    req_options=0xffffffff;
    ret_options=req_options;
    no_elements=0;
    status=dbGetField(&addr,addr.dbr_field_type,pbuffer,
	&ret_options,&no_elements);
    printBuffer(status,addr.dbr_field_type,pbuffer,
	req_options,ret_options,no_elements);
    /* Now try all request types */
    ret_options=0;
    dbr_type=DBR_STRING;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/MAX_STRING_SIZE));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_CHAR;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(char)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_UCHAR;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned char)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_SHORT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_USHORT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_LONG;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(long)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_ULONG;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned long)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_FLOAT;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(float)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_DOUBLE;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(double)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
    dbr_type=DBR_ENUM;
    no_elements=MIN(addr.no_elements,((sizeof(buffer)*4)/sizeof(unsigned short)));
    status=dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements);
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
	printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
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
	    printBuffer(status,addr.dbr_field_type,pbuffer,0L,0L,no_elements);
	}
    } else printf("sscanf failed for DBR_SHORT\n");
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
