
/* dbAccess.c */
/* share/src/db $Id$ */


/*dbAccess.c 
 *
 *	This is the former IOCDBACCESS.C
 *
 *	database access subroutines
 *
 *	Author:	Bob Dalesio(LANL) and Marty Kraimer(ANL)
 *	Date:	11/7/90
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

/* Global Database Access Routines
 *
 * dbScanLock(precord)		Lock for scanning records
 *	caddr_t		precord;
 * returns void
 *
 * dbScanUnlock(precord)	Unlock for scanning records
 *	caddr_t		precord;
 * returns void
 *
 * dbScanLockInit(nset)		Initialize scan lock
 *	int		nset;
 *
 * dbScanPassive(paddr)	process if record is passively scanned
 *	struct dbAddr *paddr;	pointer to database address structure
 *
 * dbProcess(paddr)		process a database record
 *	struct dbAddr *paddr;	pointer to database address structure
 *
 * dbNameToAddr(pname,paddr) Given "pv<.field>" compute dbAddr
 *	char		*pname
 *	struct dbAddr	*paddr;	pointer to database address structure
 *
 * dbGetLink(pdblink,pdest,dbrType,pbuffer,options,nRequest)
 *	struct db_link	*pdblink;
 *      struct dbCommon *pdest;
 *	short		dbrType;	DBR_xxx
 *	caddr_t		pbuffer;	addr of returned data
 *	long		*options;	addr of options
 *	long		*nRequest;	addr of number of elements
 *
 * dbPutLink(pdblink,psource,dbrType,pbuffer,nRequest)
 *	struct db_link	*pdblink;
 *      struct dbCommon *psource;
 *	short		dbrType;	DBR_xxx
 *	caddr_t		pbuffer;	addr of input data
 *	long		nRequest;
 *
 * dbGetField(paddr,dbrType,pbuffer,options,nRequest)
 *	struct dbAddr	*paddr;	
 *	short		dbrType;	DBR_xxx
 *	caddr_t		pbuffer;	addr of returned data
 *	long		*options;	addr of options
 *	long		*nRequest;	addr of number of elements
 *
 * dbPutField(paddr,dbrType,pbuffer,nRequest)
 *	struct dbAddr	*paddr;	
 *	short		dbrType;	DBR_xxx
 *	caddr_t		pbuffer;	addr of input data
 *	long		nRequest;
 *
 * dbBufferSize(dbr_type,options,no_elements)
 *	short	dbr_type;
 *	long	options;
 *	long	no_elements;
 * returns: number of bytes as a long
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<memLib.h>
#include	<fioLib.h>
#include	<strLib.h>
#include	<taskLib.h>

#include	<fast_lock.h>
#include	<alarm.h>
#include	<choice.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbCommon.h>
#include	<dbFldTypes.h>
#include 	<dbRecDes.h>
#include 	<dbRecType.h>
#include	<dbRecords.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>

long jba_debug=0;

long dbPut();

#define MAX_LOCK 10

#define MIN(x,y)        ((x < y)?x:y)
#define MAX(x,y)        ((x > y)?x:y)

struct scanLock{
	FAST_LOCK	lock;
	caddr_t		precord;
	ULONG		start_time;
	int		task_id;
};
static struct {
	int		nset;		/* Number of sets */
	struct scanLock *pscanLock;	/*addr of array of struct scanLock */
} dbScanPvt;

void dbScanLock(precord)
	caddr_t	precord;
{
	struct scanLock *pscanLock;
	short		lset=((struct dbCommon *)precord)->lset - 1;

	if(lset<0 || lset>=dbScanPvt.nset) {
		errMessage(S_db_badLset,"Lock Set out of range");
		exit(1);
	}
	pscanLock = dbScanPvt.pscanLock + lset;
	FASTLOCK(&pscanLock->lock);
	pscanLock->start_time = tickGet();
	pscanLock->task_id = taskIdSelf();
	pscanLock->precord = precord;
	return;
}

void dbScanUnlock(precord)
	caddr_t	precord;
{
	struct scanLock *pscanLock;
	short		lset=((struct dbCommon *)precord)->lset - 1;

	if(lset<0 || lset>=dbScanPvt.nset) {
		errMessage(S_db_badLset,"Lock Set out of range");
		return;
	}
	pscanLock = dbScanPvt.pscanLock + lset;
	pscanLock->precord = NULL;
	FASTUNLOCK(&pscanLock->lock);
	return;
}

void dbScanLockInit(nset)
	int	nset;
{
	struct scanLock	*pscanLock;
	int i;

	dbScanPvt.nset = nset;
	pscanLock = (struct scanLock *)calloc((size_t)nset,
		(size_t)sizeof(struct scanLock));
	dbScanPvt.pscanLock = pscanLock;
	for (i=0; i<nset; i++, pscanLock++) {
	    FASTLOCKINIT(&pscanLock->lock);
	    pscanLock->precord=NULL;
	    pscanLock->start_time=0;
	    pscanLock->task_id=0;
	}
	return;
}

long dbScanPassive(paddr)
	struct dbAddr *paddr;
{
	struct dbCommon *precord=(struct dbCommon *)(paddr->precord);
	
	/* if not passive just return success */
	if(precord->scan != 0) return(0);

	/* return result of process */
	return(dbProcess(paddr));
}

long dbProcess(paddr)
	struct dbAddr *paddr;
{
	struct rset	*prset;
	struct dbCommon *precord=(struct dbCommon *)(paddr->precord);
	long		status;
    
	if (jba_debug)
	printf ("-------------------------%s dbProcess entered -----------------\n",precord->name);

	/* If already active dont process */
	if(precord->pact) {
	        struct rset     *prset;
		struct valueDes valueDes;

	if (jba_debug)
	printf ("-------------------------%s already active -----------------\n",precord->name);

		/* raise scan alarm after MAX_LOCK times */
		if(precord->stat==SCAN_ALARM) return(0);
		if(precord->lcnt++ !=MAX_LOCK) return(0);
		precord->sevr = MAJOR_ALARM;
		precord->stat = SCAN_ALARM;
		precord->nsev = 0;
		precord->nsta = 0;
		/* anyone waiting for an event on this record?*/
		if(precord->mlis.count==0) return(0);
		db_post_events(precord,&precord->stat,DBE_VALUE);
		db_post_events(precord,&precord->sevr,DBE_VALUE);
	        prset=GET_PRSET(paddr->record_type);
		if( prset && prset->get_value ){
			(*prset->get_value)(precord,&valueDes);
			db_post_events(precord,valueDes.pvalue,DBE_VALUE|DBE_ALARM|DBE_LOG);
		}
		return(0);
	} else precord->lcnt=0;
		


	/* get the scan disable link if defined*/
	if(precord->sdis.type == DB_LINK) {
		long	options=0;
		long	nRequest=1;

		(status = dbGetLink(&precord->sdis.value.db_link,precord,
			DBR_SHORT,(caddr_t)(&(precord->disa)),&options,&nRequest));
		if(!RTN_SUCCESS(status)) {
			recGblDbaddrError(status,paddr,"dbProcess");
			return(status);
	    	}
	}
	/* if disabled just return success */
	if(precord->disa == precord->disv) {

	if (jba_debug)
	printf ("-------------------------%s is disabled -----------------\n",precord->name);

		return(0);
	}

	/* locate record processing routine */
	if(!(prset=GET_PRSET(paddr->record_type)) || !(prset->process)) {
		precord->pact=1;/*set pact TRUE so error is issued only once*/
		recGblRecSupError(S_db_noRSET,paddr,"dbProcess","process");
		return(S_db_noRSET);
	}

	/* process record */
	status = (*prset->process)(paddr);

	if (jba_debug)
	printf ("-------------------------%s processed ------------------------\n",precord->name);

	return(status);

}

/* forward reference for pvdGetFld */
struct fldDes *pvdGetFld();

long dbNameToAddr(pname,paddr)
char		*pname;
struct dbAddr	*paddr;
{
	char		*pbuffer;
	char		buffer[PVNAME_SZ+FLDNAME_SZ+2];
	short		field_offset;
	short		record_number;
	short		n;
	long		status=0;
	struct rset	*prset;
	struct recLoc	*precLoc;
	char*		precord;
	struct fldDes	*pfldDes;

	/* convert the record name */
	pbuffer = &buffer[0];
	n=0;
	while(*pname && (*pname != '.') && n<sizeof(buffer) ){
		*pbuffer = *pname;
		pname++;
		pbuffer++;
		n++;
	}
	*pbuffer = 0;
	if (pvdGetRec(&buffer[0],&(paddr->record_type),&record_number) < 0){
		(long)(paddr->precord) = -1;
		paddr->record_type = -1;
		return(S_db_notFound);
	}

	/* convert the field name */
	if (*pname) pname++;
	if (!(pfldDes=pvdGetFld(paddr->record_type,pname))){
		paddr->field_type = -1;
		(long)(paddr->pfield) = -1;
		paddr->field_size = -1;
		return(S_db_notFound);
	}
	paddr->pfldDes = (caddr_t)pfldDes;
	paddr->field_type = pfldDes->field_type;
	paddr->dbr_field_type = pfldDes->dbr_field_type;
	paddr->field_size = pfldDes->size;
	paddr->choice_set = pfldDes->choice_set;
	paddr->special = pfldDes->special;
	field_offset = pfldDes->offset;

	/* get the memory location of the record and field */
	if(!(precLoc=GET_PRECLOC(paddr->record_type))
	|| !(precord=GET_PRECORD(precLoc,record_number))
	|| (paddr->field_size+field_offset) > (precLoc->rec_size)) {
	    recGblDbaddrError(S_db_notFound,paddr,"dbNameToAddr");
	    return(S_db_notFound);
	}
	paddr->precord=precord;
	paddr->pfield=precord+field_offset;

	/*if special is SPC_DBADDR then call cvt_dbaddr		*/
	/*it may change pfield,no_elements,field_type,dbr_field_type,*/
	/*field_size,and special*/
	paddr->no_elements=1;
	if( ((paddr->special)==SPC_DBADDR)
	  && (prset=GET_PRSET(paddr->record_type))
	  && (prset->cvt_dbaddr) ) 
		status = (*prset->cvt_dbaddr)(paddr);

	return(status);
}

long dbGetLink(pdblink,pdest,dbrType,pbuffer,options,nRequest)
	struct db_link	*pdblink;
	struct dbCommon *pdest;
	short		dbrType;
	caddr_t		pbuffer;
	long		*options;
	long		*nRequest;
{
	struct dbAddr	*paddr=(struct dbAddr*)(pdblink->pdbAddr);
	long	status;

	if(pdblink->process_passive) {
		status=dbScanPassive(paddr);
		if(!RTN_SUCCESS(status)) return(status);
	}
	if(pdblink->maximize_sevr) {
		struct dbCommon *pfrom=(struct dbCommon*)(paddr->precord);

		if(pfrom->sevr>pdest->nsev) {
			pdest->nsev = pfrom->sevr;
			pdest->nsta = LINK_ALARM;
		}
	
	}
	status= dbGetField(paddr,dbrType,pbuffer,options,nRequest);
	if(status) recGblRecordError(status,pdest,"dbGetLink");
}

long dbPutLink(pdblink,psource,dbrType,pbuffer,nRequest)
	struct db_link	*pdblink;
	struct dbCommon *psource;
	short		dbrType;
	caddr_t		pbuffer;
	long		nRequest;
{
	struct dbAddr	*paddr=(struct dbAddr*)(pdblink->pdbAddr);
	long	status;

	status=dbPut(paddr,dbrType,pbuffer,nRequest);
	if(pdblink->maximize_sevr) {
		struct dbCommon *pto=(struct dbCommon*)(paddr->precord);

		if(pto->nsev<psource->sevr) {
			pto->nsev = psource->sevr;
			pto->nsta = LINK_ALARM;
		}
	}
	if(!RTN_SUCCESS(status)) return(status);
	if(pdblink->process_passive) status=dbScanPassive(paddr);
	if(status) recGblRecordError(status,psource,"dbPutLink");
	return(status);
}

long dbPutField(paddr,dbrType,pbuffer,nRequest)
	struct dbAddr	*paddr;
	short		dbrType;
	caddr_t		pbuffer;
	long		nRequest;
{
	long	status;
	struct fldDes *pfldDes=(struct fldDes *)(paddr->pfldDes);

	dbScanLock(paddr->precord);
	status=dbPut(paddr,dbrType,pbuffer,nRequest);
	if(status) recGblDbaddrError(status,paddr,"dbPutField");
	if(RTN_SUCCESS(status) && pfldDes->process_passive) status=dbScanPassive(paddr);
	dbScanUnlock(paddr->precord);
	return(status);
}

long dbBufferSize(dbr_type,options,no_elements)
    short	dbr_type;
    long	options;
    long	no_elements;
{
    long nbytes=0;

    nbytes += dbr_value_size[dbr_type] * no_elements;
    if(options & DBR_STATUS)	nbytes += dbr_status_size;
    if(options & DBR_UNITS)	nbytes += dbr_units_size;
    if(options & DBR_PRECISION) nbytes += dbr_precision_size;
    if(options & DBR_TIME)	nbytes += dbr_time_size;
    if(options & DBR_ENUM_STRS)	nbytes += dbr_enumStrs_size;
    if(options & DBR_GR_LONG)	nbytes += dbr_grLong_size;
    if(options & DBR_GR_DOUBLE)	nbytes += dbr_grDouble_size;
    if(options & DBR_CTRL_LONG) nbytes += dbr_ctrlLong_size;
    if(options & DBR_CTRL_DOUBLE)nbytes += dbr_ctrlDouble_size;
    if(options & DBR_AL_LONG)   nbytes += dbr_alLong_size;
    if(options & DBR_AL_DOUBLE) nbytes += dbr_alDouble_size;
    return(nbytes);
}

/*
 * F_TO_STR
 *
 * converts floating point numbers to NULL terminated strings
 * 
 * This routine was written to provide fast conversion.
 * Timing figures on a 68020
 * F_TO_STR         .316 msec
 * gcvt            2.6   msec
 * sprintf[%9.3E] 33     msecs
 *
 * This routine will call gcvt for any number with an absolute value
 * greater than or equal to 10,000,000.00
 *
 */
static void f_to_str(flt_value,pstr_value,precision)
double	flt_value;
char	*pstr_value;
int	precision;
{
	unsigned short	got_one;
	double		place;
	short		number;
	char	*pfirst_digit;

	if(flt_value>0e0 && flt_value<udfFtest) {
		strcpy(pstr_value,"undefined");
		return;
	}
	pfirst_digit = pstr_value;
	if (flt_value < 0){
		*pstr_value = '-';
		pstr_value++;
		pfirst_digit++;
		flt_value = -flt_value;
	}

	if (flt_value >= 10000000 ){
		gcvt(flt_value,20,pstr_value);
		return;
	}

	/* whole numbers */
	got_one = 0;
	for (place = 1000000; place >= 1; place /= 10){
		if (flt_value >= place){
			got_one = 1;
			number = flt_value / place;
			flt_value = flt_value - (number * place);
			*pstr_value = number + '0';
			pstr_value++;
		}else if (got_one){
			*pstr_value = '0';
			pstr_value++;
		}
	}
	if (!got_one){
		*pstr_value = '0';
		pstr_value++;
	}

	/* fraction */
	if (precision > 0){
		*pstr_value = '.';
		pstr_value++;
		for (place = .1; precision > 0; place /= 10, precision--){
			number = flt_value / place;
			flt_value = flt_value - (number * place);
			*pstr_value = number + '0';
			pstr_value++;
		}
	}
	*pstr_value = 0;

	/* rounding */
	if (flt_value >= (place * 5)){
		number = pstr_value - pfirst_digit;
		pstr_value--;
		while ((pstr_value >= pfirst_digit) && 
		  ((*pstr_value == '9') || (*pstr_value == '.'))){
			if (*pstr_value == '9')
				*pstr_value = '0';
			pstr_value--;
		}
		if (pstr_value < pfirst_digit){
			while (number >= 0){
				*(pfirst_digit + number + 1) = *(pfirst_digit + number);
				number--;
			}
			*pfirst_digit = '1';
		}else{
			*pstr_value += 1;
		}
	}
	return;
}

/* Convert various integer types to ascii */

static char digit_to_ascii[10]={'0','1','2','3','4','5','6','7','8','9'};

static void char_to_str(source,pdest)
    char 	source;
    char	*pdest;
{
    unsigned char val,temp;
    char	  digit[3];
    int		  i,j;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return;
    }
    if(source<0) {
	*pdest++ = '-';
	if(source == -128) {
	    strcpy(pdest,"128");
	    return;
	}
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static void uchar_to_str(source,pdest)
    unsigned char source;
    char	  *pdest;
{
    unsigned char val,temp;
    char	  digit[3];
    int		  i,j;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static void short_to_str(source,pdest)
    short source;
    char  *pdest;
{
    short val,temp;
    char  digit[6];
    int	  i,j;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return;
    }
    if(source==udfShort) {
	strcpy(pdest,"undefined");
	return;
    }
    if(source<0) {
	*pdest++ = '-';
	if(source == -32768) {
	    strcpy(pdest,"32768");
	    return;
	}
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static void ushort_to_str(source,pdest)
    unsigned short source;
    char	  *pdest;
{
    unsigned short val,temp;
    char	  digit[5];
    int		  i,j;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return;
    }
    if(source==udfUshort) {
	strcpy(pdest,"undefined");
	return;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static void long_to_str(source,pdest)
    long source;
    char  *pdest;
{
    long val,temp;
    char  digit[11];
    int	  i,j;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return;
    }
    if(source==udfLong) {
	strcpy(pdest,"undefined");
	return;
    }
    if(source<0) {
	*pdest++ = '-';
	if(source == -2147483648) {
	    strcpy(pdest,"2147483648");
	    return;
	}
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static void ulong_to_str(source,pdest)
    unsigned long source;
    char	  *pdest;
{
    unsigned long val,temp;
    char	  digit[10];
    int		  i,j;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return;
    }
    if(source==udfUlong) {
	strcpy(pdest,"undefined");
	return;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

/* DATABASE ACCESS GET CONVERSION SUPPORT */

static long getStringString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *psrc=paddr->pfield;
    short size=paddr->field_size;

    if(nRequest==1 && offset==0) {
	strncpy(pbuffer,psrc,size);
	return(0);
    }
    psrc+= (size*offset);
    while (nRequest) {
        strncpy(pbuffer,psrc,size);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=paddr->pfield;
	else
    		psrc  += size;
	nRequest--;
    }
    return(0);
}

static long getCharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	char_to_str(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	char_to_str(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(char *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getCharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getCharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	uchar_to_str(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	uchar_to_str(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned char *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUcharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUcharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	short_to_str(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	short_to_str(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(short *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getShortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
static long getShortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getShortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	ushort_to_str(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	ushort_to_str(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned short *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUshortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
static long getUshortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUshortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	long_to_str(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	long_to_str(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(long *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getLongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getLongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	ulong_to_str(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	ulong_to_str(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned long *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUlongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUlongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float	*psrc=(float *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;

    if((prset=GET_PRSET(record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"db_get_field","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	f_to_str((double)(*psrc),pbuffer,precision);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	f_to_str((double)(*psrc),pbuffer,precision);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(float *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getFloatChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);
    long  ltemp;	/*vxWorks does not support float to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *psrc;
	*pbuffer = ltemp;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	ltemp = *psrc++;
	*pbuffer++ = ltemp;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getFloatDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;

    if((prset=GET_PRSET(record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"db_get_field","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	f_to_str(*psrc,pbuffer,precision);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	f_to_str(*psrc,pbuffer,precision);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(double *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getDoubleChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);
    long   ltemp; /*vxWorks does not support double to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *psrc;
	*pbuffer = ltemp;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	ltemp = *psrc++;
	*pbuffer++ = ltemp;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getDoubleDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    struct rset	*prset;
    short	record_type=(paddr->record_type);
    long	status;

    if((prset=GET_PRSET(record_type)) && (prset->get_enum_str))
        return( (*prset->get_enum_str)(paddr,pbuffer) );
    status=S_db_noRSET;
    recGblRecSupError(status,paddr,"db_get_field","get_enum_str");
    return(S_db_badDbrtype);
}

static long getEnumChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getEnumDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getGchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short		choice_set=paddr->choice_set;
    short		choice_ind= *((short*)paddr->pfield);
    char		*pchoice;
    struct choiceSet	*pchoiceSet;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getGchoiceString)");
        return(S_db_onlyOne);
    }
    if((!(pchoiceSet=GET_PCHOICE_SET(choiceGbl,choice_set)))
    || (!(pchoice=GET_CHOICE(pchoiceSet,choice_ind))) ) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGetField(getGchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}

static long getGchoiceEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getGchoiceEnum)");
        return(S_db_onlyOne);
    }
    *pbuffer = *psrc;
    return(0);
}

static long getCchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short	choice_ind= *((short*)paddr->pfield);
    char	*pchoice;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getCchoiceString)");
        return(S_db_onlyOne);
    }
    if (!(pchoice=GET_CHOICE(choiceCvt,choice_ind))) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGetField(getCchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}

static long getCchoiceEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getCchoiceEnum)");
        return(S_db_onlyOne);
    }
    *pbuffer = *psrc;
    return(0);
}

static long getRchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short      		choice_set=paddr->choice_set;
    short      		choice_ind= *((short*)paddr->pfield);
    struct choiceSet	*pchoiceSet;
    struct arrChoiceSet *parrChoiceSet;
    char       		*pchoice;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getRchoiceString)");
        return(S_db_onlyOne);
    }
    if((!(parrChoiceSet=GET_PARR_CHOICE_SET(choiceRec,(paddr->record_type))))
    || (!(pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,choice_set)))
    || (!(pchoice=GET_CHOICE(pchoiceSet,choice_ind))) ) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGetField(getRchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}

static long getRchoiceEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getRchoiceEnum)");
        return(S_db_onlyOne);
    }
    *pbuffer = *psrc;
    return(0);
}

static long getDchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short      		choice_ind= *((short*)paddr->pfield);
    struct devChoiceSet *pdevChoiceSet;
    struct devChoice	*pdevChoice;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getDchoiceString)");
        return(S_db_onlyOne);
    }
    if((!(pdevChoiceSet=GET_PDEV_CHOICE_SET(choiceDev,paddr->record_type)))
    || (!(pdevChoice=GET_DEV_CHOICE(pdevChoiceSet,choice_ind))) ) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGetField(getRchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pdevChoice->pchoice,MAX_STRING_SIZE);
    return(0);
}

static long getDchoiceEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGetField(getDchoiceEnum)");
        return(S_db_onlyOne);
    }
    *pbuffer = *psrc;
    return(0);
}

/* This is the table of routines for converting database fields */
/* the rows represent the field type of the database field */
/* the columns represent the types of the buffer in which they are placed */

/* buffer types are********************************************************
 DBR_STRING,      DBR_CHR,         DBR_UCHAR,       DBR_SHORT,       DBR_USHORT,
 DBR_LONG,        DBR_ULONG,       DBR_FLOAT,       DBR_DOUBLE,      DBR_ENUM
 ***************************************************************************/

long (*get_convert_table[DBF_DEVCHOICE+1][DBR_ENUM+1])() = {

/* source is a DBF_STRING		*/
{getStringString, NULL,            NULL,            NULL,            NULL,
    NULL,         NULL,            NULL,            NULL,            NULL},
/* source is a DBF_CHAR 		*/
{getCharString,   getCharChar,     getCharUchar,    getCharShort,    getCharUshort,
 getCharLong,     getCharUlong,    getCharFloat,    getCharDouble,   getCharEnum},
/* source is a DBF_UCHAR		*/
{getUcharString,  getUcharChar,    getUcharUchar,   getUcharShort,   getUcharUshort,
 getUcharLong,    getUcharUlong,   getUcharFloat,   getUcharDouble,  getUcharEnum},
/* source is a DBF_SHORT		*/
{getShortString,  getShortChar,    getShortUchar,   getShortShort,   getShortUshort,
 getShortLong,    getShortUlong,   getShortFloat,   getShortDouble,  getShortEnum},
/* source is a DBF_USHORT		*/
{getUshortString, getUshortChar,   getUshortUchar,  getUshortShort,  getUshortUshort,
 getUshortLong,   getUshortUlong,  getUshortFloat,  getUshortDouble, getUshortEnum},
/* source is a DBF_LONG		*/
{getLongString,   getLongChar,     getLongUchar,    getLongShort,    getLongUshort,
 getLongLong,     getLongUlong,    getLongFloat,    getLongDouble,   getLongEnum},
/* source is a DBF_ULONG		*/
{getUlongString,  getUlongChar,    getUlongUchar,   getUlongShort,   getUlongUshort,
 getUlongLong,    getUlongUlong,   getUlongFloat,   getUlongDouble,  getUlongEnum},
/* source is a DBF_FLOAT		*/
{getFloatString,  getFloatChar,    getFloatUchar,   getFloatShort,   getFloatUshort,
 getFloatLong,    getFloatUlong,   getFloatFloat,   getFloatDouble,  getFloatEnum},
/* source is a DBF_DOUBLE		*/
{getDoubleString, getDoubleChar,   getDoubleUchar,  getDoubleShort,  getDoubleUshort,
 getDoubleLong,   getDoubleUlong,  getDoubleFloat,  getDoubleDouble, getDoubleEnum},
/* source is a DBF_ENUM		*/
{getEnumString,   getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_GBLCHOICE	*/
{getGchoiceString,NULL,            NULL,            NULL,            NULL,
 NULL,            NULL,            NULL,            NULL,            getGchoiceEnum},
/* source is a DBF_CVTCHOICE	*/
{getCchoiceString,NULL,            NULL,            NULL,            NULL,
 NULL,            NULL,            NULL,            NULL,            getCchoiceEnum},
/* source is a DBF_RECCHOICE	*/
{getRchoiceString,NULL,            NULL,            NULL,            NULL,
 NULL,            NULL,            NULL,            NULL,            getRchoiceEnum},
/* source is a DBF_DEVCHOICE	*/
{getDchoiceString,NULL,            NULL,            NULL,            NULL,
 NULL,            NULL,            NULL,            NULL,            getDchoiceEnum}
};


/* forward references for private routines used by dbGetField */
void get_enum_strs();
void get_graphics();
void get_control();
void get_alarm();

long dbGetField(paddr,dbrType,pbuffer,options,nRequest)
struct dbAddr	*paddr;
short		dbrType;
caddr_t		pbuffer;
long		*options;
long		*nRequest;
{
	long		no_elements=paddr->no_elements;
	long 		offset;
	struct rset	*prset;
	short		field_type=paddr->field_type;
	long		(*pconvert_routine)();
	struct dbCommon *pcommon;
	long		status;
	long		*perr_status=NULL;


	prset=GET_PRSET(paddr->record_type);

	if(!(*options)) goto GET_DATA;

	/* Process options */
	pcommon = (struct dbCommon *)(paddr->precord);
	if( (*options) & DBR_STATUS ) {
	    *((unsigned short *)pbuffer)++ = pcommon->stat;
	    *((unsigned short *)pbuffer)++ = pcommon->sevr;
	    perr_status=((long *)pbuffer)++;
	    *perr_status = 0;
	}
	if( (*options) & DBR_UNITS ) {
	    if( prset && prset->get_units ){ 
		(*prset->get_units)(paddr,pbuffer);
	    } else {
		bzero(pbuffer,dbr_units_size);
		*options = (*options) ^ DBR_UNITS; /*Turn off DBR_UNITS*/
	    }
	    pbuffer += dbr_units_size;
	}
	if( (*options) & DBR_PRECISION ) {
	    struct dbr_precision *pdbr_precision=( struct dbr_precision *)pbuffer;

	    if((field_type==DBF_FLOAT || field_type==DBF_DOUBLE) &&  prset && prset->get_precision ){ 
		(*prset->get_precision)(paddr,pbuffer);
		if(pdbr_precision->field_width<=0)
			pdbr_precision->field_width = pdbr_precision->precision + 5;
	    } else {
		bzero(pbuffer,dbr_precision_size);
		*options = (*options) ^ DBR_PRECISION; /*Turn off DBR_PRECISION*/
	    }
	    pbuffer += dbr_precision_size;
	}
	if( (*options) & DBR_TIME ) {
	    *((unsigned long *)pbuffer)++ = pcommon->time.secPastEpoch;
	    *((unsigned long *)pbuffer)++ = pcommon->time.nsec;
	}
	if( (*options) & DBR_ENUM_STRS ) get_enum_strs(paddr,&pbuffer,prset,options);
	if( (*options) & (DBR_GR_LONG|DBR_GR_DOUBLE ))
			get_graphics(paddr,&pbuffer,prset,options);
	if((*options) & (DBR_CTRL_LONG | DBR_CTRL_DOUBLE ))
			get_control(paddr,&pbuffer,prset,options);
	if((*options) & (DBR_AL_LONG | DBR_AL_DOUBLE ))
			get_alarm(paddr,&pbuffer,prset,options);


GET_DATA:

        if(*nRequest==0) return(0);
	/* Check for valid request */
	if( INVALID_DB_REQ(dbrType) || (field_type>DBF_DEVCHOICE) ){
		char message[80];

		sprintf(message,"dbGetField - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		if(perr_status) *perr_status = S_db_badDbrtype;
		return(S_db_badDbrtype);
	}
	
	/* check for array			*/
	if( no_elements>1 && prset && (prset->get_array_info) ) {
		status = (*prset->get_array_info)(paddr,&no_elements,&offset);
	}
	else offset=0;
	if(no_elements<(*nRequest)) *nRequest = no_elements;
	if(!(pconvert_routine=get_convert_table[field_type][dbrType])) {
		char message[80];

		sprintf(message,"dbGetField - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		if(perr_status) *perr_status = S_db_badDbrtype;
		return(S_db_badDbrtype);
	}
	/* convert database field to buffer type and place it in the buffer */
	status=(*pconvert_routine)(paddr,pbuffer,*nRequest,no_elements,offset);
	if(perr_status) *perr_status = status;
        return(status);
}

static void get_enum_strs(paddr,ppbuffer,prset,options)
struct dbAddr	*paddr;
char		**ppbuffer;
struct rset	*prset;
long		*options;
{
	short			field_type=paddr->field_type;
	struct choiceSet	*pchoiceSet;
	struct arrChoiceSet	*parrChoiceSet;
	struct devChoice	*pdevChoice;
	struct devChoiceSet	*pdevChoiceSet;
	unsigned long		no_str;
	char			*ptemp;
	struct dbr_enumStrs	*pdbr_enumStrs=(struct dbr_enumStrs*)(*ppbuffer);
	int			i;

	bzero(pdbr_enumStrs,dbr_enumStrs_size);
	switch(field_type) {
		case DBF_ENUM:
		    if( prset && prset->get_enum_strs ) {
			(*prset->get_enum_strs)(paddr,pdbr_enumStrs);
		    } else {
			*options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
		    }
		    break;
		case DBF_GBLCHOICE:
		    pchoiceSet=GET_PCHOICE_SET(choiceGbl,paddr->choice_set);
		    goto choice_common;
		case DBF_CVTCHOICE:
		    pchoiceSet=choiceCvt;
		    goto choice_common;
		case DBF_RECCHOICE:
		    parrChoiceSet=GET_PARR_CHOICE_SET(choiceRec,
			paddr->record_type);
		    pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,paddr->choice_set);
choice_common:
		    if(pchoiceSet==NULL) {
			*options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
			break;
		    }
		    i = sizeof(pdbr_enumStrs->strs)/sizeof(pdbr_enumStrs->strs[0]);
		    no_str=MIN(pchoiceSet->number,i);
		    pdbr_enumStrs->no_str = no_str;
		    ptemp = &(pdbr_enumStrs->strs[0][0]);
		    for (i=0; i<no_str; i++) {
			if(pchoiceSet->papChoice[i]==NULL)
			    *ptemp=0;
			else
			    strncpy(ptemp,pchoiceSet->papChoice[i],26);
			ptemp += 26;
		    }
		    break;
		case DBF_DEVCHOICE:
		    pdevChoiceSet=GET_PDEV_CHOICE_SET(choiceDev,
			paddr->record_type);
		    if(pdevChoiceSet==NULL) {
			*options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
			break;
		    }
		    i = sizeof(pdbr_enumStrs->strs)/sizeof(pdbr_enumStrs->strs[0]);
		    no_str=MIN(pdevChoiceSet->number,i);
		    pdbr_enumStrs->no_str = no_str;
		    ptemp = &(pdbr_enumStrs->strs[0][0]);
		    for (i=0; i<no_str; i++) {
			pdevChoice=GET_DEV_CHOICE(pdevChoiceSet,i);
			if(pdevChoice==NULL || pdevChoice->pchoice==NULL)
			    *ptemp=0;
			else
			    strncpy(ptemp,pdevChoice->pchoice,26);
			ptemp += 26;
		    }
		    break;
		default:
		    *options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
		    break;
	}
	*ppbuffer += dbr_enumStrs_size;
	return;
}

static void get_graphics(paddr,ppbuffer,prset,options)
struct dbAddr	*paddr;
char		**ppbuffer;
struct rset	*prset;
long		*options;
{
	short			field_type=paddr->field_type;
	struct			dbr_grDouble grd;
	int			got_data=FALSE;
	long	ltemp;/*vxWorks does not support double to unsigned long*/

	if( prset && prset->get_graphic_double ) {
		(*prset->get_graphic_double)(paddr,&grd);
		got_data=TRUE;
	}
	if( (*options) & (DBR_GR_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_grLong *pgr=(struct dbr_grLong*)pbuffer;
		    pgr->upper_disp_limit = grd.upper_disp_limit;
		    pgr->lower_disp_limit = grd.lower_disp_limit;
		} else {
		    bzero(pbuffer,dbr_grLong_size);
		    *options = (*options) ^ DBR_GR_LONG; /*Turn off option*/
		}
		*ppbuffer += dbr_grLong_size;
	}
	if( (*options) & (DBR_GR_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_grDouble *pgr=(struct dbr_grDouble*)pbuffer;
		    pgr->upper_disp_limit = grd.upper_disp_limit;
		    pgr->lower_disp_limit = grd.lower_disp_limit;
		} else {
		    bzero(pbuffer,dbr_grDouble_size);
		    *options = (*options) ^ DBR_GR_DOUBLE; /*Turn off option*/
		}
		*ppbuffer += dbr_grDouble_size;
	}
	return;
}

static void get_control(paddr,ppbuffer,prset,options)
struct dbAddr	*paddr;
char		**ppbuffer;
struct rset	*prset;
long		*options;
{
	short			field_type=paddr->field_type;
	struct dbr_ctrlDouble	ctrld;
	int			got_data=FALSE;
	long	ltemp;/*vxWorks does not support double to unsigned long*/

	if( prset && prset->get_control_double ) {
		(*prset->get_control_double)(paddr,&ctrld);
		got_data=TRUE;
	}
	if( (*options) & (DBR_CTRL_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_ctrlLong *pctrl=(struct dbr_ctrlLong*)pbuffer;
		    pctrl->upper_ctrl_limit = ctrld.upper_ctrl_limit;
		    pctrl->lower_ctrl_limit = ctrld.lower_ctrl_limit;
		} else {
		    bzero(pbuffer,dbr_ctrlLong_size);
		    *options = (*options) ^ DBR_CTRL_LONG; /*Turn off option*/
		}
		*ppbuffer += dbr_ctrlLong_size;
	}
	if( (*options) & (DBR_CTRL_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		   struct dbr_ctrlDouble *pctrl=(struct dbr_ctrlDouble*)pbuffer;
		   pctrl->upper_ctrl_limit = ctrld.upper_ctrl_limit;
		   pctrl->lower_ctrl_limit = ctrld.lower_ctrl_limit;
		} else {
		    bzero(pbuffer,dbr_ctrlDouble_size);
		    *options = (*options) ^ DBR_CTRL_DOUBLE; /*Turn off option*/
		}
		*ppbuffer += dbr_ctrlDouble_size;
	}
	return;
}

static void get_alarm(paddr,ppbuffer,prset,options)
struct dbAddr	*paddr;
char		**ppbuffer;
struct rset	*prset;
long		*options;
{
	short			field_type=paddr->field_type;
	struct			dbr_alDouble ald;
	int			got_data=FALSE;
	long	ltemp;/*vxWorks does not support double to unsigned long*/

	if( prset && prset->get_alarm_double ) {
		(*prset->get_alarm_double)(paddr,&ald);
		got_data=TRUE;
	}
	if( (*options) & (DBR_GR_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_alLong *pal=(struct dbr_alLong*)pbuffer;
		    pal->upper_alarm_limit = ald.upper_alarm_limit;
		    pal->upper_warning_limit = ald.upper_warning_limit;
		    pal->lower_warning_limit = ald.lower_warning_limit;
		    pal->lower_alarm_limit = ald.lower_alarm_limit;
		} else {
		    bzero(pbuffer,dbr_alLong_size);
		    *options = (*options) ^ DBR_AL_LONG; /*Turn off option*/
		}
		*ppbuffer += dbr_alLong_size;
	}
	if( (*options) & (DBR_GR_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_alDouble *pal=(struct dbr_alDouble*)pbuffer;
		    pal->upper_alarm_limit = ald.upper_alarm_limit;
		    pal->upper_warning_limit = ald.upper_warning_limit;
		    pal->lower_warning_limit = ald.lower_warning_limit;
		    pal->lower_alarm_limit = ald.lower_alarm_limit;
		} else {
		    bzero(pbuffer,dbr_alDouble_size);
		    *options = (*options) ^ DBR_AL_DOUBLE; /*Turn off option*/
		}
		*ppbuffer += dbr_alDouble_size;
	}
	return;
}

/* DATABASE ACCESS PUT CONVERSION SUPPORT */

static long putStringString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=paddr->pfield;
    short size=paddr->field_size;

    if(nRequest==1 && offset==0) {
	strncpy(pdest,pbuffer,size);
	return(0);
    }
    pdest+= (size*offset);
    while (nRequest) {
        strncpy(pdest,pbuffer,size);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest  += size;
	nRequest--;
    }
    return(0);
}

static long putStringChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)paddr->pfield;
    short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
		*pdest = (char)value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)paddr->pfield;
    unsigned long  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
		*pdest = (unsigned char)value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)paddr->pfield;
    short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)paddr->pfield;
    unsigned short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)paddr->pfield;
    long  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%ld",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)paddr->pfield;
    unsigned long  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%lu",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)paddr->pfield;
    float  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%f",&value) == 1) {
		*pdest = value;
		return(0);
	}
    }
    return(-1);
}

static long putStringDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)paddr->pfield;
    double  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%lf",&value) == 1) {
		*pdest = value;
		return(0);
	}
    }
    return(-1);
}

static long putStringEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    struct rset *prset;
    short       record_type=(paddr->record_type);
    long        status;

    if((prset=GET_PRSET(record_type)) && (prset->put_enum_str))
        return( (*prset->put_enum_str)(paddr,pbuffer) );
    status=S_db_noRSET;
    recGblRecSupError(status,paddr,"dbPutField","put_enum_str");
    return(S_db_badDbrtype);
}

static long putStringGchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short               choice_set=paddr->choice_set;
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    char                *pchoice;
    struct choiceSet    *pchoiceSet;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringGchoice)");
        return(S_db_onlyOne);
    }
    if(pchoiceSet=GET_PCHOICE_SET(choiceGbl,choice_set)) {
	for(i=0; i<pchoiceSet->number; i++) {
	    if(!(pchoice=pchoiceSet->papChoice[i])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringGchoice)");
    return(S_db_badChoice);
}

static long putStringCchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    char                *pchoice;
    struct choiceSet    *pchoiceSet;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringCchoice)");
        return(S_db_onlyOne);
    }
    if(pchoiceSet=choiceCvt) {
	for(i=0; i<pchoiceSet->number; i++) {
	    if(!(pchoice=pchoiceSet->papChoice[i])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringCchoice)");
    return(S_db_badChoice);
}

static long putStringRchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short               choice_set=paddr->choice_set;
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    char                *pchoice;
    struct choiceSet    *pchoiceSet;
    struct arrChoiceSet *parrChoiceSet;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringRchoice)");
        return(S_db_onlyOne);
    }
    if((parrChoiceSet=GET_PARR_CHOICE_SET(choiceRec,(paddr->record_type)))
    && (pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,choice_set))) {
	for(i=0; i<pchoiceSet->number; i++) {
	    if(!(pchoice=pchoiceSet->papChoice[i])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringRchoice)");
    return(S_db_badChoice);
}

static long putStringDchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    struct devChoiceSet *pdevChoiceSet;
    char		*pchoice;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringDchoice)");
        return(S_db_onlyOne);
    }
    if(pdevChoiceSet=GET_PDEV_CHOICE_SET(choiceDev,paddr->record_type)) {
	for(i=0; i<pdevChoiceSet->number; i++) {
	    if(!(pchoice=pdevChoiceSet->papDevChoice[i]->pchoice)) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringDchoice)");
    return(S_db_badChoice);
}

static long putCharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	char_to_str(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	char_to_str(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putCharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putCharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char 		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	uchar_to_str(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	uchar_to_str(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUcharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUcharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	short_to_str(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	short_to_str(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putShortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putShortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	ushort_to_str(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	ushort_to_str(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUshortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUshortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	long_to_str(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	long_to_str(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putLongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putLongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	ulong_to_str(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	ulong_to_str(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUlongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUlongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char	*pdest=(char *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;
    short size=paddr->field_size;

    if((prset=GET_PRSET(record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"dbPutField","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	f_to_str((double)(*pbuffer),pdest,precision);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	f_to_str((double)(*pbuffer),pdest,precision);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putFloatChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char   *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char   *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);
    long	   ltemp;/*vxWorks does not support float to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *pbuffer;
	*pdest = ltemp;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	ltemp = *pbuffer++;
	*pdest++ = ltemp;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putFloatDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char	*pdest=(char *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;
    short size=paddr->field_size;

    if((prset=GET_PRSET(record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"dbPutField","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	f_to_str(*pbuffer,pdest,precision);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	f_to_str(*pbuffer,pdest,precision);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putDoubleChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char   *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char   *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);
    long	   ltemp;/*vxWorks does not support double to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *pbuffer;
	*pdest = ltemp;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	ltemp = *pbuffer++;
	*pdest++ = ltemp;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putDoubleDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	uchar_to_str(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	uchar_to_str(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putEnumChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putEnumDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

/* This is the table of routines for converting database fields */
/* the rows represent the buffer types				*/
/* the columns represent the field types			*/

/* field types are********************************************************
 DBF_STRING,      DBF_CHAR,        DBF_UCHAR,       DBF_SHORT,       DBF_USHORT,
 DBF_LONG,        DBF_ULONG,       DBF_FLOAT,       DBF_DOUBLE,      DBF_ENUM
 DBF_GBLCHOICE,   DBF_CVTCHOICE,   DBF_RECCHOICE,   DBF_DEVCHOICE
 ***************************************************************************/

long (*put_convert_table[DBR_ENUM+1][DBF_DEVCHOICE+1])() = {
/* source is a DBR_STRING		*/
{putStringString, putStringChar,   putStringUchar,  putStringShort,  putStringUshort,
 putStringLong,   putStringUlong,  putStringFloat,  putStringDouble, putStringEnum,
 putStringGchoice,putStringCchoice,putStringRchoice,putStringDchoice},
/* source is a DBR_CHAR		*/
{putCharString,   putCharChar,     putCharUchar,    putCharShort,    putCharUshort,
 putCharLong,     putCharUlong,    putCharFloat,    putCharDouble,   putCharEnum,
 putCharEnum,     putCharEnum,     putCharEnum,     putCharEnum},
/* source is a DBR_UCHAR		*/
{putUcharString,  putUcharChar,    putUcharUchar,   putUcharShort,   putUcharUshort,
 putUcharLong,    putUcharUlong,   putUcharFloat,   putUcharDouble,  putUcharEnum,
 putUcharEnum,    putUcharEnum,    putUcharEnum,    putUcharEnum},
/* source is a DBR_SHORT		*/
{putShortString,  putShortChar,    putShortUchar,   putShortShort,   putShortUshort,
 putShortLong,    putShortUlong,   putShortFloat,   putShortDouble,  putShortEnum,
 putShortEnum,    putShortEnum,    putShortEnum,    putShortEnum},
/* source is a DBR_USHORT		*/
{putUshortString, putUshortChar,   putUshortUchar,  putUshortShort,  putUshortUshort,
 putUshortLong,   putUshortUlong,  putUshortFloat,  putUshortDouble, putUshortEnum,
 putUshortEnum,   putUshortEnum,   putUshortEnum,   putUshortEnum},
/* source is a DBR_LONG		*/
{putLongString,   putLongChar,     putLongUchar,    putLongShort,    putLongUshort,
 putLongLong,     putLongUlong,    putLongFloat,    putLongDouble,   putLongEnum,
 putLongEnum,     putLongEnum,     putLongEnum,     putLongEnum},
/* source is a DBR_ULONG		*/
{putUlongString,  putUlongChar,    putUlongUchar,   putUlongShort,   putUlongUshort,
 putUlongLong,    putUlongUlong,   putUlongFloat,   putUlongDouble,  putUlongEnum,
 putUlongEnum,    putUlongEnum,    putUlongEnum,    putUlongEnum},
/* source is a DBR_FLOAT		*/
{putFloatString,  putFloatChar,    putFloatUchar,   putFloatShort,   putFloatUshort,
 putFloatLong,    putFloatUlong,   putFloatFloat,   putFloatDouble,  putFloatEnum,
 putFloatEnum,    putFloatEnum,    putFloatEnum,    putFloatEnum},
/* source is a DBR_DOUBLE		*/
{putDoubleString, putDoubleChar,   putDoubleUchar,  putDoubleShort,  putDoubleUshort,
 putDoubleLong,   putDoubleUlong,  putDoubleFloat,  putDoubleDouble, putDoubleEnum,
 putDoubleEnum,   putDoubleEnum,   putDoubleEnum,   putDoubleEnum},
/* source is a DBR_ENUM		*/
{putEnumString,   putEnumChar,     putEnumUchar,    putEnumShort,    putEnumUshort,
 putEnumLong,     putEnumUlong,    putEnumFloat,    putEnumDouble,   putEnumEnum,
 putEnumEnum,     putEnumEnum,     putEnumEnum,     putEnumEnum}
};

static long dbPut(paddr,dbrType,pbuffer,nRequest)
struct dbAddr	*paddr;
short		dbrType;
caddr_t		pbuffer;
long		nRequest;
{
	long		no_elements=paddr->no_elements;
	long		dummy;
	long		offset;
	long		(*pconvert_routine)();
	long int	(*pspecial)()=NULL;
	struct rset	*prset;
	struct dbCommon *precord=(struct dbCommon *)(paddr->precord);
	long		status=0;
	static   char	val[4]={'V','A','L',' '};
	long		*pval=(long *)&val[0];
	struct fldDes	*pfldDes;
	long		*pfield_name;
	long		special=paddr->special;
	short		field_type=paddr->field_type;

	/* Check for valid request */
	if( INVALID_DB_REQ(dbrType) || (field_type>DBF_DEVCHOICE)
	|| (!(pconvert_routine=put_convert_table[dbrType][field_type])) )
	{
		char message[80];

		sprintf(message,"dbPut - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		return(S_db_badDbrtype);
	}
	
	prset=GET_PRSET(paddr->record_type);
	
	/* check for special processing	is required */
	if(special) {
	    if(special<100) { /*global processing*/
		if(special==SPC_NOMOD) return(S_db_noMod);
		if(special==SPC_SCAN) delete_from_scan_list(paddr);
	    }
	    else {
		if( prset && (pspecial = (prset->special))) {
		    status=(*pspecial)(paddr,0);
		    if(!RTN_SUCCESS(status)) return(status);
		} else {
		    recGblRecSupError(S_db_noSupport,paddr,"dbPut",
			"special");
		    return(S_db_noSupport);
		}
	    }
	}

	/* check for array			*/
	if( no_elements>1 && prset && (prset->get_array_info) ) {
		status= (*prset->get_array_info)(paddr,&dummy,&offset);
	}
	else offset=0;
	if(no_elements<(nRequest)) nRequest = no_elements;

	/* convert database field to buffer type and place it in the buffer */
	status=(*pconvert_routine)(paddr,pbuffer,nRequest,no_elements,offset);

	/* update array info	*/
	if( no_elements>1 && prset && (prset->put_array_info) ) {
		status= (*prset->put_array_info)(paddr,nRequest);
	}

	if(!RTN_SUCCESS(status)) return(status);

	/* check for special processing	is required */
	if(special) {
	    if(special<=100) { /*global processing*/
		if(special==SPC_SCAN) add_to_scan_list(paddr,0xffff);
	    }
	    else {
		status=(*pspecial)(paddr,1);
		if(!RTN_SUCCESS(status)) return(status);
	    }
	}

	/* propagate events for this field */
	/* if the field is VAL and process_passive is true dont propagate*/
	pfldDes = (struct fldDes *)(paddr->pfldDes);
	pfield_name = (long *)&(pfldDes->fldname[0]);
	if(precord->mlis.count &&
	((*pval != *pfield_name) || (!pfldDes->process_passive)))
		db_post_events(precord,paddr->pfield,DBE_VALUE);

	return(status);
}
