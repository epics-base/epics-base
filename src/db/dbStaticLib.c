/*dbStaticLib.c*/
/* share/src/db $Id$ */
/*
 *
 *     Authors:	Marty Kraimer
 *		Bob Zieman: dbRead and dbWrite
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
 * .01	02-03-93	mrk	Consolidated all databse defs in one place
 */

#ifdef vxWorks
#include <vxWorks.h>
#include <taskLib.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include <errMdef.h>
#include <ellLib.h>
#include <dbDefs.h>
#include <sdrHeader.h>
#include <cvtFast.h>
#include <dbStaticLib.h>
#include <devSup.h>
#include <drvSup.h>
#include <cvtTable.h>
#include <choice.h>

#define messagesize	100

static char *ppstring[2]={"NPP","PP"};
static char *msstring[2]={"NMS","MS"};

static int mapDBFtoDCT[DBF_NOACCESS+1] = {
	DCT_STRING,
	DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,
	DCT_REAL,DCT_REAL,
	DCT_INTEGER,
	DCT_MENU,DCT_MENU,DCT_MENU,DCT_MENUFORM,
	DCT_INLINK,DCT_OUTLINK,DCT_FWDLINK,
	DCT_NOACCESS};

struct form {
	DBLINK		*plink;
	int		nlines;
	char		**prompt;
	char		**value;
	char		**verify;
};

static char *promptCONSTANT[] = {
	"Constant:"};
static char *promptPV_LINK[] = {
	"       Record Name:",
	"        Field Name:",
	"   Process Passive?",
	" Maximize Severity?"};
static char *promptVME_IO[] = {
	"  card:",
	"signal:"};
static char *promptCAMAC_IO[] = {
	" branch:",
	"  crate:",
	"   slot:",
	"channel:"};
static char *promptAB_IO[] = {
	"    link:",
	" adapter:",
	"    card:",
	"  signal:",
	"plc_flag:"};
static char *promptGPIB_IO[] = {
	"link:",
	"addr:",
	"parm:"};
static char *promptBITBUS_IO[] = {
	"  link:",
	"  node:",
	"  port:",
	"signal:",
	"  parm:"};
static char *promptINST_IO[] = {
	"parm:"};
static char *promptBBGPIB_IO[] = {
	"    link:",
	"  bbaddr:",
	"gpibaddr:",
	"    parm:"};
static char *promptVXI_IO[] = {
	"     Dynamic?",
	"DYN    frame:",
	"DYN    slot :",
	"STATIC   la :",
	"        parm:"};
static char **promptAddr[VXI_IO+1] = {
	promptCONSTANT,promptPV_LINK,promptVME_IO,
	promptCAMAC_IO,promptAB_IO,promptGPIB_IO,promptBITBUS_IO,
	NULL,NULL,promptINST_IO,promptBBGPIB_IO,
	promptVXI_IO};

static int formlines[VXI_IO+1] = {
	1,4,2,
	4,5,3,5,
	0,0,1,4,
	5};


/* internal routines*/
#ifdef __STDC__
static void entryErrMessage(DBENTRY *pdbentry,long status,char *mess)
{
#else
static void entryErrMessage(pdbentry,status,mess)
DBENTRY *pdbentry;
long status;
char *mess;
{
#endif /*__STDC__*/
    char		message[200];
    char		*pmessage=&message[0];
    DBBASE		*pdbbase = pdbentry->pdbbase;
    RECNODE		*precnode = pdbentry->precnode;
    struct fldDes 	*pflddes = pdbentry->pflddes;
    char		*pname;

    *pmessage=0;
    if(pname = GET_PRECNAME(pdbbase->precType,pdbentry->record_type)){
	strcat(pmessage,"RecordType:");
	strcat(pmessage,pname);
    }
    if(precnode){
	strcat(pmessage," Record:");
	strcat(pmessage,(char *)precnode->precord);
    }
    if(pflddes) {
	char *pstr=pflddes->fldname;

	strcat(pmessage," Field:");
	strcat(pmessage,pstr);
    }
    strcat(pmessage,"\n");
    strcat(pmessage,mess);
    errMessage(status,pmessage);
}

#ifdef __STDC__
static void zeroDbentry(DBENTRY *pdbentry)
{
#else
static void zeroDbentry(pdbentry)
DBENTRY *pdbentry;
{
#endif /*__STDC__*/
    /*NOTE that pdbbase, message, and formpvt MUST NOT be set to NULL*/
    pdbentry->precnode=NULL;
    pdbentry->pflddes=NULL;
    pdbentry->pfield=NULL;
    pdbentry->record_type=0;
    pdbentry->indfield=0;
}

#ifdef __STDC__
static char *getpMessage(DBENTRY *pdbentry)
#else
static char *getpMessage(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    if(!pdbentry->message) pdbentry->message = dbCalloc(1,messagesize);
    return(pdbentry->message);
}

static char hex_digit_to_ascii[16]={'0','1','2','3','4','5','6','7','8','9',
		'a','b','c','d','e','f'};

#ifdef __STDC__
static void ulongToHexString(unsigned long source,char *pdest)
#else
static void ulongToHexString(source,pdest)
unsigned long source;
char *pdest;
#endif /*__STDC__*/
{
    unsigned long  val,temp;
    char  digit[10];
    int	  i,j;

    if(source==0) {
	strcpy(pdest,"0x0");
	return;
    }
    *pdest++ = '0'; *pdest++ = 'x';
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/16;
	digit[i] = hex_digit_to_ascii[val - temp*16];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static double delta[2]={1e-6,1e-15};
static int precision[2]={6,14};
#ifdef __STDC__
static void realToString(double value,char *preturn,int isdouble)
#else
static void realToString(value,preturn,isdouble)
double value;
char *preturn;
int isdouble;
#endif /*__STDC__*/
{
    long	intval;
    double	diff,absvalue;
    int		logval,prec,end;
    char	tstr[30];
    char	*ptstr=&tstr[0];
    int		round;
    int		ise=FALSE;
    char	*loce=NULL;

    if(value==0.0) {strcpy(preturn,"0"); return;};
    intval=value;
    diff = value - intval;
    if(diff<0.0) diff =-diff;
    absvalue = (value<0.0? -value: value);
    if(diff < absvalue*delta[isdouble]) {
	cvtLongToString(intval,preturn);
	return;
    }
    /*Now starts the hard cases*/
    if(value<0.0) {*preturn++ = '-'; value = -value;}
    logval = (int)log10(value);
    if(logval>6 || logval<-2 ) {
	ise=TRUE;
	prec = precision[isdouble];
	sprintf(ptstr,"%.*e",prec,value);
	loce = strchr(ptstr,'e');
	if(!loce) {errMessage(-1,"logic error in real to string"); return;}
	*loce++ = 0;
    } else {
	prec = precision[isdouble]-logval;
	if(prec<0)prec=0;
	sprintf(ptstr,"%.*f",prec,value);
    }
    if(prec>0) {
	end = strlen(ptstr) -1;
	round=FALSE;
	while(TRUE) {
	    if(end<=0)break;
	    if(tstr[end]=='.'){end--; break;}
	    if(tstr[end]=='0'){end--; continue;}
	    if(!round && end<precision[isdouble]) break;
	    if(!round && tstr[end]<'8') break;
	    if(tstr[end-1]=='.') {
		if(round)end = end-2;
		break;
	    }
	    if(tstr[end-1]!='9') break;
	    round=TRUE;
	    end--;
	}
	tstr[end+1]=0;
	while (round) {
	    if(tstr[end]<'9') {tstr[end]++; break;}
	    if(end==0) { *preturn++='1'; tstr[end]='0'; break;}
	    tstr[end--]='0';
	}
    }
    strcpy(preturn,&tstr[0]);
    if(ise) {
	if(!(strchr(preturn,'.'))) strcat(preturn,".0");
	strcat(preturn,"e");
	strcat(preturn,loce);
    }
    return;
}

#ifdef __STDC__
static void floatToString(float value,char *preturn)
#else
static void floatToString(value,preturn)
float value;
char *preturn;
#endif /*__STDC__*/
{
    realToString((double)value,preturn,0);
    return;
}

#ifdef __STDC__
static void doubleToString(double value,char *preturn)
#else
static void doubleToString(value,preturn)
double value;
char *preturn;
#endif /*__STDC__*/
{
    realToString(value,preturn,1);
    return;
}

/* Beginning of Public Routines */
#ifdef __STDC__
void *dbCalloc(size_t nobj,size_t size)
#else
void *dbCalloc(nobj,size)
size_t nobj;
size_t size;
#endif /*__STDC__*/
{
    void *p;

    p=calloc(nobj,size);
    if(p) return(p);
#ifdef vxWorks
    taskSuspend(0);
#else
    abort();
#endif
    return(NULL);
}
#ifdef __STDC__
void *dbMalloc(size_t size)
#else
void *dbMalloc(size)
size_t size;
#endif /*__STDC__*/
{
    void *p;

    p=malloc(size);
    if(p) return(p);
#ifdef vxWorks
    taskSuspend(0);
#else
    abort();
#endif
    return(NULL);
}

#ifdef __STDC__
DBBASE *dbAllocBase(void)
#else
DBBASE *dbAllocBase()
#endif /*__STDC__*/
{
    DBBASE	*pdbbase;

    pdbbase = dbCalloc(1,sizeof(DBBASE));
    dbPvdInitPvt(pdbbase);
    return (pdbbase);
}

#ifdef __STDC__
void dbFreeBase(DBBASE *pdbbase)
#else
void dbFreeBase(pdbbase)
DBBASE *pdbbase;
#endif /*__STDC__*/
{
    ELLLIST          **ppvd = (ELLLIST **) pdbbase->ppvd;
    struct recType *precType = (struct recType *) pdbbase->precType;
    struct recLoc  *precLoc;
    struct recHeader *precHeader = (struct recHeader *) pdbbase->precHeader;
    RECNODE        *precnode;
    RECNODE        *pnext;
    ELLLIST           *preclist;
    int             recType;
    if (!pdbbase || !ppvd || !precType) return;
    dbPvdFreeMem(pdbbase);
    /* loop thru the recLocs - removing lists then recLoc only */
    for (recType = 0; recType < precType->number; recType++) {
	if (!(precLoc = GET_PRECLOC(precHeader, recType))) continue;
	preclist = precLoc->preclist;
	if (preclist == NULL) {
	    free((void *) precLoc);
	    continue;
	}
	precnode = (RECNODE *) ellFirst(preclist);
	while(precnode) {
	    pnext = (RECNODE *) ellNext((ELLNODE *) precnode);
	    free(precnode->precord);
	    ellDelete(preclist, (ELLNODE*)precnode);
	    dbPvdDelete(pdbbase,precnode);
	    free((void *)precnode);
	    precnode = pnext;
	}
	free((void *) preclist);
	free((void *) precLoc);
    }
    /* free the rest of the memory allocations */
    if (pdbbase->pchoiceCvt)
	free((void *) pdbbase->pchoiceCvt);
    if (pdbbase->pchoiceDev)
	free((void *) pdbbase->pchoiceDev);
    if (pdbbase->pchoiceGbl)
	free((void *) pdbbase->pchoiceGbl);
    if (pdbbase->pchoiceRec)
	free((void *) pdbbase->pchoiceRec);
    if (pdbbase->pcvtTable)
	free((void *) pdbbase->pcvtTable);
    if (pdbbase->pdbName)
	free((void *) pdbbase->pdbName);
    if (pdbbase->pdrvSup)
	free((void *) pdbbase->pdrvSup);
    if (pdbbase->ppvd)
	free((void *) pdbbase->ppvd);
    if (pdbbase->precDes)
	free((void *) pdbbase->precDes);
    if (pdbbase->precDevSup)
	free((void *) pdbbase->precDevSup);
    if (pdbbase->precHeader)
	free((void *) pdbbase->precHeader);
    if (pdbbase->precSup)
	free((void *) pdbbase->precSup);
    if (pdbbase->precType)
	free((void *) pdbbase->precType);
    if (pdbbase->psdrSum)
	free((void *) pdbbase->psdrSum);

    /* finally */
    free((void *) pdbbase);
    return;
}

#ifdef __STDC__
DBENTRY *dbAllocEntry(DBBASE *pdbbase)
#else
DBENTRY *dbAllocEntry(pdbbase)
DBBASE *pdbbase;
#endif /*__STDC__*/
{
    DBENTRY *pdbentry;

    pdbentry = dbCalloc(1,sizeof(DBENTRY));
    pdbentry->pdbbase = pdbbase;
    return(pdbentry);
}

#ifdef __STDC__
void dbFreeEntry(DBENTRY *pdbentry)
#else
void dbFreeEntry(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    if(pdbentry->message) free((void *)pdbentry->message);
    if(pdbentry->formpvt) dbFreeForm(pdbentry);
    free((void *)pdbentry);
}

#ifdef __STDC__
void dbInitEntry(DBBASE *pdbbase,DBENTRY *pdbentry)
#else
void dbInitEntry(pdbbase,pdbentry)
DBBASE *pdbbase;
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    memset((char *)pdbentry,'\0',sizeof(DBENTRY));
    pdbentry->pdbbase = pdbbase;
}

#ifdef __STDC__
void dbFinishEntry(DBENTRY *pdbentry)
#else
void dbFinishEntry(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    if(pdbentry->message) free((void *)pdbentry->message);
    if(pdbentry->formpvt) dbFreeForm(pdbentry);
}

#ifdef __STDC__
DBENTRY *dbCopyEntry(DBENTRY *pdbentry)
#else
DBENTRY *dbCopyEntry(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBENTRY *pnew;

    dbFinishEntry(pdbentry);
    pnew = dbAllocEntry(pdbentry->pdbbase);
    *pnew = *pdbentry;
    return(pnew);
}

#ifdef __STDC__
long dbFindRecdes(DBENTRY *pdbentry,char *rectype)
#else
long dbFindRecdes(pdbentry,rectype)
DBENTRY *pdbentry;
char *rectype;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase=pdbentry->pdbbase;
    struct recType	*precType = pdbbase->precType;
    int			i;

    zeroDbentry(pdbentry);
    for(i=1; i<precType->number; i++) {
	if(strcmp(precType->papName[i],rectype) == 0) {
	    pdbentry->record_type = i;
	    return(0);
	}
    }
    return(S_dbLib_recdesNotFound);
}

#ifdef __STDC__
long dbFirstRecdes(DBENTRY *pdbentry)
#else
long dbFirstRecdes(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{

    zeroDbentry(pdbentry);
    /* Note that first record type is NULL*/
    pdbentry->record_type = 1;
    return(0);
}

#ifdef __STDC__
long dbNextRecdes(DBENTRY *pdbentry)
#else
long dbNextRecdes(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase=pdbentry->pdbbase;
    short		record_type = pdbentry->record_type;
    struct recType	*precType = pdbbase->precType;

    zeroDbentry(pdbentry);
    if(++record_type >= precType->number) return(S_dbLib_recdesNotFound);
    pdbentry->record_type = record_type;
    return(0);
}

#ifdef __STDC__
char *dbGetRecdesName(DBENTRY *pdbentry)
#else
char *dbGetRecdesName(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE	*pdbbase=pdbentry->pdbbase;
    short	record_type = pdbentry->record_type;

    return(GET_PRECNAME(pdbbase->precType,record_type));
}

#ifdef __STDC__
int dbGetNRecdes(DBENTRY *pdbentry)
#else
int dbGetNRecdes(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase=pdbentry->pdbbase;
    struct recType	*precType = pdbbase->precType;

    return(precType->number - 1);
}

#ifdef __STDC__
long dbCopyRecdes(DBENTRY *from,DBENTRY *to)
#else
long dbCopyRecdes(from,to)
DBENTRY *from;
DBENTRY *to;
#endif /*__STDC__*/
{
    errMessage(-1,"dbCopyRecdes not implemented");
    return(-1);
}

#ifdef __STDC__
static void init_record(DBBASE *pdbbase, void *precord, short record_type)
#else
static void init_record(pdbbase, precord, record_type)
DBBASE *pdbbase;
void *precord;
short record_type;
#endif /*__STDC__*/
{
    struct recDes  	*precDes = pdbbase->precDes;
    struct recTypDes 	*precTypDes;
    struct fldDes  	*pflddes;
    int             	i;
    char           	*ptr;

    if ((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL) return;

    for (i = 0; i < precTypDes->no_fields; i++) {
	pflddes = GET_PFLDDES(precTypDes, i);
	ptr = (char *)precord + pflddes->offset;
	switch (pflddes->field_type) {
	case (DBF_CHAR):
	    *(char *) ptr = pflddes->initial.char_value;
	    break;
	case (DBF_UCHAR):
	    *(unsigned char *) ptr =
		pflddes->initial.uchar_value;
	    break;
	case (DBF_SHORT):
	    *(short *) ptr = pflddes->initial.short_value;
	    break;
	case (DBF_USHORT):
	    *(unsigned short *) ptr =
		pflddes->initial.ushort_value;
	    break;
	case (DBF_LONG):
	    *(long *) ptr = pflddes->initial.long_value;
	    break;
	case (DBF_ULONG):
	    *(unsigned long *) ptr =
		pflddes->initial.ulong_value;
	    break;
	case (DBF_FLOAT):
	    *(float *) ptr = pflddes->initial.float_value;
	    break;
	case (DBF_DOUBLE):
	    *(double *) ptr = pflddes->initial.double_value;
	    break;
	case (DBF_GBLCHOICE):
	case (DBF_CVTCHOICE):
	case (DBF_RECCHOICE):
	case (DBF_DEVCHOICE):
	case (DBF_ENUM):
	    *(unsigned short *) ptr =
		pflddes->initial.enum_value;
	    break;
	case (DBF_INLINK):
	case (DBF_OUTLINK):
	case (DBF_FWDLINK):
	    ((struct link *) ptr)->type = CONSTANT;
	    ((struct link *) ptr)->value.value = 0.0;
	    break;
	}
    }
    return;
}

#ifdef __STDC__
long dbCreateRecord(DBENTRY *pdbentry,char *precordName)
#else
long dbCreateRecord(pdbentry,precordName)
DBENTRY *pdbentry;
char *precordName;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase = pdbentry->pdbbase;
    int			record_type = pdbentry->record_type;
    PVDENTRY       	*ppvd;
    ELLLIST           	*preclist = NULL;
    RECNODE        	*precnode = NULL;
    RECNODE        	*pNewRecNode = NULL;
    void           	*pNewRecord = NULL;
    struct recDes  	*precDes = pdbbase->precDes;
    struct recTypDes 	*precTypDes = NULL;
    struct recHeader 	*precHeader = (struct recHeader *) pdbbase->precHeader;
    struct recLoc  	*precLoc = NULL;
    short           	rec_size;

    /* clear callers entry */
    zeroDbentry(pdbentry);
    if(!dbFindRecord(pdbentry,precordName)) return (S_dbLib_recExists);
    /* verify that record_type is valid			 */
    if(record_type < 1) return(S_dbLib_recdesNotFound);
    if ((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL)
	return(S_dbLib_recdesNotFound);
    precLoc = precHeader->papRecLoc[record_type];
    if ((precLoc->preclist) == NULL) {
	/* allocate new list for this record type */
	precLoc->preclist = dbCalloc(1,sizeof(ELLLIST));
	ellInit(precLoc->preclist);
    }
    preclist = precLoc->preclist;
    rec_size = precTypDes->rec_size;
    /* create a recNode */
    pNewRecNode = dbCalloc(1,sizeof(RECNODE));
    /* create a new record of this record type */
    pNewRecord = dbCalloc(1,rec_size);
    strcpy((char*)pNewRecord, precordName);
    /* give it the default values */
    init_record(pdbbase, pNewRecord, record_type);
    pNewRecNode->precord = pNewRecord;
    /* install record node in list in sorted postion */
    precnode = (RECNODE *)ellFirst(preclist);
    while(precnode && strcmp(precordName,(char*)precnode->precord) > 0)
	precnode = (RECNODE *)ellNext((ELLNODE*)precnode);
    if(precnode)
	ellInsert(preclist, ellPrevious((ELLNODE*)precnode) ,(ELLNODE *)pNewRecNode);
    else
	ellAdd(preclist, (ELLNODE*)pNewRecNode);
    ppvd = dbPvdAdd(pdbbase,record_type,pNewRecNode);
    if(!ppvd) {errMessage(-1,"Logic Err: Could not add to PVD");return(-1);}
    pdbentry->precnode = pNewRecNode;
    pdbentry->record_type = record_type;
    return (0);
}

#ifdef __STDC__
long dbDeleteRecord(DBENTRY *pdbentry)
#else
long dbDeleteRecord(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase = pdbentry->pdbbase;
    int			record_type = pdbentry->record_type;
    RECNODE		*precnode = pdbentry->precnode;
    ELLLIST           	*preclist;


    if (!precnode) return (-1);
    preclist = pdbbase->precHeader->papRecLoc[record_type]->preclist;
    free(precnode->precord);
    ellDelete(preclist, (ELLNODE*)precnode);
    dbPvdDelete(pdbbase,precnode);
    free((void *)precnode);
    pdbentry->precnode = NULL;
    return (0);
}

#ifdef __STDC__
long dbFindRecord(DBENTRY *pdbentry,char *precordName)
#else
long dbFindRecord(pdbentry,precordName)
DBENTRY *pdbentry;
char *precordName;
#endif /*__STDC__*/
{
    DBBASE	*pdbbase = pdbentry->pdbbase;
    int         lenName=0;
    PVDENTRY    *ppvdNode;
    char        convName[PVNAME_SZ + 1];
    char        *pconvName = &convName[0];

    
    zeroDbentry(pdbentry);
    /* convert the record name */
    while (*precordName && (*precordName != '.') && (lenName < PVNAME_SZ)) {
	*pconvName++ = *precordName++;
	lenName++;
    }
    *pconvName = 0;
    pconvName = &convName[0];
    ppvdNode = dbPvdFind(pdbbase,pconvName,lenName);
    if(!ppvdNode) return(S_dbLib_recNotFound);
    pdbentry->precnode = ppvdNode->precnode;
    pdbentry->record_type = ppvdNode->record_type;
    if(*precordName++=='.') return(dbFindField(pdbentry, precordName));
    return (0);
}

#ifdef __STDC__
long dbFirstRecord(DBENTRY *pdbentry)
#else
long dbFirstRecord(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    short		record_type = pdbentry->record_type;
    DBBASE		*pdbbase = pdbentry->pdbbase;
    struct recHeader	*precHeader = pdbbase->precHeader;
    struct recLoc	*precLoc;
    RECNODE		*precnode;

    zeroDbentry(pdbentry);
    if(record_type<1) return(S_dbLib_recdesNotFound);
    pdbentry->record_type = record_type;
    if(!(precLoc=GET_PRECLOC(precHeader,record_type))) return(S_dbLib_recNotFound);
    if(!precLoc->preclist) return(S_dbLib_recNotFound);
    precnode = (RECNODE *)ellFirst(precLoc->preclist);
    if(!precnode) return(S_dbLib_recNotFound);
    pdbentry->precnode = precnode;
    return(0);
}

#ifdef __STDC__
long dbNextRecord(DBENTRY *pdbentry)
#else
long dbNextRecord(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    RECNODE	*precnode=pdbentry->precnode;
    long	status=0;

    if(!precnode) return(S_dbLib_recNotFound);
    precnode = (RECNODE *)ellNext((ELLNODE *)precnode);
    if(!precnode) status = S_dbLib_recNotFound;
    pdbentry->precnode = precnode;
    pdbentry->pfield = NULL;
    return(status);
}

#ifdef __STDC__
int dbGetNRecords(DBENTRY *pdbentry)
#else
int dbGetNRecords(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    short		record_type = pdbentry->record_type;
    DBBASE		*pdbbase = pdbentry->pdbbase;
    struct recHeader	*precHeader = pdbbase->precHeader;
    struct recLoc	*precLoc;

    if(record_type<1) return(S_dbLib_recdesNotFound);
    if(!(precLoc=GET_PRECLOC(precHeader,record_type)))
	return(0);
    if(!precLoc->preclist) return(0);
    return(ellCount(precLoc->preclist));
}

#ifdef __STDC__
char *dbGetRecordName(DBENTRY *pdbentry)
#else
char *dbGetRecordName(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    RECNODE	*precnode=pdbentry->precnode;

    if(!precnode) return(NULL);
    return((char *)precnode->precord);
}

#ifdef __STDC__
long dbCopyRecord(DBENTRY *from,DBENTRY *to)
#else
long dbCopyRecord(from,to)
DBENTRY *from;
DBENTRY *to;
#endif /*__STDC__*/
{
    errMessage(-1,"dbCopyRecord not implemented");
    return(-1);
}

#ifdef __STDC__
long dbRenameRecord(DBENTRY *pdbentry,char *newName)
#else
long dbRenameRecord(pdbentry,newName)
DBENTRY *pdbentry;
char *newName;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase = pdbentry->pdbbase;
    short		record_type = pdbentry->record_type;
    RECNODE		*precnode = pdbentry->precnode;
    struct recHeader	*precHeader = (struct recHeader *) pdbbase->precHeader;
    char		*precord;
    PVDENTRY		*ppvd;
    ELLLIST		*preclist;
    RECNODE		*plistnode;

    if(!precnode) return(S_dbLib_recNotFound);
    precord = precnode->precord;
    dbPvdDelete(pdbbase,precnode);
    strcpy(precord,newName);
    ppvd = dbPvdAdd(pdbbase,record_type,precnode);
    if(!ppvd) {errMessage(-1,"Logic Err: Could not add to PVD");return(-1);}
    /*remove from record list and reinstall in sorted order*/
    preclist = precHeader->papRecLoc[record_type]->preclist;
    ellDelete(preclist, (ELLNODE*)precnode);
    plistnode = (RECNODE *)ellFirst(preclist);
    while(plistnode && strcmp(newName,(char*)plistnode->precord) < 0)
	plistnode = (RECNODE *)ellNext((ELLNODE*)plistnode);
    if(plistnode)
	ellInsert(preclist, ellPrevious((ELLNODE*)plistnode) ,(ELLNODE *)precnode);
    else
	ellAdd(preclist, (ELLNODE*)precnode);
    return(0);
}

#ifdef __STDC__
long dbFindField(DBENTRY *pdbentry,char *pfieldName)
#else
long dbFindField(pdbentry,pfieldName)
DBENTRY *pdbentry;
char *pfieldName;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    RECNODE		*precnode = pdbentry->precnode;
    char		*precord;
    struct recDes  	*precDes = pdbbase->precDes;
    short           	record_type = pdbentry->record_type;
    struct fldDes  	*pflddes;
    struct recTypDes 	*precTypDes;
    short           	i,top, bottom, test;
    char            	convName[FLDNAME_SZ];
    char           	*pconvName = &convName[0];
    unsigned long  	*sortFldName;
    short          	*sortFldInd;

    if(!precnode) return(S_dbLib_recNotFound);
    if (record_type < 1) return (S_dbLib_recdesNotFound);
    precord = precnode->precord;
    if((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL)
	return(S_dbLib_recdesNotFound);
    sortFldName = precTypDes->sortFldName;
    sortFldInd = precTypDes->sortFldInd;
    /* blank pad the field name */
    i = 0;
    while (*(pfieldName + i) && (i < FLDNAME_SZ)) {
	*(pconvName + i) = *(pfieldName + i);
	i++;
    }
    while (i < FLDNAME_SZ) {
	*(pconvName + i) = ' ';
	i++;
    }
    /* check for default field name to be supplied */
    if (*pconvName == ' ') {
	memcpy(pconvName, "VAL ", FLDNAME_SZ);
    }
    /* binary search through ordered field names */
    top = precTypDes->no_fields - 1;
    bottom = 0;
    test = (top + bottom) / 2;
    while (1) {
	/* check the field name */
	if (sortFldName[test] == *(unsigned long *) pconvName) {
	    if(!(pflddes=GET_PFLDDES(precTypDes,sortFldInd[test])))
		return(S_dbLib_recdesNotFound);
	    pdbentry->pflddes = pflddes;
	    pdbentry->pfield = precord + pflddes->offset;
	    pdbentry->indfield = sortFldInd[test];
	    return (0);
	} else if (sortFldName[test] > *(unsigned long *) pconvName) {
	    top = test - 1;
	    if (top < bottom) return (S_dbLib_fieldNotFound);
	    test = (top + bottom) / 2;
	} else {
	    bottom = test + 1;
	    if (top < bottom) return (S_dbLib_fieldNotFound);
	    test = (top + bottom) / 2;
	}
    }
}

#ifdef __STDC__
long dbFirstFielddes(DBENTRY *pdbentry,int dctonly)
#else
long dbFirstFielddes(pdbentry,dctonly)
DBENTRY *pdbentry;
int dctonly;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct recDes  	*precDes = pdbbase->precDes;
    RECNODE		*precnode = pdbentry->precnode;
    short           	record_type = pdbentry->record_type;
    struct fldDes  	*pflddes;
    struct recTypDes 	*precTypDes;
    short		indfield = 0;

    if (record_type < 1) return (S_dbLib_recdesNotFound);
    /* read the record type definitions for this record */
    if ((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL) {
	return (S_dbLib_recdesNotFound);
    }
    while(TRUE) {
	if(!(pflddes = GET_PFLDDES(precTypDes,indfield))) {
	    pdbentry->indfield = 0;
	    pdbentry->pflddes = NULL;
	    pdbentry->pfield = NULL;
	    return(S_dbLib_fieldNotFound);
	}
	if(!dctonly || pflddes->promptflag) {
	    pdbentry->indfield = indfield;
	    pdbentry->pflddes = pflddes;
	    if(precnode)
		pdbentry->pfield = ((char *)precnode->precord)+pflddes->offset;
	    else
		pdbentry->pfield = NULL;
	    return(0);
	}
	indfield++;
    }
}

#ifdef __STDC__
long dbNextFielddes(DBENTRY *pdbentry,int dctonly)
#else
long dbNextFielddes(pdbentry,dctonly)
DBENTRY *pdbentry;
int dctonly;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct recDes  	*precDes = pdbbase->precDes;
    RECNODE		*precnode = pdbentry->precnode;
    short           	record_type = pdbentry->record_type;
    struct fldDes  	*pflddes;
    struct recTypDes 	*precTypDes;
    short		indfield = pdbentry->indfield;

    if (record_type < 1) return (S_dbLib_recdesNotFound);
    /* read the record type definitions for this record */
    if ((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL) {
	return (S_dbLib_recdesNotFound);
    }
    indfield++;
    while(TRUE) {
	if(!(pflddes = GET_PFLDDES(precTypDes,indfield))) {
	    pdbentry->indfield = 0;
	    pdbentry->pflddes = NULL;
	    pdbentry->pfield = NULL;
	    return(S_dbLib_fieldNotFound);
	}
	if(!dctonly || pflddes->promptflag) {
	    pdbentry->indfield = indfield;
	    pdbentry->pflddes = pflddes;
	    if(precnode)
		pdbentry->pfield = ((char *)precnode->precord)+pflddes->offset;
	    else
		pdbentry->pfield = NULL;
	    return(0);
	}
	indfield++;
    }
}

#ifdef __STDC__
int  dbGetFieldType(DBENTRY *pdbentry)
#else
int  dbGetFieldType(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;
    long		status;

    if(!pflddes){
	status = S_dbLib_flddesNotFound;
	entryErrMessage(pdbentry,status,"dbGetFieldType");
	return(status);
    }
    return(mapDBFtoDCT[pflddes->field_type]);
}

#ifdef __STDC__
int dbGetNFields(DBENTRY *pdbentry,int dctonly)
#else
int dbGetNFields(pdbentry,dctonly)
DBENTRY *pdbentry;
int dctonly;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct recDes  	*precDes = pdbbase->precDes;
    short           	record_type = pdbentry->record_type;
    struct recTypDes 	*precTypDes;
    struct fldDes	*pflddes;
    int			indfield,n;

    if (record_type < 1) return (-1);
    /* read the record type definitions for this record */
    if ((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL) {
	return (-1);
    }
    n = 0;
    for(indfield=0; indfield<precTypDes->no_fields; indfield++) {
	if(!(pflddes = GET_PFLDDES(precTypDes,indfield))) {
	    errMessage(-1,"Logic Error dbGetNFields");
	    return(0);
	}
	if(!dctonly || pflddes->promptflag) n++;
    }
    return(n);
}

#ifdef __STDC__
char *dbGetFieldName(DBENTRY *pdbentry)
#else
char *dbGetFieldName(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(&pflddes->fldname[0]);
}

#ifdef __STDC__
char *dbGetPrompt(DBENTRY *pdbentry)
#else
char *dbGetPrompt(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(&pflddes->prompt[0]);
}

#ifdef __STDC__
int dbGetPromptGroup(DBENTRY *pdbentry)
#else
int dbGetPromptGroup(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(pflddes->promptflag);
}

#ifdef __STDC__
char *dbGetString(DBENTRY *pdbentry)
#else
char *dbGetString(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct fldDes  	*pflddes = pdbentry->pflddes;
    void		*pfield = pdbentry->pfield;
    char		*message;
    unsigned char 	cvttype;

    message = getpMessage(pdbentry);
    *message = 0;
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
    if(!pfield) {strcpy(message,"Field not found"); return(message);}
    cvttype = pflddes->cvt_type;
    switch (pflddes->field_type) {
    case DBF_STRING:
	strcpy(message, (char *)pfield);
	break;
    case DBF_CHAR:
	if(cvttype==CT_DECIMAL)
	    cvtCharToString(*(char*)pfield, message);
	else
	    ulongToHexString((unsigned long)(*(char*)pfield),message);
	break;
    case DBF_UCHAR:
	if(cvttype==CT_DECIMAL)
	    cvtUcharToString(*(unsigned char*)pfield, message);
	else
	    ulongToHexString((unsigned long)(*(unsigned char*)pfield),message);
	break;
    case DBF_SHORT:
	if(cvttype==CT_DECIMAL)
	    cvtShortToString(*(short*)pfield, message);
	else
	    ulongToHexString((unsigned long)(*(short*)pfield),message);
	break;
    case DBF_USHORT:
    case DBF_ENUM:
	if(cvttype==CT_DECIMAL)
	    cvtUshortToString(*(unsigned short*)pfield, message); 
	else
	    ulongToHexString((unsigned long)(*(unsigned short*)pfield),message);
	break;
    case DBF_LONG:
	if(cvttype==CT_DECIMAL) 
	    cvtLongToString(*(long*)pfield, message);
	else
	    ulongToHexString((unsigned long)(*(long*)pfield), message);
	break;
    case DBF_ULONG:
	if(cvttype==CT_DECIMAL)
	    cvtUlongToString(*(unsigned long *)pfield, message);
	else
	    ulongToHexString(*(unsigned long*)pfield, message);
	break;
    case DBF_FLOAT:
	floatToString(*(float *)pfield,message);
	break;
    case DBF_DOUBLE:
	doubleToString(*(double *)pfield,message);
	break;
    case DBF_GBLCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    short		choice_ind;
	    char		*pchoice;

	    choice_ind = *((short *) pdbentry->pfield);
	    if((!(pchoiceSet = GET_PCHOICE_SET(pdbbase->pchoiceGbl,choice_set)))
	    || (!(pchoice = GET_CHOICE(pchoiceSet, choice_ind)))) return (NULL);
	    strcpy(message, pchoice);
	}
	break;
    case DBF_RECCHOICE: {
	    struct arrChoiceSet	*parrChoiceSet;
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    short		record_type = pdbentry->record_type;
	    short		choice_ind;
	    char		*pchoice;
	    
	    choice_ind = *((short *) pdbentry->pfield);
	    if(!(parrChoiceSet = GET_PARR_CHOICE_SET(pdbbase->pchoiceRec,record_type))
	    || !(pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, choice_set))
	    || !(pchoice = GET_CHOICE(pchoiceSet, choice_ind))) return (NULL);
	    strcpy(message, pchoice);
	}
	break;
    case DBF_CVTCHOICE: {
	    short	choice_ind;
	    char	*pchoice;

	    choice_ind = *((short *) pdbentry->pfield);
	    if (!(pchoice = GET_CHOICE(pdbbase->pchoiceCvt, choice_ind))) 
	        return (NULL);
	    strcpy(message, pchoice);
	}
	break;
    case DBF_DEVCHOICE: {
	    struct devChoiceSet	*pdevChoiceSet;
	    short		record_type = pdbentry->record_type;
	    char		*pchoice;
	    short		choice_ind;

	    choice_ind = *((short *) pdbentry->pfield);
	    if(!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type))) return(NULL);
	    if(choice_ind<0 || choice_ind>=pdevChoiceSet->number)
		return(NULL);
	    pchoice = pdevChoiceSet->papChoice[choice_ind];
	    strcpy(message, pchoice);
	}
	break;
    case DBF_INLINK:
    case DBF_OUTLINK: {
	    DBLINK *plink=(DBLINK *)pfield;

	    switch(plink->type) {
	    case CONSTANT:
		doubleToString(plink->value.value,message);
		break;
	    case PV_LINK:
	        sprintf(message,"%s.%4.4s %s %s",
		    plink->value.pv_link.pvname,plink->value.pv_link.fldname,
		    ppstring[plink->value.pv_link.process_passive],
		    msstring[plink->value.pv_link.maximize_sevr]);
		break;
	    case VME_IO:
		sprintf(message,"#C%d S%d",
		    plink->value.vmeio.card,plink->value.vmeio.signal);
		break;
	    case CAMAC_IO:
		sprintf(message,"#B%d C%d S%d A%d",
		    plink->value.camacio.branch,plink->value.camacio.crate,
		    plink->value.camacio.slot,plink->value.camacio.channel);
		break;
	    case AB_IO:
		sprintf(message,"#L%d A%d C%d S%d F%d",
		    plink->value.abio.link,plink->value.abio.adapter,
		    plink->value.abio.card,plink->value.abio.signal,
		    plink->value.abio.plc_flag);
		break;
	    case GPIB_IO:
		sprintf(message,"#L%d A%d @%s",
		    plink->value.gpibio.link,plink->value.gpibio.addr,
		    plink->value.gpibio.parm);
		break;
	    case BITBUS_IO:
		sprintf(message,"#L%u N%u P%u S%u @%s",
		    plink->value.bitbusio.link,plink->value.bitbusio.node,
		    plink->value.bitbusio.port,plink->value.bitbusio.signal,
		    plink->value.bitbusio.parm);
		break;
	    case BBGPIB_IO:
		sprintf(message,"#L%u B%u G%u @%s",
		    plink->value.bbgpibio.link,plink->value.bbgpibio.bbaddr,
		    plink->value.bbgpibio.gpibaddr,plink->value.bbgpibio.parm);
		break;
	    case INST_IO:
		sprintf(message,"%s", plink->value.instio.string);
		break;
	    case VXI_IO :
		if (plink->value.vxiio.flag == VXIDYNAMIC)
		    sprintf(message,"#V%d C%d @%s",
			plink->value.vxiio.frame,plink->value.vxiio.slot,
			plink->value.vxiio.parm);
		else
		    sprintf(message,"#V%d @%s",
			plink->value.vxiio.la,plink->value.vxiio.parm);
		break;
	    default :
	        return(NULL);
	    }
	}
	break;
    case DBF_FWDLINK: {
	    DBLINK *plink=(DBLINK *)pfield;

	    switch(plink->type) {
	    case CONSTANT:
		doubleToString(plink->value.value,message);
		break;
	    case PV_LINK:
	        sprintf(message,"%s.%4.4s %s %s",
		    plink->value.pv_link.pvname,plink->value.pv_link.fldname,
		    ppstring[plink->value.pv_link.process_passive],
		    msstring[plink->value.pv_link.maximize_sevr]);
		break;
	    default :
	        return(NULL);
	    }
	}
	break;
    default:
	return(NULL);
    }
    return (message);
}

/* utility routines used by dbPutString */
#ifdef __STDC__
static long checkDevChoice(DBENTRY *pdbentry, long link_type)
#else
static long checkDevChoice(pdbentry, link_type)
DBENTRY *pdbentry;
long link_type;
#endif /*__STDC__*/
{
    struct fldDes  	*savepflddes = pdbentry->pflddes;
    void		*savepfield = pdbentry->pfield;
    short		saveindfield = pdbentry->indfield;
    struct fldDes	*pflddes;
    DBLINK		*plink;
    long		status=0;

    status = dbFindField(pdbentry,"INP");
    if(status) status = dbFindField(pdbentry,"OUT");
    if(!status) {
	pflddes = pdbentry->pflddes;
	plink = (DBLINK *)(pdbentry->pfield);
	if(plink->type == link_type) goto clean_up;
	if(link_type==CONSTANT && plink->type==PV_LINK)goto clean_up;
	if(link_type!=CONSTANT && plink->type==PV_LINK) {
	    status = S_dbLib_badField;
	    goto clean_up;
	}
	memset((char *)plink,0,sizeof(struct link));
	plink->type = link_type;
    } else {
	if(link_type==CONSTANT) status = 0;
	else status = S_dbLib_badField;
    }
clean_up:
    pdbentry->pflddes = savepflddes;
    pdbentry->pfield = savepfield;
    pdbentry->indfield = saveindfield;
    return(status);
}

#ifdef __STDC__
long dbPutString(DBENTRY *pdbentry,char *pstring)
#else
long dbPutString(pdbentry,pstring)
DBENTRY *pdbentry;
char *pstring;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct fldDes  	*pflddes = pdbentry->pflddes;
    void		*pfield = pdbentry->pfield;
    long		status=0;

    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!pfield) return(S_dbLib_fieldNotFound);
    switch (pflddes->field_type) {
    case DBF_STRING:
	strncpy((char *)pfield, pstring,pflddes->size);
	break;
    case DBF_CHAR :
    case DBF_SHORT :
    case DBF_LONG:{
	    long  value;
	    char  *endp;

	    value = strtol(pstring,&endp,0);
	    if(*endp!=0) status = S_dbLib_badField;
	    switch (pflddes->field_type) {
	    case DBF_CHAR : *(char *)pfield = value; break;
	    case DBF_SHORT : *(short *)pfield = value; break;
	    case DBF_LONG : *(long *)pfield = value; break;
	    }
	}
	break;
    case DBF_UCHAR:
    case DBF_USHORT:
    case DBF_ULONG:
    case DBF_ENUM:{
	    unsigned long  value;
	    char  *endp;

	    value = strtoul(pstring,&endp,0);
	    if(*endp!=0) status = S_dbLib_badField;
	    switch (pflddes->field_type) {
	    case DBF_UCHAR : *(unsigned char *)pfield = value; break;
	    case DBF_USHORT:
	    case DBF_ENUM: *(unsigned short *)pfield=value; break;
	    case DBF_ULONG : *(unsigned long *)pfield = value; break;
	    }
	}
	break;
    case DBF_FLOAT:
    case DBF_DOUBLE: {	
	    double value;
	    char  *endp;

	    value = strtod(pstring,&endp);
	    if(*endp!=0) status = S_dbLib_badField;
	    if(pflddes->field_type==DBF_FLOAT)
	    	*(float *)pfield = value;
	    else
		*(double *)pfield = value;
	}
	break;
    case DBF_GBLCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    char		*pchoice;
	    int			i;

	    if(!(pchoiceSet = GET_PCHOICE_SET(pdbbase->pchoiceGbl, choice_set)))
		 return(S_dbLib_menuNotFound);
	    for (i = 0; i < pchoiceSet->number; i++) {
		if (!(pchoice = pchoiceSet->papChoice[i])) continue;
		if (strcmp(pchoice, pstring) == 0) {
		    *(unsigned short *)pfield = i;
		    return(0);
		}
	    }
	}
	return (S_dbLib_badField);
    case DBF_RECCHOICE: {
	    struct arrChoiceSet	*parrChoiceSet;
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    short		record_type = pdbentry->record_type;
	    unsigned short	choice_ind;
	    char		*pchoice;
	    int			i;
	    
	    choice_ind = *((short *) pdbentry->pfield);
	    if(!(parrChoiceSet = GET_PARR_CHOICE_SET(pdbbase->pchoiceRec,record_type))
	    || !(pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, choice_set))) 
		return(S_dbLib_menuNotFound);
	    for (i = 0; i < pchoiceSet->number; i++) {
		if(!(pchoice = pchoiceSet->papChoice[i])) continue;
		if(strcmp(pchoice, pstring) == 0) {
		    *(unsigned short *)pfield = i;
		    return(0);
		}
	    }
	}
	return (S_dbLib_badField);
    case DBF_CVTCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    char                *pchoice;
	    int			i;

	    if(!(pchoiceSet = pdbbase->pchoiceCvt)) return(S_dbLib_menuNotFound);
	    for (i = 0; i < pchoiceSet->number; i++) {
		if (!(pchoice = pchoiceSet->papChoice[i]))
		    continue;
		if (strcmp(pchoice, pstring) == 0) {
		    *(unsigned short *)pfield = i;
		    return(0);
		}
	    }
	}
	return (S_dbLib_badField);
    case DBF_DEVCHOICE: {
	    struct devChoiceSet	*pdevChoiceSet;
	    short		record_type = pdbentry->record_type;
	    char		*pchoice;
	    int			i;

	    if(!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type))) 
		return(S_dbLib_menuNotFound);
	    if(!(pdevChoiceSet = GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type)))
		return(S_dbLib_menuNotFound);
	    for (i = 0; i < pdevChoiceSet->number; i++) {
		if (!(pchoice = pdevChoiceSet->papDevChoice[i]->pchoice))
		    continue;
		if (strcmp(pchoice, pstring) == 0) {
		    long link_type,status;

		    link_type = pdevChoiceSet->papDevChoice[i]->link_type;
		    status = checkDevChoice(pdbentry,link_type);
		    if(!status) *(unsigned short *)pfield = i;
		    return(status);
		}
	    }
	}
	return(S_dbLib_badField);
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK: {
	    DBLINK	*plink=(DBLINK *)pfield;
	    char	string[80];
	    char	*pstr=&string[0];

	    if(strlen(pstring)>=sizeof(string)) {
	        status = S_dbLib_badField;
	        errMessage(status,"dbPutString received a LONG string");
	        return(status);
            }
	    strcpy(pstr,pstring);
	    /*strip off leading blanks and tabs*/
	    while(*pstr && (*pstr==' ' || *pstr=='\t')) pstr++;
	    if(!pstr || strlen(pstr)<=0 ) {
		if(plink->type==PV_LINK) dbCvtLinkToConstant(pdbentry);
		if(plink->type!=CONSTANT) return(S_dbLib_badField);
		return(0);
	    }
	    switch(plink->type) {
	    case CONSTANT: 
	    case PV_LINK: {
	    	    int		pp=0;
		    int		ms=0;
	    	    char	*end;
		    char	chr;

		    /* Check first to see if string is a constant*/
		    chr = pstr[0];
		    if(isdigit(chr) || chr=='.' || chr=='-' || chr=='+') {
			if(plink->type==PV_LINK) dbCvtLinkToConstant(pdbentry);
			plink->value.value = strtod(pstr,&end);
			if(*end!=0) return(S_dbLib_badField);
			return(0);
		    }
		    if(plink->type==CONSTANT) dbCvtLinkToPvlink(pdbentry);
	    	    end = strchr(pstr,' ');
		    if(end) {
		        if(strstr(end," PP")) pp = TRUE;
		        if(strstr(end,".PP")) pp = TRUE;
		        if(strstr(end," MS")) ms = TRUE;
		        if(strstr(end,".MS")) ms = TRUE;
		        *end = 0;
		    }
		    status = dbPutPvlink(pdbentry,pp,ms,pstr);
		    return(0);
		}
	    case VME_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.vmeio.card);
		    if(!(end = strchr(pstr,'S'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.vmeio.signal);
		}
		break;
	    case CAMAC_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'B'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.camacio.branch);
		    if(!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.camacio.crate);
		    if(!(end = strchr(pstr,'S'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.camacio.slot);
		    if(!(end = strchr(pstr,'A'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.camacio.channel);
		}
		break;
	    case AB_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.abio.link);
		    if(!(end = strchr(pstr,'A'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.abio.adapter);
		    if(!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.abio.card);
		    if(!(end = strchr(pstr,'S'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.abio.signal);
		    if(!(end = strchr(pstr,'F')))  {
			plink->value.abio.plc_flag = 0;
		    } else {
		        pstr = end + 1;
		        sscanf(pstr,"%hd",&plink->value.abio.plc_flag);
		    }
		}
		break;
	    case GPIB_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.gpibio.link);
		    if(!(end = strchr(pstr,'A'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.gpibio.addr);
		    plink->value.gpibio.parm[0] = 0;
		    if(!(end = strchr(pstr,'@'))) {
		        pstr = end + 1;
		        sscanf(pstr,"%s",&plink->value.gpibio.parm[0]);
		    }
		}
		break;
	    case BITBUS_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.bitbusio.link);
		    if(!(end = strchr(pstr,'N'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.bitbusio.node);
		    if(!(end = strchr(pstr,'P'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.bitbusio.port);
		    if(!(end = strchr(pstr,'S'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.bitbusio.signal);
		    plink->value.bitbusio.parm[0] = 0;
		    if(!(end = strchr(pstr,'@'))) {
		        pstr = end + 1;
		        sscanf(pstr,"%s",&plink->value.bitbusio.parm[0]);
		    }
		}
		break;
	    case BBGPIB_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.bbgpibio.link);
		    if(!(end = strchr(pstr,'B'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.bbgpibio.bbaddr);
		    if(!(end = strchr(pstr,'G'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.bbgpibio.gpibaddr);
		    plink->value.bbgpibio.parm[0] = 0;
		    if(!(end = strchr(pstr,'@'))) {
		        pstr = end + 1;
		        sscanf(pstr,"%s",&plink->value.bbgpibio.parm[0]);
		    }
		}
		break;
	    case VXI_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    memset((char *)&plink->value.vxiio,0,sizeof(struct  vxiio));
		    if(!(end = strchr(pstr,'C')))  {
			plink->value.vxiio.flag = VXISTATIC;
		        if(!(end = strchr(pstr,'V'))) return (S_dbLib_badField);
		        pstr = end + 1;
		        sscanf(pstr,"%hd",&plink->value.vxiio.la);
		    } else {
			plink->value.vxiio.flag = VXIDYNAMIC;
		        if(!(end = strchr(pstr,'V'))) return (S_dbLib_badField);
		        pstr = end + 1;
		        sscanf(pstr,"%hd",&plink->value.vxiio.frame);
		        if(!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		        pstr = end + 1;
		        sscanf(pstr,"%hd",&plink->value.vxiio.slot);
		    }
		    if(!(end = strchr(pstr,'@'))) {
		        pstr = end + 1;
		        sscanf(pstr,"%s",&plink->value.gpibio.parm[0]);
		    }
		}
		break;
	    case INST_IO: {
		    sscanf(pstr,"%s",&plink->value.gpibio.parm[0]);
		}
		break;
	    }
	}
	break;
    default:
	return (S_dbLib_badField);
    }
    return(status);
}

#ifdef __STDC__
char *dbVerify(DBENTRY *pdbentry,char *pstring)
#else
char *dbVerify(pdbentry,pstring)
DBENTRY *pdbentry;
char *pstring;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct fldDes  	*pflddes = pdbentry->pflddes;
    void		*pfield = pdbentry->pfield;
    char		*message;

    message = getpMessage(pdbentry);
    *message = 0;
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
    if(!pfield) {strcpy(message,"Field not found"); return(message);}
    switch (pflddes->field_type) {
    case DBF_STRING: {
	    unsigned int length;

	    length=strlen(pstring);
	    if(length>=pflddes->size) {
		sprintf(message,"string to big. max=%hd",pflddes->size);
		return(message);
	    }
	}
	return(NULL);
    case DBF_CHAR :
    case DBF_SHORT :
    case DBF_LONG:{
	    long  value;
	    char  *endp;

	    value = strtol(pstring,&endp,0);
	    if(*endp!=0) {
		strcpy(message,"not an integer number");
		return(message);
	    }
	    switch (pflddes->field_type) {
	    case DBF_CHAR : 
		if(value<-128 || value>127) {
		    strcpy(message,"must have -128<=value<=127");
		    return(message);
		}
		return(NULL);
	    case DBF_SHORT : 
		if(value<-32768 || value>32767) {
		    strcpy(message,"must have -32768<=value<=32767");
		    return(message);
		}
		return(NULL);
	    case DBF_LONG : return(NULL);
	    }
	}
    case DBF_UCHAR:
    case DBF_USHORT:
    case DBF_ULONG:
    case DBF_ENUM:{
	    unsigned long  value;
	    char  *endp;

	    value = strtoul(pstring,&endp,0);
	    if(*endp!=0) {
		strcpy(message,"not an integer number");
		return(message);
	    }
	    switch (pflddes->field_type) {
	    case DBF_UCHAR : 
		if(value>255) {
		    strcpy(message,"must have 0<=value<=255");
		    return(message);
		}
		return(NULL);
	    case DBF_ENUM:
	    case DBF_USHORT : 
		if(value>65535) {
		    strcpy(message,"must have 0<=value<=65535");
		    return(message);
		}
		return(NULL);
	    case DBF_ULONG : return(NULL);
	    }
	}
    case DBF_FLOAT:
    case DBF_DOUBLE: {	
	    double value;
	    char  *endp;

	    value = strtod(pstring,&endp);
	    if(*endp!=0) {
		strcpy(message,"not a number");
		return(message);
	    }
	    return(NULL);
	}
    case DBF_GBLCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    char		*pchoice;
	    int			i;

	    if(!(pchoiceSet=GET_PCHOICE_SET(pdbbase->pchoiceGbl, choice_set))){
		strcpy(message,"Menu not found");
		return (message);
	    }
	    for (i = 0; i < pchoiceSet->number; i++) {
		if (!(pchoice = pchoiceSet->papChoice[i])) continue;
		if (strcmp(pchoice, pstring) == 0) {
		    return(NULL);
		}
	    }
	}
	strcpy(message,"Not a valid menu choice");
	return (message);
	break;
    case DBF_RECCHOICE: {
	    struct arrChoiceSet	*parrChoiceSet;
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    short		record_type = pdbentry->record_type;
	    unsigned short	choice_ind;
	    char		*pchoice;
	    int			i;
	    
	    choice_ind = *((short *) pdbentry->pfield);
	    if(!(parrChoiceSet = GET_PARR_CHOICE_SET(pdbbase->pchoiceRec,record_type))
	    || !(pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, choice_set))) {
		strcpy(message,"Menu not found");
		return (message);
	    }
	    for (i = 0; i < pchoiceSet->number; i++) {
		if(!(pchoice = pchoiceSet->papChoice[i])) continue;
		if(strcmp(pchoice, pstring) == 0) {
		    return(NULL);
		}
	    }
	}
	strcpy(message,"Not a valid menu choice");
	return (message);
	break;
    case DBF_CVTCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    char                *pchoice;
	    int			i;

	    if(!(pchoiceSet = pdbbase->pchoiceCvt)) {
		strcpy(message,"Menu not found");
		return (message);
	    }
	    for (i = 0; i < pchoiceSet->number; i++) {
		if (!(pchoice = pchoiceSet->papChoice[i]))
		    continue;
		if (strcmp(pchoice, pstring) == 0) {
		    return(NULL);
		}
	    }
	}
	strcpy(message,"Not a valid menu choice");
	return (message);
	break;
    case DBF_DEVCHOICE: {
	    struct devChoiceSet	*pdevChoiceSet;
	    short		record_type = pdbentry->record_type;
	    char		*pchoice;
	    int			i;

	    if(!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type))) {
		strcpy(message,"Menu not found");
		return (message);
	    }
	    if(!(pdevChoiceSet = GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type))) {
		strcpy(message,"Menu not found");
		return (message);
	    }
	    for (i = 0; i < pdevChoiceSet->number; i++) {
		if (!(pchoice = pdevChoiceSet->papDevChoice[i]->pchoice))
		    continue;
		if (strcmp(pchoice, pstring) == 0) {
		    return (NULL);
		}
	    }
	}
	strcpy(message,"Not a valid menu choice");
	return (message);
	break;
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	return(NULL);
    default: break;
    }
    strcpy(message,"Not a valid field type");
    return (message);
}

#ifdef __STDC__
char *dbGetRange(DBENTRY *pdbentry)
#else
char *dbGetRange(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;
    char		*message;

    message = getpMessage(pdbentry);
    *message = 0;
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
    switch (pflddes->field_type) {
    case DBF_STRING: {strcpy(message,"string"); return(message);}
    case DBF_CHAR : {strcpy(message,"-128<->127"); return(message);}
    case DBF_SHORT : {strcpy(message,"-32768<->32767");return(message);}
    case DBF_LONG: {strcpy(message,"integer"); return(message);}
    case DBF_UCHAR: {strcpy(message,"0<->255");return(message);}
    case DBF_USHORT:{strcpy(message,"0<->65535");return(message);}
    case DBF_ULONG:{strcpy(message,"unsigned integer");return(message);}
    case DBF_ENUM: return(NULL);
    case DBF_FLOAT:
    case DBF_DOUBLE: {strcpy(message,"float");return(message);}
    case (DBF_GBLCHOICE):
    case (DBF_CVTCHOICE):
    case (DBF_RECCHOICE):
    case (DBF_DEVCHOICE):
		{strcpy(message,"menu");return(message);}
    case DBF_INLINK: {strcpy(message,"inlink");return(message);}
    case DBF_OUTLINK: {strcpy(message,"outlink");return(message);}
    case DBF_FWDLINK: {strcpy(message,"fwdlink");return(message);}
    }
    strcpy(message,"Not a valid field type");
    return (message);
}

#ifdef __STDC__
int  dbIsDefaultValue(DBENTRY *pdbentry)
#else
int  dbIsDefaultValue(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;
    void           	*pfield = pdbentry->pfield;

    if(!pflddes) return(FALSE);
    if(!pfield) return(FALSE);
    switch (pflddes->field_type) {
	case (DBF_STRING) :
	    return(*((char *)pfield)==0);
	case (DBF_CHAR):
	    return((*(char *) pfield) == pflddes->initial.char_value);
	case (DBF_UCHAR):
	    return((*(unsigned char *) pfield) == pflddes->initial.uchar_value);
	case (DBF_SHORT):
	    return((*(short *) pfield) == pflddes->initial.short_value);
	case (DBF_USHORT):
	    return((*(unsigned short *) pfield) == pflddes->initial.ushort_value);
	case (DBF_LONG):
	    return((*(long *) pfield) == pflddes->initial.long_value);
	case (DBF_ULONG):
	    return((*(unsigned long *) pfield) == pflddes->initial.ulong_value);
	case (DBF_FLOAT):
	    return((*(float *) pfield) == pflddes->initial.float_value);
	case (DBF_DOUBLE):
	    return((*(double *) pfield) == pflddes->initial.double_value);
	case (DBF_GBLCHOICE):
	case (DBF_CVTCHOICE):
	case (DBF_RECCHOICE):
	case (DBF_DEVCHOICE):
	case (DBF_ENUM):
	    return((*(unsigned short *) pfield) == pflddes->initial.enum_value);
	case (DBF_INLINK):
	case (DBF_OUTLINK):
	case (DBF_FWDLINK):
	    if(((struct link *) pfield)->type !=CONSTANT) return(FALSE);
	    return(((struct link *) pfield)->value.value == 0.0);
    }
    return(FALSE);
}

#ifdef __STDC__
char   **dbGetChoices(DBENTRY *pdbentry)
#else
char   **dbGetChoices(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct fldDes  	*pflddes = pdbentry->pflddes;
    void		*pfield = pdbentry->pfield;
    unsigned char 	cvttype;

    if(!pflddes) return(NULL);
    if(!pfield) return(NULL);
    cvttype = pflddes->cvt_type;
    switch (pflddes->field_type) {
    case DBF_GBLCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;

	    if((!(pchoiceSet = GET_PCHOICE_SET(pdbbase->pchoiceGbl,choice_set))))
	        return (NULL);
	    return(pchoiceSet->papChoice);
	}
	break;
    case DBF_RECCHOICE: {
	    struct arrChoiceSet	*parrChoiceSet;
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    short		record_type = pdbentry->record_type;
	    
	    if(!(parrChoiceSet = GET_PARR_CHOICE_SET(pdbbase->pchoiceRec,record_type))
	    || !(pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, choice_set)))
	        return (NULL);
	    return(pchoiceSet->papChoice);
	}
	break;
    case DBF_CVTCHOICE: {
	    return(pdbbase->pchoiceCvt->papChoice);
	}
	break;
    case DBF_DEVCHOICE: {
	    struct devChoiceSet	*pdevChoiceSet;
	    short		record_type = pdbentry->record_type;

	    if(!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type))) return(NULL);
	    return(pdevChoiceSet->papChoice);
	}
	break;
    default:
	return(NULL);
    }
}

#ifdef __STDC__
int dbGetMenuIndex(DBENTRY *pdbentry)
#else
int dbGetMenuIndex(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;
    void		*pfield = pdbentry->pfield;

    if(!pflddes) return(-1);
    if(!pfield) return(-1);
    switch (pflddes->field_type) {
	case (DBF_GBLCHOICE):
	case (DBF_CVTCHOICE):
	case (DBF_RECCHOICE):
	case (DBF_DEVCHOICE):
    	return((int)(*(unsigned short *)pfield));
    }
    return(-1);
}

#ifdef __STDC__
long dbPutMenuIndex(DBENTRY *pdbentry,int index)
#else
long dbPutMenuIndex(pdbentry,index)
DBENTRY *pdbentry;
int index;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct fldDes  	*pflddes = pdbentry->pflddes;
    unsigned short	*pfield = pdbentry->pfield;

    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!pfield) return(S_dbLib_fieldNotFound);
    switch (pflddes->field_type) {
    case DBF_GBLCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;

	    if(!(pchoiceSet = GET_PCHOICE_SET(pdbbase->pchoiceGbl, choice_set)))
		 return(S_dbLib_menuNotFound);
	    if(index<0 | index>=pchoiceSet->number) return(S_dbLib_badField);
	    *pfield = (unsigned short)index;
	    return(0);
	}
    case DBF_RECCHOICE: {
	    struct arrChoiceSet	*parrChoiceSet;
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    short		record_type = pdbentry->record_type;
	    
	    if(!(parrChoiceSet = GET_PARR_CHOICE_SET(pdbbase->pchoiceRec,record_type))
	    || !(pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, choice_set))) 
		return(S_dbLib_menuNotFound);
	    if(index<0 | index>=pchoiceSet->number) return(S_dbLib_badField);
	    *pfield = (unsigned short)index;
	    return(0);
	}
    case DBF_CVTCHOICE: {
	    struct choiceSet	*pchoiceSet;

	    if(!(pchoiceSet = pdbbase->pchoiceCvt)) return(S_dbLib_menuNotFound);
	    if(index<0 | index>=pchoiceSet->number) return(S_dbLib_badField);
	    *pfield = (unsigned short)index;
	    return(0);
	}
    case DBF_DEVCHOICE: {
	    struct devChoiceSet	*pdevChoiceSet;
	    short		record_type = pdbentry->record_type;

	    if(!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type))) 
		return(S_dbLib_menuNotFound);
	    if(!(pdevChoiceSet = GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type)))
		return(S_dbLib_menuNotFound);
	    if(index<0 | index>=pdevChoiceSet->number) return(S_dbLib_badField);
	    *pfield = (unsigned short)index;
	    return(0);
	}
    default:
	break;
    }
    return (S_dbLib_badField);
}

#ifdef __STDC__
int dbGetNMenuChoices(DBENTRY *pdbentry)
#else
int dbGetNMenuChoices(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE 		*pdbbase = pdbentry->pdbbase;
    struct fldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(-1);
    switch (pflddes->field_type) {
    case DBF_GBLCHOICE: {
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;

	    if(!(pchoiceSet = GET_PCHOICE_SET(pdbbase->pchoiceGbl, choice_set)))
		 return(-1);
	    return(pchoiceSet->number);
	}
    case DBF_RECCHOICE: {
	    struct arrChoiceSet	*parrChoiceSet;
	    struct choiceSet	*pchoiceSet;
	    short		choice_set = pflddes->choice_set;
	    short		record_type = pdbentry->record_type;
	    
	    if(!(parrChoiceSet = GET_PARR_CHOICE_SET(pdbbase->pchoiceRec,record_type))
	    || !(pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, choice_set))) 
		return(0);
	    return(pchoiceSet->number);
	}
    case DBF_CVTCHOICE: {
		return(pdbbase->pchoiceCvt->number);
	}
    case DBF_DEVCHOICE: {
	    struct devChoiceSet	*pdevChoiceSet;
	    short		record_type = pdbentry->record_type;

	    if(!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type))) 
		return(0);
	    if(!(pdevChoiceSet = GET_PDEV_CHOICE_SET(pdbbase->pchoiceDev,record_type)))
		return(0);
	    return(pdevChoiceSet->number);
	}
    default:
	break;
    }
    return (-1);
}

#ifdef __STDC__
long dbCopyMenu(DBENTRY *from,DBENTRY *to)
#else
long dbCopyMenu(from,to)
DBENTRY *from;
DBENTRY *to;
#endif /*__STDC__*/
{

    errMessage(-1,"dbCopyMenu not implemented");
    return(-1);
}

#ifdef __STDC__
int dbAllocForm(DBENTRY *pdbentry)
#else
int dbAllocForm(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes  	*pflddes = pdbentry->pflddes;
    void		*pfield = pdbentry->pfield;
    short		indfield = pdbentry->indfield;
    struct fldDes	*plinkflddes;
    DBLINK		*plink;
    int			nlines=0;
    char		*pstr;
    struct form		*pform;
    long		status;
    int			nbytes,i;

    if(pflddes->field_type != DBF_DEVCHOICE) return(0);
    status = dbFindField(pdbentry,"INP");
    if(status) status = dbFindField(pdbentry,"OUT");
    if(!status) {
	plinkflddes = pdbentry->pflddes;
	plink = (DBLINK *)(pdbentry->pfield);
	nlines = formlines[plink->type];
	nbytes = sizeof(struct form) 
		+ 2*nlines*(sizeof(char *) + MAX_STRING_SIZE);
	pform = dbCalloc(1,nbytes);
	pdbentry->formpvt = pform;
	pform->plink = plink ;
	pform->prompt = promptAddr[plink->type];
	pform->value = (char **)((char *)pform + sizeof(struct form));
	pform->verify = (char **)((char *)(pform->value)+nlines*sizeof(char *));
	pstr = (char *)(pform->verify) + nlines*sizeof(char *);
	for(i=0; i<nlines; i++) {
	    pform->value[i] = pstr;
	    pstr += MAX_STRING_SIZE;
	}
	for(i=0; i<nlines; i++) {
	    pform->verify[i] = pstr;
	    pstr += MAX_STRING_SIZE;
	}
	
    }
    pdbentry->pflddes = pflddes;
    pdbentry->pfield = pfield;
    pdbentry->indfield = indfield;
    return(nlines);
}

#ifdef __STDC__
long  dbFreeForm(DBENTRY *pdbentry)
#else
long  dbFreeForm(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    if(pdbentry->formpvt) free(pdbentry->formpvt);
    return(0);
}

#ifdef __STDC__
char  **dbGetFormPrompt(DBENTRY *pdbentry)
#else
char  **dbGetFormPrompt(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct form *pform = pdbentry->formpvt;

    if(!pform) return(NULL);
    return(pform->prompt);
}

#ifdef __STDC__
char  **dbGetFormValue(DBENTRY *pdbentry)
#else
char  **dbGetFormValue(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct form *pform = pdbentry->formpvt;
    DBLINK	*plink;
    char	**value;

    if(!pform) return(NULL);
    plink = pform->plink;
    if(!plink) return(NULL);
    value = pform->value;
    switch(plink->type) {
    case CONSTANT: 
	doubleToString(plink->value.value,*value );
	break;
    case PV_LINK:
	strcpy(*value,plink->value.pv_link.pvname);
	value++;
	strncpy(*value,plink->value.pv_link.fldname,4);
	value++;
	strcpy(*value, (plink->value.pv_link.process_passive ? "Yes" : "No"));
	value++;
	strcpy(*value, (plink->value.pv_link.maximize_sevr ? "Yes" : "No"));
	break;
    case VME_IO:
	cvtShortToString(plink->value.vmeio.card,*value);
	value++;
	cvtShortToString(plink->value.vmeio.signal,*value);
	break;
    case CAMAC_IO:
	cvtShortToString(plink->value.camacio.branch,*value);
	value++;
	cvtShortToString(plink->value.camacio.crate,*value);
	value++;
	cvtShortToString(plink->value.camacio.slot,*value);
	value++;
	cvtShortToString(plink->value.camacio.channel,*value);
	break;
    case AB_IO:
	cvtShortToString(plink->value.abio.link,*value);
	value++;
	cvtShortToString(plink->value.abio.adapter,*value);
	value++;
	cvtShortToString(plink->value.abio.card,*value);
	value++;
	cvtShortToString(plink->value.abio.signal,*value);
	value++;
	strcpy(*value, (plink->value.abio.plc_flag ? "Yes" : "No"));
	break;
    case GPIB_IO:
	cvtShortToString(plink->value.gpibio.link,*value);
	value++;
	cvtShortToString(plink->value.gpibio.addr,*value);
	value++;
	strcpy(*value,plink->value.gpibio.parm);
	break;
    case BITBUS_IO:
	cvtCharToString(plink->value.bitbusio.link,*value);
	value++;
	cvtCharToString(plink->value.bitbusio.node,*value);
	value++;
	cvtCharToString(plink->value.bitbusio.port,*value);
	value++;
	cvtCharToString(plink->value.bitbusio.signal,*value);
	value++;
	strcpy(*value,plink->value.bitbusio.parm);
	break;
    case INST_IO:
	strcpy(*value,plink->value.instio.string);
	break;
    case BBGPIB_IO:
	cvtCharToString(plink->value.bbgpibio.link,*value);
	value++;
	cvtCharToString(plink->value.bbgpibio.bbaddr,*value);
	value++;
	cvtCharToString(plink->value.bbgpibio.gpibaddr,*value);
	value++;
	strcpy(*value,plink->value.bbgpibio.parm);
	break;
    case VXI_IO:
	strcpy(*value,(plink->value.vxiio.flag == VXIDYNAMIC ? "Yes" : "No"));
	value++;
	if(plink->value.vxiio.flag = VXIDYNAMIC)
	    cvtShortToString(plink->value.vxiio.frame,*value);
	else
	    **value = 0;
	value++;
	if(plink->value.vxiio.flag = VXIDYNAMIC)
	    cvtShortToString(plink->value.vxiio.slot,*value);
	else
	    **value = 0;
	value++;
	if(plink->value.vxiio.flag = VXISTATIC)
	    cvtShortToString(plink->value.vxiio.la,*value);
	else
	    **value = 0;
	value++;
	strcpy(*value,plink->value.vxiio.parm);
	break;
    default :
	return(NULL);
    }
    return(pform->value);
}

#ifdef __STDC__
long  dbPutForm(DBENTRY *pdbentry,char **value)
#else
long  dbPutForm(pdbentry,value)
DBENTRY *pdbentry;
char **value;
#endif /*__STDC__*/
{
    struct form *pform = pdbentry->formpvt;
    DBLINK	*plink;
    char	**verify;
    long	lvalue;
    double	dvalue;
    char	*endp;

    if(!pform) return(S_dbLib_badLink);
    plink = pform->plink;
    if(!plink) return(S_dbLib_badLink);
    verify = pform->verify;
    switch(plink->type) {
    case CONSTANT: 
	dvalue = strtod(*value,&endp);
	if(*endp==0) {
	    plink->value.value = dvalue;
	    **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	break;
    case PV_LINK:
	strcpy(plink->value.pv_link.pvname,*value);
	**verify = 0;
	value++; verify++;
	strncpy(plink->value.pv_link.fldname,*value,4);
	**verify = 0;
	value++; verify++;
	plink->value.pv_link.process_passive = 
	    ((strchr(*value,'Y') || strchr(*value,'y') ? TRUE : FALSE));
	value++; verify++;
	plink->value.pv_link.maximize_sevr = 
	    ((strchr(*value,'Y') || strchr(*value,'y') ? TRUE : FALSE));
	break;
    case VME_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.vmeio.card = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.vmeio.signal = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	break;
    case CAMAC_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.branch = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.crate = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.slot = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.channel = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	break;
    case AB_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.abio.link = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.abio.adapter = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.abio.card = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.abio.signal = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	plink->value.abio.plc_flag = 
	    ((strchr(*value,'Y') || strchr(*value,'y') ? TRUE : FALSE));
	break;
    case GPIB_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.gpibio.link = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.gpibio.addr = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	strcpy(plink->value.gpibio.parm,*value);
	**verify = 0;
	break;
    case BITBUS_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.bitbusio.link = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.bitbusio.node = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.bitbusio.port = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.bitbusio.signal = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	strcpy(plink->value.bitbusio.parm,*value);
	**verify = 0;
	break;
    case INST_IO:
	strcpy(plink->value.instio.string,*value);
	**verify = 0;
	break;
    case BBGPIB_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.bbgpibio.link = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.bbgpibio.bbaddr = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.bbgpibio.gpibaddr = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	strcpy(plink->value.bbgpibio.parm,*value);
	**verify = 0;
	break;
    case VXI_IO:
	plink->value.vxiio.flag = 
	    ((strchr(*value,'Y')||strchr(*value,'y') ? VXIDYNAMIC : VXISTATIC));
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.vxiio.frame = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.vxiio.slot = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.vxiio.la = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	strcpy(plink->value.vxiio.parm,*value);
	**verify = 0;
	break;
    default :
	return(S_dbLib_badLink);
    }
    return(0);
}

#ifdef __STDC__
char  **dbVerifyForm(DBENTRY *pdbentry,char **value)
#else
char  **dbVerifyForm(pdbentry,value)
DBENTRY *pdbentry;
char **value;
#endif /*__STDC__*/
{
    struct form *pform = pdbentry->formpvt;
    DBLINK	*plink;
    DBLINK	savelink;
    int		nlines,i;

    if(!pform) return(NULL);
    plink = pform->plink;
    if(!plink) return(NULL);
    savelink = *plink;
    dbPutForm(pdbentry,value);
    *plink = savelink;
    nlines = pform->nlines;
    for(i=0; i<nlines; i++) if(pform->verify[i]) return(pform->verify);
    return(NULL);
}

#ifdef __STDC__
int dbGetNLinks(DBENTRY *pdbentry)
#else
int dbGetNLinks(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase = pdbentry->pdbbase;
    int			record_type = pdbentry->record_type;
    struct recDes  	*precDes = pdbbase->precDes;
    struct recTypDes 	*precTypDes = NULL;

    /* verify that record_type is valid			 */
    if(record_type < 1) return(-1);
    if ((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL)
	return(-1);
    return((int)precTypDes->no_links);
}

#ifdef __STDC__
long dbGetLinkField(DBENTRY *pdbentry,int index)
#else
long dbGetLinkField(pdbentry,index)
DBENTRY *pdbentry;
int index;
#endif /*__STDC__*/
{
    DBBASE		*pdbbase = pdbentry->pdbbase;
    RECNODE		*precnode = pdbentry->precnode;
    int			record_type = pdbentry->record_type;
    struct recDes  	*precDes = pdbbase->precDes;
    struct recTypDes 	*precTypDes = NULL;
    struct fldDes	*pflddes = NULL;

    /* verify that record_type is valid			 */
    if(record_type < 1) return(-1);
    if ((precTypDes = GET_PRECTYPDES(precDes, record_type)) == NULL)
	return(S_dbLib_recdesNotFound);
    if(index<0 || index>=precTypDes->no_links) return(S_dbLib_badField);
    pdbentry->indfield = precTypDes->link_ind[index];
    pdbentry->pflddes = pflddes = precTypDes->papFldDes[pdbentry->indfield];
    if(precnode) pdbentry->pfield = (char *)precnode->precord + pflddes->offset;
    else pdbentry->pfield = NULL;
    return(0);
}

#ifdef __STDC__
int dbGetLinkType(DBENTRY *pdbentry)
#else
int dbGetLinkType(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes	*pflddes = pdbentry->pflddes;
    DBLINK		*plink = (DBLINK *)pdbentry->pfield;
    int			field_type;
    int			INPorOUT=FALSE;

    if(!pflddes) return(-1);
    if(!plink) return(-1);
    if(strncmp(pflddes->fldname,"INP",4)==0
    || strncmp(pflddes->fldname,"INP ",4)==0
    || strncmp(pflddes->fldname,"OUT",4)==0
    || strncmp(pflddes->fldname,"OUT ",4)==0) INPorOUT = TRUE;
    field_type = pflddes->field_type;
    switch (field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	switch(plink->type) {
	case CONSTANT:
	    if(INPorOUT) return(DCT_LINK_FORM);
	    else return(DCT_LINK_CONSTANT);
	case PV_LINK:
	    return(DCT_LINK_PV);
	default:
	    return(DCT_LINK_DEVICE);
	}
    }
    return(-1);
}

#ifdef __STDC__
long dbCvtLinkToConstant(DBENTRY *pdbentry)
#else
long dbCvtLinkToConstant(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes	*pflddes = pdbentry->pflddes;
    DBLINK		*plink = (DBLINK *)pdbentry->pfield;;

    if(!pflddes) return(-1);
    if(!plink) return(-1);
    switch (pflddes->field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	if(plink->type == CONSTANT) return(0);
	if(plink->type != PV_LINK) return(S_dbLib_badLink);
	plink->type = CONSTANT;
	plink->value.value = 0.0;
	return(0);
    }
    return(S_dbLib_badLink);
}

#ifdef __STDC__
long dbCvtLinkToPvlink(DBENTRY *pdbentry)
#else
long dbCvtLinkToPvlink(pdbentry)
DBENTRY *pdbentry;
#endif /*__STDC__*/
{
    struct fldDes	*pflddes = pdbentry->pflddes;
    DBLINK		*plink = (DBLINK *)pdbentry->pfield;;

    if(!pflddes) return(-1);
    if(!plink) return(-1);
    switch (pflddes->field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	if(plink->type == PV_LINK) return(0);
	if(plink->type != CONSTANT) return(S_dbLib_badLink);
	plink->type = PV_LINK;
	plink->value.pv_link.process_passive = 0;
	plink->value.pv_link.maximize_sevr = 0;
	plink->value.pv_link.pvname[0] = 0;
	plink->value.pv_link.fldname[0] = 0;
	return(0);
    }
    return(S_dbLib_badLink);
}

#ifdef __STDC__
long dbPutPvlink(DBENTRY *pdbentry,int pp,int ms,char *pvname)
#else
long dbPutPvlink(pdbentry,pp,ms,pvname)
DBENTRY *pdbentry;
int pp;
int ms;
char *pvname;
#endif /*__STDC__*/
{
    struct fldDes	*pflddes = pdbentry->pflddes;
    DBLINK		*plink = (DBLINK *)pdbentry->pfield;
    char		*pname;
    int         	lenName=0;
    
    if(!pflddes) return(-1);
    if(!plink) return(-1);
    switch (pflddes->field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	if(plink->type != PV_LINK) return(S_dbLib_badLink);
	pname = plink->value.pv_link.pvname;
	while(*pvname  && *pvname!= '.' && lenName < PVNAME_SZ) {
	    *pname++ = *pvname++;
	    lenName++;
	}
	*pname++ = 0;
	pname = plink->value.pv_link.fldname;
	if(*pvname=='.') {
	    pvname++;
	    lenName = 0;
	    while(*pvname  && lenName < FLDNAME_SZ) {
		*pname++ = *pvname++;
		lenName++;
	    }
	    while(lenName++ < FLDNAME_SZ) {
		*pname++ = ' ';
	    }
	} else {
	    strncpy(pname,"VAL ",FLDNAME_SZ);
	}
	plink->value.pv_link.process_passive = pp;
	plink->value.pv_link.maximize_sevr = ms;
	return(0);
    }
    return(S_dbLib_badLink);
}

#ifdef __STDC__
long dbGetPvlink(DBENTRY *pdbentry,int *pp,int *ms,char *pvname)
#else
long dbGetPvlink(pdbentry,pp,ms,pvname)
DBENTRY *pdbentry;
int *pp;
int *ms;
char *pvname;
#endif /*__STDC__*/
{
    struct fldDes	*pflddes = pdbentry->pflddes;
    DBLINK		*plink = (DBLINK *)pdbentry->pfield;
    char		*pname;
    
    if(!pflddes) return(-1);
    if(!plink) return(-1);
    switch (pflddes->field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	if(plink->type != PV_LINK) return(S_dbLib_badLink);
	pname = plink->value.pv_link.pvname;
	while(*pname) {
	    *pvname++ = *pname++;
	}
	pname = plink->value.pv_link.fldname;
	if(*pname!=0 && *pname!=' ') {
	    *pvname++ = '.';
	    while(*pname && *pname!=' ') *pvname++ = *pname++;
	    *pvname = 0;
	}
	*pp = plink->value.pv_link.process_passive;
	*ms = plink->value.pv_link.maximize_sevr;
	return(0);
    }
    return(S_dbLib_badLink);
}

#ifdef __STDC__
void dbDumpRecords(DBBASE *pdbbase,char *precdesname,int modOnly)
#else
void dbDumpRecords(pdbbase,precdesname,modOnly)
DBBASE *pdbbase;
char *precdesname;
int modOnly;
#endif /*__STDC__*/
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;

    dbInitEntry(pdbbase,pdbentry);
    if(!precdesname)
	status = dbFirstRecdes(pdbentry);
    else
	status = dbFindRecdes(pdbentry,precdesname);
    if(status) {printf("No record description\n"); return;}
    while(!status) {
	printf("record type: %s",dbGetRecdesName(pdbentry));
	status = dbFirstRecord(pdbentry);
	if(status) printf("  No Records\n"); 
	while(!status) {
	    printf("\n  Record: %s\n",dbGetRecordName(pdbentry));
	    status = dbFirstFielddes(pdbentry,TRUE);
	    if(status) printf("    No Fields\n");
	    while(!status) {
		char *pstr;

		if(!dbIsDefaultValue(pdbentry) || !modOnly) {
		    printf("    %4s ",dbGetFieldName(pdbentry));
		    pstr = dbGetString(pdbentry);
		    if(pstr) printf("%s\n",pstr);
		    else printf("\n");
		}
		status=dbNextFielddes(pdbentry,TRUE);
	    }
	    status = dbNextRecord(pdbentry);
	}
	if(precdesname) break;
	status = dbNextRecdes(pdbentry);
    }
    printf("End of all Records\n");
    dbFinishEntry(pdbentry);
}

/* Read/Write routines*/
/* Loads self defining records */

/* An input file to be loaded by dbRead consists
 * of one or more SDR files. SDR_DB_RECTYPE is needed to decipher many
 * of the other SDRs. If it is present, it must be the first SDR in
 * the input file.
 */


/******************************************************************
 * DBREAD - loads one or more databases into memory
 *
 *    adds records to the pvd list and the appropriate record list
 *
 *    creates a sorted record list for each record type
 *
 *    An SDR record is composed of a header "type" followed by a data block
 *
 *    Format of a database:
 *        =========================================================
 *        
 *        =========================================================
 *        The set of SDR record "types" contained in the
 *             default.dctsdr file.
 *        "type" SDR_DB_RECTYPE - must be the first SDR record in the set.
 *                ...           - other ones in the set.
 *        "type" SDR_ALLSUMS    - is the last SDR record in the set.
 *        =========================================================
 *        Followed by one or more SDR records (of type SDR_DB_RECORDS)
 *            SDR_DB_RECORDS
 *                ...            - one for each recType in database
 *        =========================================================
 *        =========================================================
 *
 *      Each SDR record of type SDR_DB_RECORDS contains a
 *      recLoc structure followed by the contiguos set of PV records
 *      for the record type defined in the recLoc structure.
 *
 *      The 1st database loaded by dbRead creates and initializes various
 *      structures and computes the sdrFileSize of the default.dctsdr file
 *      which was copied verbatim when the database was created.
 *      This is currently only used when writing the database on the host.
 *      All databases that are loaded must have been created from the
 *      same default.dctsdr file and is enforced by dbRead.
 *
 *      When an attempt is made to load a PV already in the database
 *      a warning message is issued, and if the record type agrees
 *      the current version will be overwritten. Otherwise the new
 *      PV is skipped.
 *
 *      The definition of the 1st database:
 *          start of loading - arg1  (ppdbbase) is NULL
 *          end - EOF on file (and SDR_ALLSUMS found and checked)
 *          If ppdbbase != NULL it is NOT the 1st database.
 *****************************************************************/



#define SAME 0

/* forward references */
static int	adj_dbRecType();
static int	adj_dbRecords();
static int	adj_dbRecDes();
static int	adj_dbPvd();
static int	adj_choiceGbl();
static int	adj_choiceCvt();
static int	adj_choiceRec();
static int	adj_choiceDev();
static int	adj_devSup();
static int	adj_cvtTable();
static int	adj_drvSup();
static int	adj_recSup();
static int	adj_sdrSum();
static int	initadj_fun();
static void	post_adj_devSup();

/* array of pointers to adjusting functions */
static          (*adj_fun[SDR_NTYPES]) ();

/* Note: see addValidDefaults() function for sdr types in default.dctsdr file */

/* job types */
#define  LOAD_ONLY_ONE		0	/* only for bld tools */
#define  LOAD_DATABASE		1
#define  LOAD_DCT_DEFAULTS	2

static struct sdrHeader sdrHeader; /* place to read in SDR headers */

/*Convert an offset value to an address */
#ifdef __STDC__
static void *cvtOffsetToAddr(void *pbase,void *poff)
#else
static void *cvtOffsetToAddr(pbase,poff)
void *pbase;
void *poff;
#endif /*__STDC__*/
{
    long   offset;
    void   **poffset= (void **)&offset;

    *poffset = poff;
    return((void *)((char *)pbase + offset));
}

#ifdef __STDC__
long dbRead(DBBASE *pdbbase,FILE *fp)
#else
long dbRead(pdbbase,fp)
DBBASE *pdbbase;
FILE *fp;
#endif /*__STDC__*/
{
    int             job_type = 999;
    char           *ptmp = NULL;
    long	    status;


    initadj_fun(pdbbase);
    /* determine type of file to process */
    if (pdbbase->pvtSumFlag == 0) {
	job_type = LOAD_DATABASE;	/* prior to loading the database */
    } else if (pdbbase->pvtSumFlag == 1 )		 {
	/* load only one SDR file w/o checking sum */
	job_type = LOAD_ONLY_ONE;
    } else if (pdbbase->pvtSumFlag == 2) {
	/* load all of the SDR files except database record instances*/
	job_type = LOAD_DCT_DEFAULTS;
    }
    while (1) {
	    if ((fread((char *) &sdrHeader, sizeof(struct sdrHeader),
		       1, fp)) != 1) {
		if (feof(fp)) {
    		    post_adj_devSup(pdbbase);
    		    return (0);
		}
		status = S_sdr_noRead;
		errMessage(status,"dbRead: READHEADER");
		return(-1);
		break;
	    }
	    if (sdrHeader.magic != DBMAGIC) {
		status = S_sdr_noMagic;
		errMessage(status,"dbRead: READHEADER - wrong DBMAGIC");
		return(-1);
		break;
	    }
	    if (sdrHeader.type > SDR_NTYPES || sdrHeader.type < 0) {
		status = S_sdr_noRecDef;
		errMessage(status,"dbRead: READHEADER - bad type");
		return(-1);
		break;
	    }
	    ptmp = dbCalloc(1,sdrHeader.nbytes);
	    if ((fread((char *) ptmp,  sdrHeader.nbytes, 1, fp)) != 1) {
		free(ptmp);
		status = S_sdr_noRead;
		errMessage(status,"dbRead: Error reading file");
		return(-1);
		break;
	    }
	    if ((adj_fun[sdrHeader.type]) == NULL) {
		status = S_sdr_noSdrType;
		errMessage(status,"dbRead: Warning: S_sdr_noSdrType not found");
		free(ptmp);
		return(-1);
		break;
	    }
	    /* pass the adjusting function the allocated ptr */
	    if (((*adj_fun[sdrHeader.type]) (ptmp, pdbbase)) < 0) {
		return(-1);
		break;
	    }
	    if(sdrHeader.type==SDR_ALLSUMS) {
		if(pdbbase->sdrFileSize==0) {
		    pdbbase->sdrFileSize = ftell(fp);
		}else {
		    if(pdbbase->sdrFileSize!=ftell(fp))
			return(S_sdr_sumError);
		}
	    }
	    if (job_type == LOAD_ONLY_ONE) {
		return(0);
		break;
	    }
    }
}

/*
 * ADJ_DBRECTYPE
 * 
 * Load and adjust SDR structure
 * 
 */
static int 
#ifdef __STDC__
adj_dbRecType(char *ptmp, DBBASE *pdbbase)
#else
adj_dbRecType(ptmp, pdbbase)
    char    *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct recType 	*precType = NULL;
    struct recHeader	*precHeader;
    int			i,size;

    if (pdbbase->precType == NULL) {
	precType = (struct recType *) ptmp;
	precType->papName = cvtOffsetToAddr(precType,precType->papName);
	for (i = 0; i < precType->number; i++) {
	    if (precType->papName[i] != NULL)
		precType->papName[i] = cvtOffsetToAddr(precType,
			precType->papName[i]);
	}
	pdbbase->precType = precType;
	size = sizeof(struct recHeader) + (precType->number * sizeof(char *));
	precHeader = dbCalloc(1,size);
	precHeader->papRecLoc = (struct recLoc **)(precHeader) + 1;
	precHeader->number = precType->number;
	pdbbase->precHeader = precHeader;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_DBRECORDS
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_dbRecords(char *ptmp,DBBASE *pdbbase)
#else
adj_dbRecords(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    DBENTRY         dbEntry;
    struct recType *precType = (struct recType *) pdbbase->precType;
    struct recDes  *precDes = (struct recDes *) pdbbase->precDes;
    /* set ptr to first of contiguous set of records in ptmp */
    char           *precord = (char *) ptmp + sizeof(struct recLoc);
    struct recLoc  *precLoc = (struct recLoc *) ptmp;
    long            no_records;
    int             found = 0;
    char	    text[120];
    long	    status;
    int		    i;

    if (precType == NULL) {
	status = S_sdr_notLoaded;
	errMessage(status,"dbRead: adj_dbRecords() recType == NULL");
	return (-1);
    }
    if (precDes == NULL) {
	status = S_sdr_notLoaded;
	errMessage(status,"dbRead: adj_dbRecords() recDes == NULL");
	return (-1);
    }
    dbInitEntry(pdbbase,&dbEntry);
    no_records = (sdrHeader.nbytes / precLoc->rec_size);
    for (i = 0; i < no_records; i++, precord += precLoc->rec_size) {
	if (*precord == '\0')
	    continue;
	found = 0;
	if ((dbFindRecord(&dbEntry,(char *) precord)) == 0) {
	    found = 1;
	}
	/* record found - does type match */
	if (found && (dbEntry.record_type != precLoc->record_type)) {
		sprintf(text,
		 "dbRead: adj_dbRecords: record (%s) skipped - wrong type ",
			 (char *) precord);
		errMessage(-1L, text);
	} else if (found && (dbEntry.record_type == precLoc->record_type)) {
	    /* copy precord to dbEntry.precord */
	    memcpy( (char *) dbEntry.precnode->precord, (char *) precord , precLoc->rec_size);
		sprintf(text,
		 "dbRead: adj_dbRecords: record (%s) overwritten ",
			 (char *) precord);
		errMessage(-1L, text);
	} else if (!found) {
	    dbEntry.record_type = precLoc->record_type;
	    if ((dbCreateRecord(&dbEntry,(char *) precord)) != 0) {
		sprintf(text, "dbRead: adj_dbRecords: dbCreateRecord  failed");
		errMessage(-1L, text);
	    } else {
	         memcpy((char*)dbEntry.precnode->precord, (char*)precord, precLoc->rec_size);
	    }
	}
    }
    free(ptmp);
    dbFinishEntry(&dbEntry);
    return 0;
}

/*
 * ADJ_DBRECDES
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_dbRecDes(char *ptmp, DBBASE *pdbbase)
#else
adj_dbRecDes(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct recDes 	*precDes = (struct recDes*)pdbbase->precDes;
    struct recHeader	*precHeader = (struct recHeader *) pdbbase->precHeader;
    char	    	text[120];
    long		status;
    int			i,j;

    if (pdbbase->precType == NULL) {
	status = S_sdr_notLoaded;
	sprintf(text, "dbRead: adj_dbRecDes() precType == NULL");
	errMessage(status, text);
	return (-1);
    }
    if (pdbbase->precDes == NULL) {
	precDes = (struct recDes *) ptmp;
	precDes->papRecTypDes = cvtOffsetToAddr(precDes,precDes->papRecTypDes);
	for (i = 0; i < precDes->number; i++) {
	    if (precDes->papRecTypDes[i] != NULL) {
		precDes->papRecTypDes[i] =
		   cvtOffsetToAddr(precDes,precDes->papRecTypDes[i]);
		precDes->papRecTypDes[i]->papFldDes =
		   cvtOffsetToAddr(precDes,precDes->papRecTypDes[i]->papFldDes);
		precDes->papRecTypDes[i]->sortFldName =
		   cvtOffsetToAddr(precDes,precDes->papRecTypDes[i]->sortFldName);
		precDes->papRecTypDes[i]->sortFldInd =
		   cvtOffsetToAddr(precDes,precDes->papRecTypDes[i]->sortFldInd);
		precDes->papRecTypDes[i]->link_ind =
		   cvtOffsetToAddr(precDes,precDes->papRecTypDes[i]->link_ind);
		for (j = 0; j < precDes->papRecTypDes[i]->no_fields; j++) {
		    if (precDes->papRecTypDes[i]->papFldDes != NULL) {
			if (precDes->papRecTypDes[i]->papFldDes[j] != NULL) {
			    precDes->papRecTypDes[i]->papFldDes[j] =
				cvtOffsetToAddr(precDes,precDes->papRecTypDes[i]->papFldDes[j]);
			}
		    }
		}
		precHeader->papRecLoc[i] = dbCalloc(1,sizeof(struct recLoc));
		precHeader->papRecLoc[i]->rec_size = precDes->papRecTypDes[i]->rec_size;
		precHeader->papRecLoc[i]->record_type = i;
	    }
	}
	pdbbase->precDes = precDes;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_CHOICEGBL
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_choiceGbl(char *ptmp, DBBASE *pdbbase)
#else
adj_choiceGbl(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct arrChoiceSet *pchoiceGbl = NULL;
    int			i,j;

    if (pdbbase->pchoiceGbl == NULL) {
	pchoiceGbl = (struct arrChoiceSet *) ptmp;
	pchoiceGbl->papChoiceSet = cvtOffsetToAddr(pchoiceGbl,pchoiceGbl->papChoiceSet);
	for (i = 0; i < pchoiceGbl->number; i++) {
	    pchoiceGbl->papChoiceSet[i] = cvtOffsetToAddr(pchoiceGbl,
		pchoiceGbl->papChoiceSet[i]);
	    if (pchoiceGbl->papChoiceSet[i] != NULL) {
		pchoiceGbl->papChoiceSet[i]->papChoice = cvtOffsetToAddr(
		    pchoiceGbl,pchoiceGbl->papChoiceSet[i]->papChoice);
		for (j = 0; j < pchoiceGbl->papChoiceSet[i]->number; j++) {
		    if (pchoiceGbl->papChoiceSet[i]->papChoice[j] != NULL) {
			pchoiceGbl->papChoiceSet[i]->papChoice[j] = cvtOffsetToAddr(
			    pchoiceGbl,pchoiceGbl->papChoiceSet[i]->papChoice[j]);
		    }
		}
	    }
	}
	pdbbase->pchoiceGbl = pchoiceGbl;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_CHOICECVT
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_choiceCvt(char *ptmp, DBBASE *pdbbase)
#else
adj_choiceCvt(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct choiceSet *pchoiceCvt = NULL;
    int		     i;

    if (pdbbase->pchoiceCvt == NULL) {
	pchoiceCvt = (struct choiceSet *) ptmp;
	pchoiceCvt->papChoice = cvtOffsetToAddr(pchoiceCvt,pchoiceCvt->papChoice);
	for (i = 0; i < pchoiceCvt->number; i++) {
	    if (pchoiceCvt->papChoice[i] != NULL) {
		pchoiceCvt->papChoice[i] = cvtOffsetToAddr(pchoiceCvt,
			pchoiceCvt->papChoice[i]);
	    }
	}
	pdbbase->pchoiceCvt = pchoiceCvt;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_CHOICEREC
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_choiceRec(char *ptmp, DBBASE *pdbbase)
#else
adj_choiceRec(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct choiceRec *pchoiceRec = NULL;
    struct recType 	*precType = pdbbase->precType;
    char	    	text[120];
    int			i,j,k;
    long		status;

    if (pdbbase->precType == NULL) {
	status = S_sdr_notLoaded;
	sprintf(text, "dbRead: adj_choiceRec() recType == NULL");
	errMessage(status, text);
	return (-1);
    }
    if (pdbbase->pchoiceRec == NULL) {
	pchoiceRec = (struct choiceRec *) ptmp;
	pchoiceRec->papArrChoiceSet = cvtOffsetToAddr(pchoiceRec,
		pchoiceRec->papArrChoiceSet);
	for (i = 0; i < precType->number; i++) {
	    if (pchoiceRec->papArrChoiceSet[i] != NULL) {
		pchoiceRec->papArrChoiceSet[i] = cvtOffsetToAddr(
		    pchoiceRec,pchoiceRec->papArrChoiceSet[i]);
		pchoiceRec->papArrChoiceSet[i]->papChoiceSet = cvtOffsetToAddr(
		    pchoiceRec,pchoiceRec->papArrChoiceSet[i]->papChoiceSet);
		for (j = 0; j < pchoiceRec->papArrChoiceSet[i]->number; j++) {
		    if (pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j] != NULL) {
			pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j] = cvtOffsetToAddr(
			    pchoiceRec,pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]);
			pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice =
			    cvtOffsetToAddr(pchoiceRec,
				pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice);
			for (k = 0; k < pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->number; k++) {
			    if (pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k] != NULL) {
				pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k] =
				    cvtOffsetToAddr(pchoiceRec,
				    pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k]);
			    }
			}
		    }
		}
	    }
	}
	pdbbase->pchoiceRec = pchoiceRec;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_CHOICEDEV
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_choiceDev(char *ptmp, DBBASE *pdbbase)
#else
adj_choiceDev(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct devChoiceRec *pchoiceDev = (struct devChoiceRec*)pdbbase->pchoiceDev;
    struct recType *precType = (struct recType*)pdbbase->precType;
    char	    text[120];
    long	    status;
    int		    i,j;

    if (pdbbase->precType == NULL) {
	status = S_sdr_notLoaded;
	sprintf(text, "dbRead: adj_choiceDev() precType == NULL");
	errMessage(status, text);
	return (-1);
    }
    if (pdbbase->pchoiceDev == NULL) {
	pchoiceDev = (struct devChoiceRec *) ptmp;
	pchoiceDev->papDevChoiceSet = cvtOffsetToAddr(pchoiceDev,pchoiceDev->papDevChoiceSet);
	for (i = 0; i < precType->number; i++) {
	    if (pchoiceDev->papDevChoiceSet[i] != NULL) {
		pchoiceDev->papDevChoiceSet[i] = cvtOffsetToAddr(pchoiceDev,
		    pchoiceDev->papDevChoiceSet[i]);
	 	pchoiceDev->papDevChoiceSet[i]->papDevChoice = cvtOffsetToAddr(pchoiceDev,
		    pchoiceDev->papDevChoiceSet[i]->papDevChoice);
		for (j = 0; j < pchoiceDev->papDevChoiceSet[i]->number; j++) {
		    if (pchoiceDev->papDevChoiceSet[i]->papDevChoice[j] != NULL) {
			pchoiceDev->papDevChoiceSet[i]->papDevChoice[j] =
			    cvtOffsetToAddr(pchoiceDev,
			    pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]);
			pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice =
			    cvtOffsetToAddr(pchoiceDev,
			    pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice);
		    }
		}
	    }
	}
	pdbbase->pchoiceDev = pchoiceDev;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_DEVSUP
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_devSup(char *ptmp, DBBASE *pdbbase)
#else
adj_devSup(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct recDevSup *precDevSup = (struct recDevSup*)pdbbase->precDevSup;
    struct recType *precType = (struct recType*)pdbbase->precType;
    char	    text[120];
    long	    status;
    int		    i,j;

    if (pdbbase->precType == NULL) {
	status = S_sdr_notLoaded;
	sprintf(text, "dbRead: adj_devSup() precType == NULL");
	errMessage(status, text);
	return (-1);
    }
    if (pdbbase->precDevSup == NULL) {
	precDevSup = (struct recDevSup *) ptmp;
	precDevSup->papDevSup = cvtOffsetToAddr(precDevSup,precDevSup->papDevSup);
	for (i = 0; i < precType->number; i++) {
	    if (precDevSup->papDevSup[i] != NULL) {
		precDevSup->papDevSup[i] = cvtOffsetToAddr(precDevSup,
			precDevSup->papDevSup[i]);
		precDevSup->papDevSup[i]->papDsetName = cvtOffsetToAddr(precDevSup,
			precDevSup->papDevSup[i]->papDsetName);
		precDevSup->papDevSup[i]->papDset = cvtOffsetToAddr(precDevSup,
			precDevSup->papDevSup[i]->papDset);
		for (j = 0; j < precDevSup->papDevSup[i]->number; j++) {
		    if (precDevSup->papDevSup[i]->papDsetName[j] != NULL) {
			precDevSup->papDevSup[i]->papDsetName[j] =
			    cvtOffsetToAddr(precDevSup,precDevSup->papDevSup[i]->papDsetName[j]);
		    }
		}
	    }
	}
	pdbbase->precDevSup = precDevSup;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_CVTTABLE
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_cvtTable(char *ptmp, DBBASE *pdbbase)
#else
adj_cvtTable(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct arrBrkTable *pcvtTable = (struct arrBrkTable*) pdbbase->pcvtTable;
    int			i,j;

    if (pdbbase->pcvtTable == NULL) {
	pcvtTable = (struct arrBrkTable *) ptmp;
	pcvtTable->papBrkTable = cvtOffsetToAddr(pcvtTable,pcvtTable->papBrkTable);
	for (i = 0; i < pcvtTable->number; i++) {
	    if (pcvtTable->papBrkTable[i] != NULL) {
		pcvtTable->papBrkTable[i] = cvtOffsetToAddr(pcvtTable,
		    pcvtTable->papBrkTable[i]);
		pcvtTable->papBrkTable[i]->papBrkInt = cvtOffsetToAddr(pcvtTable,
		    pcvtTable->papBrkTable[i]->papBrkInt);
		pcvtTable->papBrkTable[i]->name = cvtOffsetToAddr(pcvtTable,
		    pcvtTable->papBrkTable[i]->name);
		for (j = 0; j < pcvtTable->papBrkTable[i]->number; j++) {
		    pcvtTable->papBrkTable[i]->papBrkInt[j] = cvtOffsetToAddr(pcvtTable,
			pcvtTable->papBrkTable[i]->papBrkInt[j]);
		}
	    }
	}
	pdbbase->pcvtTable = pcvtTable;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_DRVSUP
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_drvSup(char *ptmp, DBBASE *pdbbase)
#else
adj_drvSup(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct drvSup *pdrvSup = (struct drvSup*)pdbbase->pdrvSup;
    int		  i;

    if (pdbbase->pdrvSup == NULL) {
	pdrvSup = (struct drvSup *) ptmp;
	pdrvSup->papDrvName = cvtOffsetToAddr(pdrvSup,pdrvSup->papDrvName);
	pdrvSup->papDrvet = cvtOffsetToAddr(pdrvSup,pdrvSup->papDrvet);
	for (i = 0; i < pdrvSup->number; i++) {
	    if (pdrvSup->papDrvName[i] != NULL)
		pdrvSup->papDrvName[i] = cvtOffsetToAddr(pdrvSup,pdrvSup->papDrvName[i]);
	}
	pdbbase->pdrvSup = pdrvSup;
    } else {
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_RECSUP
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_recSup(char *ptmp, DBBASE *pdbbase)
#else
adj_recSup(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{

/* NOTE: this function never entered - iocInit creates rset's */
    errMessage(S_sdr_noRecDef,"dbRead: adj_recSup() not valid ");
    return (-1);
}

/*
 * ADJ_SDRSUM
 *
 * Load SDR structure
 *
 */
static int
#ifdef __STDC__
adj_sdrSum(char *ptmp, DBBASE *pdbbase)
#else
adj_sdrSum(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    struct sdrSum  *psdrSum = (struct sdrSum*)pdbbase->psdrSum;

    if(psdrSum==NULL) {
	pdbbase->psdrSum = (struct sdrSum *) ptmp;
    } else {
	 struct sdrSum *pnewsdrSum = (struct sdrSum *)ptmp;

	if((strncmp(psdrSum->allSdrSums,pnewsdrSum->allSdrSums,
		strlen(psdrSum->allSdrSums)))!=0) return(-1);
	/* ignore - already loaded */
	free(ptmp);
    }
    return 0;
}

/*
 * ADJ_DBPVD
 *
 * Load and adjust SDR structure
 *
 */
static int
#ifdef __STDC__
adj_dbPvd(char *ptmp, DBBASE *pdbbase)
#else
adj_dbPvd(ptmp, pdbbase)
    char           *ptmp;
    DBBASE  *pdbbase;
#endif /*__STDC__*/
{
    /*  skip pvd if present - this function will eventually go away */
    free(ptmp);
    return 0;
}

/*
 * INITDBREAD
 *
 * Inititialize dbBase and precHeader
 *
 */
#ifdef __STDC__
static int initadj_fun(DBBASE *pdbbase)
#else
static int initadj_fun(pdbbase)
DBBASE *pdbbase;
#endif /*__STDC__*/
{
    int             i;

    /* clear the adj_fun array */
    for (i = 0; i < SDR_NTYPES; i++) {
	adj_fun[i] = 0;
    }
    /* init the adj func array */
    adj_fun[SDR_DB_RECTYPE] = adj_dbRecType;
    adj_fun[SDR_DB_RECORDS] = adj_dbRecords;
    adj_fun[SDR_DB_RECDES] = adj_dbRecDes;
    adj_fun[SDR_DB_PVD] = adj_dbPvd;
    adj_fun[SDR_CHOICEGBL] = adj_choiceGbl;
    adj_fun[SDR_CHOICECVT] = adj_choiceCvt;
    adj_fun[SDR_CHOICEREC] = adj_choiceRec;
    adj_fun[SDR_CHOICEDEV] = adj_choiceDev;
    adj_fun[SDR_DEVSUP] = adj_devSup;
    adj_fun[SDR_CVTTABLE] = adj_cvtTable;
    adj_fun[SDR_DRVSUP] = adj_drvSup;
    adj_fun[SDR_RECSUP] = adj_recSup;	/* Note: not an sdr type */
    adj_fun[SDR_ALLSUMS] = adj_sdrSum;
    return (0);
}

/*generate devChoiceSet.papChoice			*/
/* If papDevChoiceSet[i] is NULL then set DTYP to noprompt*/
#ifdef __STDC__
static void post_adj_devSup(DBBASE *pdbbase) 
#else
static void post_adj_devSup(pdbbase)
DBBASE *pdbbase;
#endif /*__STDC__*/
{
    struct devChoiceRec *pchoiceDev=pdbbase->pchoiceDev;
    struct devChoiceSet *pdevChoiceSet;
    int			rtype,i;
    struct  recDes	*precDes = pdbbase->precDes;
    struct recTypDes	*precTypDes;
    struct  fldDes	*pfldDes;

    if(!pchoiceDev) return;
    for(rtype=0; rtype<pchoiceDev->number; rtype++) {
	if((pdevChoiceSet = pchoiceDev->papDevChoiceSet[rtype])) {
	    pdevChoiceSet->papChoice =
		dbCalloc(1,(pdevChoiceSet->number * sizeof(char *)));
	    for(i=0; i<pdevChoiceSet->number; i++) {
		pdevChoiceSet->papChoice[i] =
			pdevChoiceSet->papDevChoice[i]->pchoice;
	    }
	} else {
	    if(precTypDes = precDes->papRecTypDes[rtype]) {
		for(i=0; i<precTypDes->no_fields; i++) {
		    if(!(pfldDes = precDes->papRecTypDes[rtype]->papFldDes[i])) continue;
		    if(strncmp(pfldDes->fldname,"DTYP",4)==0) {
			pfldDes->promptflag = FALSE;
			break;
		    }
		}
	    }
	}
    }
    return;
}

#ifdef __STDC__
long dbWrite(DBBASE *pdbbase,FILE *fpdctsdr,FILE *fp)
#else
long dbWrite(pdbbase,fpdctsdr,fp)
DBBASE *pdbbase;
FILE *fpdctsdr;
FILE *fp;
#endif /*__STDC__*/
{
    RECNODE        *precnode;
    ELLLIST           *preclist;
    int             no_entries;
    unsigned        rec_size;	/* set to record size */
    struct sdrHeader sdrHeader;
    struct recLoc  *precLoc;
    short           recType;
    struct recHeader *precHeader = pdbbase->precHeader;
    struct recTypDes *pRecTypDes;	/* set to record description struct for this type */
    struct recDes  *precDes = pdbbase->precDes;
    char           *iobuff;

    rewind(fpdctsdr);
    iobuff = dbCalloc(1,pdbbase->sdrFileSize);
    if (fread(iobuff, pdbbase->sdrFileSize, 1, fpdctsdr) != 1) {
	errMessage(-1,"dbWrite: incomplete read of dctsdr file \n");
	return (-1);
    }
    if ((fwrite(iobuff, (int) pdbbase->sdrFileSize, 1, fp)) != 1) {
	errMessage(-1,"dbWrite: incomplete write");
	return (-1);
    }
    free((void *)iobuff);
    /*write (append) the memory resident database to the tempfile*/
    for (recType = 0; recType < precHeader->number; recType++) {
	if ((pRecTypDes = GET_PRECTYPDES(precDes, recType)) == NULL) continue;
	if ((precLoc = GET_PRECLOC(precHeader, recType)) == NULL) continue;
	if((preclist = precLoc->preclist) == NULL) continue;
	/* set up and write the SDR_DB_RECORDS sdrHeader */
	no_entries = ellCount(preclist);
	rec_size = pRecTypDes->rec_size;
	memset((char *) &sdrHeader, '\0', sizeof(struct sdrHeader));
	sdrHeader.magic = DBMAGIC;
	sdrHeader.nbytes = no_entries * rec_size + sizeof(struct recLoc);
	sdrHeader.type = SDR_DB_RECORDS;
	sdrHeader.create_date = (long) 0;
	if ((fwrite((char*)&sdrHeader, (int) sizeof(struct sdrHeader), 1, fp)) != 1) {
	    errMessage(-1,"dbWrite: incomplete write");
	    return (-1);
	}
	if ((fwrite(precHeader->papRecLoc[recType],
		    (int) sizeof(struct recLoc), 1, fp)) != 1) {
	    errMessage(-1,"dbWrite: incomplete write");
	    return (-1);
	}
	for (precnode = (RECNODE *) ellFirst(preclist);
		precnode != NULL;
		precnode = (RECNODE *) ellNext((ELLNODE*)precnode)) {
	    /* write  each record of this type contiguous */
	    if ((fwrite(precnode->precord, (int) rec_size, 1, fp)) != 1) {
	        errMessage(-1,"dbWrite: incomplete write");
		return (-1);
	    }
	}
    }
    return (0);
}

/* Beginning of Process Variable Directory Routines*/
#define 	HASH_NO	512		/* number of hash table entries */


/*The hash algorithm is a modification of the algorithm described in	*/
/* Fast Hashing of Variable Length Text Strings, Peter K. Pearson,	*/
/* Communications of the ACM, June 1990					*/
/* The modifications were desdigned by Marty Kraimer and Ben Chin Cha	*/
/* The mods were implemented and tested by Ben Chin Cha			*/

static unsigned char T0[256] = {
 39,159,180,252, 71,  6, 13,164,232, 35,226,155, 98,120,154, 69,
157, 24,137, 29,147, 78,121, 85,112,  8,248,130, 55,117,190,160,
176,131,228, 64,211,106, 38, 27,140, 30, 88,210,227,104, 84, 77,
 75,107,169,138,195,184, 70, 90, 61,166,  7,244,165,108,219, 51,
  9,139,209, 40, 31,202, 58,179,116, 33,207,146, 76, 60,242,124,
254,197, 80,167,153,145,129,233,132, 48,246, 86,156,177, 36,187,
 45,  1, 96, 18, 19, 62,185,234, 99, 16,218, 95,128,224,123,253,
 42,109,  4,247, 72,  5,151,136,  0,152,148,127,204,133, 17, 14,
182,217, 54,199,119,174, 82, 57,215, 41,114,208,206,110,239, 23,
189, 15,  3, 22,188, 79,113,172, 28,  2,222, 21,251,225,237,105,
102, 32, 56,181,126, 83,230, 53,158, 52, 59,213,118,100, 67,142,
220,170,144,115,205, 26,125,168,249, 66,175, 97,255, 92,229, 91,
214,236,178,243, 46, 44,201,250,135,186,150,221,163,216,162, 43,
 11,101, 34, 37,194, 25, 50, 12, 87,198,173,240,193,171,143,231,
111,141,191,103, 74,245,223, 20,161,235,122, 63, 89,149, 73,238,
134, 68, 93,183,241, 81,196, 49,192, 65,212, 94,203, 10,200, 47 
};

static unsigned char T1[256] = {
  9,139,209, 40, 31,202, 58,179,116, 33,207,146, 76, 60,242,124,
254,197, 80,167,153,145,129,233,132, 48,246, 86,156,177, 36,187,
 45,  1, 96, 18, 19, 62,185,234, 99, 16,218, 95,128,224,123,253,
 42,109,  4,247, 72,  5,151,136,  0,152,148,127,204,133, 17, 14,
182,217, 54,199,119,174, 82, 57,215, 41,114,208,206,110,239, 23,
189, 15,  3, 22,188, 79,113,172, 28,  2,222, 21,251,225,237,105,
102, 32, 56,181,126, 83,230, 53,158, 52, 59,213,118,100, 67,142,
220,170,144,115,205, 26,125,168,249, 66,175, 97,255, 92,229, 91,
214,236,178,243, 46, 44,201,250,135,186,150,221,163,216,162, 43,
 11,101, 34, 37,194, 25, 50, 12, 87,198,173,240,193,171,143,231,
111,141,191,103, 74,245,223, 20,161,235,122, 63, 89,149, 73,238,
134, 68, 93,183,241, 81,196, 49,192, 65,212, 94,203, 10,200, 47,
 39,159,180,252, 71,  6, 13,164,232, 35,226,155, 98,120,154, 69,
157, 24,137, 29,147, 78,121, 85,112,  8,248,130, 55,117,190,160,
176,131,228, 64,211,106, 38, 27,140, 30, 88,210,227,104, 84, 77,
 75,107,169,138,195,184, 70, 90, 61,166,  7,244,165,108,219, 51,
};

#ifdef __STDC__
static unsigned short hash( char *pname, int length)
#else
static unsigned short hash( pname, length)
char *pname;
int length;
#endif /*__STDC__*/
{
    unsigned short h=0;
    unsigned short hret;
    unsigned char  *h0=(unsigned char *)&h;
    unsigned char  *h1= h0 + 1;
    int		i;

    for(i=0; i<length; i+=2, pname+=2) {
	*h0 = T0[*h0 ^ *pname];
	*h1 = T1[*h1 ^ *(pname+1)];
    }
    hret = *h0;
    return(hret + *h1);
}

#ifdef __STDC__
void    dbPvdInitPvt(DBBASE *pdbbase)
#else
void    dbPvdInitPvt(pdbbase)
DBBASE *pdbbase;
#endif /*__STDC__*/
{
    ELLLIST **ppvd;

    ppvd = dbCalloc(HASH_NO, sizeof(ELLLIST *));
    pdbbase->ppvd = (void *) ppvd;
    return;
}

#ifdef __STDC__
PVDENTRY *dbPvdFind(DBBASE *pdbbase,char *name,int lenName)
#else
PVDENTRY *dbPvdFind(pdbbase,name,lenName)
DBBASE *pdbbase;
char *name;
int lenName;
#endif /*__STDC__*/
{
    unsigned short  hashInd;
    ELLLIST          **ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST           *pvdlist;
    PVDENTRY       *ppvdNode;
    
    hashInd = hash(name, lenName);
    if ((pvdlist=ppvd[hashInd]) == NULL) return (NULL);
    ppvdNode = (PVDENTRY *) ellFirst(pvdlist);
    while(ppvdNode) {
	if(strcmp(name,(char *)ppvdNode->precnode->precord) == 0)
		return(ppvdNode);
	ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
    }
    return (NULL);
}

#ifdef __STDC__
PVDENTRY *dbPvdAdd(DBBASE *pdbbase,unsigned short record_type,RECNODE *precnode)
#else
PVDENTRY *dbPvdAdd(pdbbase,record_type,precnode)
DBBASE *pdbbase;
unsigned short record_type;
RECNODE *precnode;
#endif /*__STDC__*/
{
    unsigned short  hashInd;
    ELLLIST          **ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST           *ppvdlist;
    PVDENTRY       *ppvdNode;
    int		lenName;
    char	*name=(char *)precnode->precord;
    
    lenName=strlen(name);
    hashInd = hash(name, lenName);
    if (ppvd[hashInd] == NULL) {
	ppvd[hashInd] = dbCalloc(1, sizeof(ELLLIST));
	ellInit(ppvd[hashInd]);
    }
    ppvdlist=ppvd[hashInd];
    ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
    while(ppvdNode) {
	if(strcmp(name,(char *)ppvdNode->precnode->precord) == 0) return(NULL);
	ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
    }
    ppvdNode = dbCalloc(1, sizeof(PVDENTRY));
    ellAdd(ppvdlist, (ELLNODE*)ppvdNode);
    ppvdNode->record_type = record_type;
    ppvdNode->precnode = precnode;
    return (ppvdNode);
}

#ifdef __STDC__
void dbPvdDelete(DBBASE *pdbbase,RECNODE *precnode)
#else
void dbPvdDelete(pdbbase,precnode)
DBBASE *pdbbase;
RECNODE *precnode;
#endif /*__STDC__*/
{
    char	*name=(char *)precnode->precord;
    unsigned short  hashInd;
    ELLLIST        **ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST        *ppvdlist;
    PVDENTRY    *ppvdNode;
    int		lenName;
    
    lenName=strlen(name);
    hashInd = hash(name, lenName);
    if (ppvd[hashInd] == NULL)return;
    ppvdlist=ppvd[hashInd];
    ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
    while(ppvdNode) {
	if(strcmp(name,(char *)ppvdNode->precnode->precord) == 0) {
	    ellDelete(ppvdlist, (ELLNODE*)ppvdNode);
	    free((void *)ppvdNode);
	    return;
	}
	ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
    }
    return;
}

#ifdef __STDC__
void dbPvdFreeMem(DBBASE *pdbbase)
#else
void dbPvdFreeMem(pdbbase)
DBBASE *pdbbase;
#endif /*__STDC__*/
{
    unsigned short  hashInd;
    ELLLIST          **ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST           *ppvdlist;
    PVDENTRY       *ppvdNode;
    PVDENTRY       *next;
    
    if (ppvd == NULL) return;
    for (hashInd=0; hashInd<HASH_NO; hashInd++) {
	if(ppvd[hashInd] == NULL) continue;
	ppvdlist=ppvd[hashInd];
	ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
	while(ppvdNode) {
	    next = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
	    ellDelete(ppvdlist,(ELLNODE*)ppvdNode);
	    free((void *)ppvdNode);
	    ppvdNode = next;
	}
	free((void *)ppvd[hashInd]);
    }
    free((void *)ppvd);
}

#ifdef __STDC__
void dbDumpPvd(DBBASE *pdbbase)
#else
void dbDumpPvd(pdbbase)
DBBASE *pdbbase;
#endif /*__STDC__*/
{
    unsigned short  hashInd;
    ELLLIST          **ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST           *ppvdlist;
    PVDENTRY       *ppvdNode;
    int		number;
    
    if (ppvd == NULL) return;
    printf("Process Variable Directory\n");
    for (hashInd=0; hashInd<HASH_NO; hashInd++) {
	if(ppvd[hashInd] == NULL) continue;
	ppvdlist=ppvd[hashInd];
	ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
	printf(" %3.3hd=%3.3d\n",hashInd,ellCount(ppvdlist));
	number=0;
	while(ppvdNode) {
	    printf(" %s",(char *)ppvdNode->precnode->precord);
	    if(number++ ==2) {number=0;printf("\n        ");}
	    ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
	}
    }
    printf("\nEnd of Process Variable Directory\n");
}
