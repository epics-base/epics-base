/*dbStaticLib.c*/
/* share/src/db @(#)dbStaticLib.c	1.21     7/11/94 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 * Modification Log:
 * -----------------
 * .01	02-03-93	mrk	Consolidated all databse defs in one place
 * .02	09-10-93	mrk	dbIsDefault always returns FALSE for DEVCHOICE
 * .03	02-23-94	mrk	dbPutString to DEV_CHOICE. Ok if no INP or OUT
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

#define DBFLDTYPES_GBLSOURCE
#define GUIGROUPS_GBLSOURCE
#define SPECIAL_GBLSOURCE
#include <dbDefs.h>
#define LINK_GBLSOURCE
#include <link.h>
#undef LINK_GBLSOURCE
#include <dbFldTypes.h>
#include <epicsPrint.h>
#include <errMdef.h>
#include <ellLib.h>
#include <cvtFast.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <devSup.h>
#include <drvSup.h>
#include <special.h>
#include <gpHash.h>
#include <guigroup.h>
#include <special.h>

int dbStaticDebug = 0;
#define messagesize	100
#define RPCL_LEN 184
#define MAX_FIELD_NAME_LENGTH 20
long postfix(char *pinfix, char *ppostfix,short *perror);

static char *ppstring[5]={"NPP","PP","CA","CP","CPP"};
static char *msstring[2]={"NMS","MS"};

static int mapDBFtoDCT[DBF_NOACCESS+1] = {
	DCT_STRING,
	DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,
	DCT_REAL,DCT_REAL,
	DCT_INTEGER,
	DCT_MENU,
	DCT_MENUFORM,
	DCT_INLINK,DCT_OUTLINK,DCT_FWDLINK,
	DCT_NOACCESS};

struct form {
	DBLINK		*plink;
	int		linkType;
	int		nlines;
	char		**prompt;
	char		**value;
	char		**verify;
};

static char *promptCONSTANT[] = {
	"Constant:"};
static char *promptINLINK[] = {
	"         PV Name:",
	"NPP PP CA CP CPP:",
	"       NMS or MS:"};
static char *promptOUTLINK[] = {
	"  PV Name:",
	"NPP PP CA:",
	"NMS or MS:"};
static char *promptFWDLINK[] = {
	"  PV Name:",
	"<null> CA:"};
static char *promptVME_IO[] = {
	"  card:",
	"signal:",
	"  parm:"};
static char *promptCAMAC_IO[] = {
	"    branch:",
	"     crate:",
	"   station:",
	"subaddress:",
	"  function:",
	" parameter:"};
static char *promptAB_IO[] = {
	"    link:",
	" adapter:",
	"    card:",
	"  signal:",
	"    parm:"};
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
static char *promptRF_IO[] = {
	"      cryo:",
	"     micro:",
	"   dataset:",
	"   element:"};
static char *promptVXI_IO[] = {
	"     Dynamic?",
	"DYN    frame:",
	"DYN     slot:",
	"STATIC    la:",
	"      Signal:",
	"        parm:"};

/*Each DBF link type is separate case*/
#define FORM_CONSTANT	0
#define FORM_INLINK	1
#define FORM_OUTLINK	2
#define FORM_FWDLINK	3
#define FORM_VME_IO	4
#define FORM_CAMAC_IO	5
#define FORM_AB_IO	6
#define FORM_GPIB_IO	7
#define FORM_BITBUS_IO	8
#define FORM_INST_IO	9
#define FORM_BBGPIB_IO	10
#define FORM_RF_IO	11
#define FORM_VXI_IO	12
#define FORM_NTYPES	(FORM_VXI_IO + 1)

static char **promptAddr[FORM_NTYPES] = {
promptCONSTANT,
promptINLINK,
promptOUTLINK,
promptFWDLINK,
promptVME_IO,
promptCAMAC_IO,
promptAB_IO,
promptGPIB_IO,
promptBITBUS_IO,
promptINST_IO,
promptBBGPIB_IO,
promptRF_IO,
promptVXI_IO};

static int formlines[FORM_NTYPES] = {
sizeof(promptCONSTANT)/sizeof(char *),
sizeof(promptINLINK)/sizeof(char *),
sizeof(promptOUTLINK)/sizeof(char *),
sizeof(promptFWDLINK)/sizeof(char *),
sizeof(promptVME_IO)/sizeof(char *),
sizeof(promptCAMAC_IO)/sizeof(char *),
sizeof(promptAB_IO)/sizeof(char *),
sizeof(promptGPIB_IO)/sizeof(char *),
sizeof(promptBITBUS_IO)/sizeof(char *),
sizeof(promptINST_IO)/sizeof(char *),
sizeof(promptBBGPIB_IO)/sizeof(char *),
sizeof(promptRF_IO)/sizeof(char *),
sizeof(promptVXI_IO)/sizeof(char *)};

/*forward references for private routines*/
static long checkDevChoice(DBENTRY *pcheck, long link_type);
static long putParmString(char **pparm,char *pstring);
static long mapLINKTtoFORMT(DBLINK *plink,dbFldDes *pflddes,int *ind);
static void entryErrMessage(DBENTRY *pdbentry,long status,char *mess);
static void zeroDbentry(DBENTRY *pdbentry);
static char *getpMessage(DBENTRY *pdbentry);
static long putPvLink(DBENTRY *pdbentry,short pvlMask,char *pvname);

/*Following are obsolete. Will go away next release*/
long dbRead(DBBASE *pdbbase,FILE *fp)
{
    return(dbReadDatabaseFP(&pdbbase,fp,0,0));
}
long dbWrite(DBBASE *pdbbase,FILE *fpdctsdr,FILE *fp)
{
    fprintf(stderr,"dbWrite obsolete. It does NOTHING\n");
    return(-1);
}
long dbFindRecdes(DBENTRY *pdbentry,char *recdesname)
	{return dbFindRecordType(pdbentry,recdesname);}
long dbFirstRecdes(DBENTRY *pdbentry)
	{return dbFirstRecordType(pdbentry);}
long dbNextRecdes(DBENTRY *pdbentry)
	{return dbNextRecordType(pdbentry);}
char *dbGetRecdesName(DBENTRY *pdbentry)
	{return dbGetRecordTypeName(pdbentry);}
int  dbGetNRecdes(DBENTRY *pdbentry)
	{return dbGetNRecordTypes(pdbentry);}
long dbFirstFielddes(DBENTRY *pdbentry,int dctonly)
	{return dbFirstField(pdbentry,dctonly);}
long dbNextFielddes(DBENTRY *pdbentry,int dctonly)
	{return dbNextField(pdbentry,dctonly);}
char **dbGetChoices(DBENTRY *pdbentry)
	{return dbGetMenuChoices(pdbentry);}
void dbDumpRecDes(DBBASE *pdbbase,char *recordTypeName)
	{dbDumpRecordType(pdbbase,recordTypeName);}

/* internal routines*/
void copyEntry(DBENTRY *pfrom,DBENTRY *pto)
{
    *pto = *pfrom;
    pto->message = NULL;
    pto->formpvt = NULL;
}

/*checkDevChoice initializes INP or OUT field*/
static char *pNullString = "";
static long checkDevChoice(DBENTRY *pcheck, long link_type)
{
    DBENTRY	dbEntry;
    DBENTRY	*pdbentry= &dbEntry;
    dbFldDes	*pflddes;
    DBLINK	*plink;
    long	status=0;

    copyEntry(pcheck,pdbentry);
    status = dbFindField(pdbentry,"INP");
    if(status) status = dbFindField(pdbentry,"OUT");
    if(!status) {
	pflddes = pdbentry->pflddes;
	plink = (DBLINK *)(pdbentry->pfield);
	if(plink->type == link_type) goto done;
	if(link_type==CONSTANT && plink->type==PV_LINK)goto done;
	if(link_type==PV_LINK && plink->type==CONSTANT)goto done;
	dbFreeLinkContents(plink);
	plink->type = link_type;
	switch(plink->type) {
	    case VME_IO: plink->value.vmeio.parm = pNullString; break;
	    case CAMAC_IO: plink->value.camacio.parm = pNullString; break;
	    case AB_IO: plink->value.abio.parm = pNullString; break;
	    case GPIB_IO: plink->value.gpibio.parm = pNullString; break;
	    case BITBUS_IO: plink->value.bitbusio.parm = pNullString; break;
	    case INST_IO: plink->value.instio.string = pNullString; break;
	    case BBGPIB_IO: plink->value.bbgpibio.parm = pNullString; break;
	    case VXI_IO: plink->value.vxiio.parm = pNullString; break;
	}
    } else {
	if(link_type==CONSTANT) status = 0;
	else status = S_dbLib_badField;
    }
done:
    dbFinishEntry(pdbentry);
    return(status);
}

static long putParmString(char **pparm,char *pstring)
{
    size_t	size;

    if(*pparm && (*pparm != pNullString)){
	free((void *)(*pparm));
	*pparm = NULL;
    }
    if(!pstring) return(0);
    if(!(pstring = strchr(pstring,'@'))) return(0);
    pstring++;
    size = strlen(pstring) + 1;
    if(size==1) return(0);
    *pparm = dbCalloc(size, sizeof(char *));
    strcpy(*pparm,pstring);
    return(0);
}

void dbFreeLinkContents(struct link *plink)
{ 
    char *parm = NULL;

    switch(plink->type) {
	case CONSTANT: free((void *)plink->value.constantStr); break;
	case PV_LINK: free((void *)plink->value.pv_link.pvname); break;
	case VME_IO: parm = plink->value.vmeio.parm; break;
	case CAMAC_IO: parm = plink->value.camacio.parm; break;
	case AB_IO: parm = plink->value.abio.parm; break;
	case GPIB_IO: parm = plink->value.gpibio.parm; break;
	case BITBUS_IO: parm = plink->value.bitbusio.parm;break;
	case INST_IO: parm = plink->value.instio.string; break;
	case BBGPIB_IO: parm = plink->value.bbgpibio.parm;break;
	case VXI_IO: parm = plink->value.vxiio.parm; break;
   	default:
	     epicsPrintf("dbFreeLink called but link type unknown\n");
    }
    if(parm && (parm != pNullString)) free((void *)parm);
    memset((char *)plink,0,sizeof(struct link));
}

void dbFreePath(DBBASE *pdbbase)
{
    ELLLIST	*ppathList;
    dbPathNode	*pdbPathNode;

    if(!pdbbase) return;
    ppathList = (ELLLIST *)pdbbase->pathPvt;
    if(!ppathList) return;
    while(pdbPathNode = (dbPathNode *)ellFirst(ppathList)) {
	ellDelete(ppathList,&pdbPathNode->node);
	free((void *)pdbPathNode->directory);
	free((void *)pdbPathNode);
    }
    free((void *)ppathList);
    pdbbase->pathPvt = 0;
    return;
}

#define INC_SIZE	256
void dbCatString(char **string,int *stringLength,char *new,
	char *separator)
{
    if((*string==NULL)
    || ((strlen(*string)+strlen(new)+2) > (size_t)*stringLength)) {
	char	*newString;
	size_t	size;

	size = strlen(new);
	if(*string) size += strlen(*string);
	/*Make size multiple of INC_SIZE*/
	size = ((size + 2 + INC_SIZE)/INC_SIZE) * INC_SIZE;
	newString = dbCalloc(size,sizeof(char));
	if(*string) {
	    strcpy(newString,*string);
	    free((void *)(*string));
	}
	*string = newString;
    }
    if(*stringLength>0) {
	strcat(*string,separator);
	*stringLength += strlen(separator);
    }
    strcat(*string,new);
    *stringLength += strlen(new);
}

static long mapLINKTtoFORMT(DBLINK *plink,dbFldDes *pflddes,int *ind)
{
    switch(plink->type) {
    case CONSTANT:
	*ind = FORM_CONSTANT; return(0);
    case PV_LINK:
	switch(pflddes->field_type) {
	case DBF_INLINK:
	    *ind = FORM_INLINK; return(0);
	case DBF_OUTLINK:
	    *ind = FORM_OUTLINK; return(0);
	case DBF_FWDLINK:
	    *ind = FORM_FWDLINK; return(0);
	default:
	    break;
	}
	break;
    case VME_IO:
	*ind = FORM_VME_IO; return(0);
    case CAMAC_IO:
	*ind = FORM_CAMAC_IO; return(0);
    case AB_IO:
	*ind = FORM_AB_IO; return(0);
    case GPIB_IO:
	*ind = FORM_GPIB_IO; return(0);
    case BITBUS_IO:
	*ind = FORM_BITBUS_IO; return(0);
    case INST_IO:
	*ind = FORM_INST_IO; return(0);
    case BBGPIB_IO:
	*ind = FORM_BBGPIB_IO; return(0);
    case RF_IO:
	*ind = FORM_RF_IO; return(0);
    case VXI_IO:
	*ind = FORM_VXI_IO; return(0);
    default:
	break;
    }
    return(S_dbLib_badLink);
}

static void entryErrMessage(DBENTRY *pdbentry,long status,char *mess)
{
    char		message[200];
    char		*pmessage=&message[0];
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes 		*pflddes = pdbentry->pflddes;
    char		*pname = NULL;

    *pmessage=0;
    if(pdbentry->precordType) pname = pdbentry->precordType->name;
    if(pname) {
	strcat(pmessage,"RecordType:");
	strcat(pmessage,pname);
    }
    if(precnode){
	strcat(pmessage," Record:");
	strcat(pmessage,(char *)precnode->precord);
    }
    if(pflddes) {
	char *pstr=pflddes->name;

	strcat(pmessage," Field:");
	strcat(pmessage,pstr);
    }
    strcat(pmessage,"\n");
    strcat(pmessage,mess);
    errMessage(status,pmessage);
}

static void zeroDbentry(DBENTRY *pdbentry)
{
    /*NOTE that pdbbase, message, and formpvt MUST NOT be set to NULL*/
    pdbentry->precordType=NULL;
    pdbentry->pflddes=NULL;
    pdbentry->precnode=NULL;
    pdbentry->pfield=NULL;
    pdbentry->indfield=0;
}

static char *getpMessage(DBENTRY *pdbentry)
{
    if(!pdbentry->message) pdbentry->message = dbCalloc(1,messagesize);
    return(pdbentry->message);
}

static long putPvLink(DBENTRY *pdbentry,short pvlMask,char *pvname)
{
    dbFldDes	*pflddes;
    DBLINK	*plink;
    char	*pname;
    
    dbGetFieldAddress(pdbentry);
    pflddes = pdbentry->pflddes;
    if(!pflddes) return(-1);
    plink = (DBLINK *)pdbentry->pfield;
    if(!plink) return(-1);
    switch (pflddes->field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	if(plink->type != PV_LINK) return(S_dbLib_badLink);
	pname = plink->value.pv_link.pvname;
	if(pname) free((void *)pname);
	pname = dbCalloc(strlen(pvname)+1,sizeof(char));
	plink->value.pv_link.pvname = pname;
	strcpy(pname,pvname);
	plink->value.pv_link.pvlMask = pvlMask;
	return(0);
    default:
	errPrintf(-1,__FILE__, __LINE__,"Logic Error\n");
    }
    return(S_dbLib_badLink);
}

/*Public only for dbStaticNoRun*/
dbDeviceMenu *dbGetDeviceMenu(DBENTRY *pdbentry)
{
    dbRecordType	*precordType = pdbentry->precordType;
    dbFldDes	*pflddes = pdbentry->pflddes;
    dbDeviceMenu *pdbDeviceMenu;
    devSup	*pdevSup;
    int		ind;
    int		nChoice;

    if(!precordType) return(NULL);
    if(!pflddes) return(NULL);
    if(pflddes->field_type!=DBF_DEVICE) return(NULL);
    if(pflddes->ftPvt) return((dbDeviceMenu *)pflddes->ftPvt);
    nChoice = ellCount(&precordType->devList);
    if(nChoice <= 0) return(NULL);
    pdbDeviceMenu = dbCalloc(1,sizeof(dbDeviceMenu));
    pdbDeviceMenu->nChoice = nChoice;
    pdbDeviceMenu->papChoice = dbCalloc(pdbDeviceMenu->nChoice,sizeof(char *));
    pdevSup = (devSup *)ellFirst(&precordType->devList);
    ind = 0;
    while(pdevSup) {
	pdbDeviceMenu->papChoice[ind] = pdevSup->choice;
	ind++;
	pdevSup = (devSup *)ellNext(&pdevSup->node);
    }
    pflddes->ftPvt = pdbDeviceMenu;
    return(pdbDeviceMenu);
}

/* Beginning of Public Routines */
void *dbCalloc(size_t nobj,size_t size)
{
    void *p;

    p=calloc(nobj,size);
    if(p) return(p);
    printf("dbCalloc: Can't allocate memory\n");
#ifdef vxWorks
    taskSuspend(0);
#else
    abort();
#endif
    return(NULL);
}
void *dbMalloc(size_t size)
{
    void *p;

    p=malloc(size);
    if(p) return(p);
    printf("dbMalloc: Can't allocate memory\n");
#ifdef vxWorks
    taskSuspend(0);
#else
    abort();
#endif
    return(NULL);
}

dbBase *dbAllocBase(void)
{
    dbBase	*pdbbase;

    pdbbase = dbCalloc(1,sizeof(dbBase));
    ellInit(&pdbbase->menuList);
    ellInit(&pdbbase->recordTypeList);
    ellInit(&pdbbase->drvList);
    ellInit(&pdbbase->bptList);
    gphInitPvt(&pdbbase->pgpHash,256);
    dbPvdInitPvt(pdbbase);
    return (pdbbase);
}

void dbFreeBase(dbBase *pdbbase)
{
    dbMenu	*pdbMenu;
    dbMenu	*pdbMenuNext;
    dbRecordType	*pdbRecordType;
    dbRecordType	*pdbRecordTypeNext;
    dbFldDes	*pdbFldDes;
    dbRecordNode *pdbRecordNode;
    dbRecordNode *pdbRecordNodeNext;
    devSup	*pdevSup;
    devSup	*pdevSupNext;
    drvSup	*pdrvSup;
    drvSup	*pdrvSupNext;
    brkTable	*pbrkTable;
    brkTable	*pbrkTableNext;
    int		i;
    DBENTRY	dbentry;
    

    dbInitEntry(pdbbase,&dbentry);
    pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    while(pdbRecordType) {
	pdbRecordNode = (dbRecordNode *)ellFirst(&pdbRecordType->recList);
	while(pdbRecordNode) {
	    pdbRecordNodeNext = (dbRecordNode *)ellNext(&pdbRecordNode->node);
	    if(!dbFindRecord(&dbentry,pdbRecordNode->recordname))
		dbDeleteRecord(&dbentry);
	    pdbRecordNode = pdbRecordNodeNext;
	}
	pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node);
    }
    dbFinishEntry(&dbentry);
    pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    while(pdbRecordType) {
	for(i=0; i<pdbRecordType->no_fields; i++) {
	    pdbFldDes = pdbRecordType->papFldDes[i];
	    free((void *)pdbFldDes->prompt);
	    free((void *)pdbFldDes->name);
	    free((void *)pdbFldDes->extra);
	    free((void *)pdbFldDes->initial);
	    if(pdbFldDes->field_type==DBF_DEVICE && pdbFldDes->ftPvt) {
		dbDeviceMenu *pdbDeviceMenu;
		
		pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt;
		free((void *)pdbDeviceMenu->papChoice);
		free((void *)pdbDeviceMenu);
		pdbFldDes->ftPvt=0;
	    }
	    free((void *)pdbFldDes);
	}
	pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
	while(pdevSup) {
	    pdevSupNext = (devSup *)ellNext(&pdevSup->node);
	    ellDelete(&pdbRecordType->devList,&pdevSup->node);
	    free((void *)pdevSup->name);
	    free((void *)pdevSup->choice);
	    free((void *)pdevSup);
	    pdevSup = pdevSupNext;
	}
	pdbRecordTypeNext = (dbRecordType *)ellNext(&pdbRecordType->node);
	gphDelete(pdbbase->pgpHash,pdbRecordType->name,&pdbbase->recordTypeList);
	ellDelete(&pdbbase->recordTypeList,&pdbRecordType->node);
	free((void *)pdbRecordType->name);
	free((void *)pdbRecordType->link_ind);
	free((void *)pdbRecordType->papsortFldName);
	free((void *)pdbRecordType->sortFldInd);
	free((void *)pdbRecordType->papFldDes);
	free((void *)pdbRecordType);
	pdbRecordType = pdbRecordTypeNext;
    }
    pdbMenu = (dbMenu *)ellFirst(&pdbbase->menuList);
    while(pdbMenu) {
	pdbMenuNext = (dbMenu *)ellNext(&pdbMenu->node);
	gphDelete(pdbbase->pgpHash,pdbMenu->name,&pdbbase->menuList);
	ellDelete(&pdbbase->menuList,&pdbMenu->node);
	for(i=0; i< pdbMenu->nChoice; i++) {
	    free((void *)pdbMenu->papChoiceName[i]);
	    free((void *)pdbMenu->papChoiceValue[i]);
	}
	free((void *)pdbMenu->papChoiceName);
	free((void *)pdbMenu->papChoiceValue);
	free((void *)pdbMenu ->name);
	free((void *)pdbMenu);
	pdbMenu = pdbMenuNext;
    }
    pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList);
    while(pdrvSup) {
	pdrvSupNext = (drvSup *)ellNext(&pdrvSup->node);
	ellDelete(&pdbbase->drvList,&pdrvSup->node);
	free((void *)pdrvSup->name);
	free((void *)pdrvSup);
	pdrvSup = pdrvSupNext;
    }
    pbrkTable = (brkTable *)ellFirst(&pdbbase->bptList);
    while(pbrkTable) {
	pbrkTableNext = (brkTable *)ellNext(&pbrkTable->node);
	gphDelete(pdbbase->pgpHash,pbrkTable->name,&pdbbase->bptList);
	ellDelete(&pdbbase->bptList,&pbrkTable->node);
	free(pbrkTable->name);
	for(i=0; i<pbrkTable->number; i++) 
	    free((void *)(pbrkTable->papBrkInt[i]));
	free((void *)pbrkTable);
	pbrkTable = pbrkTableNext;
    }
    gphFreeMem(pdbbase->pgpHash);
    dbPvdFreeMem(pdbbase);
    dbFreePath(pdbbase);
    free((void *)pdbbase);
    return;
}

DBENTRY *dbAllocEntry(dbBase *pdbbase)
{
    DBENTRY *pdbentry;

    pdbentry = dbCalloc(1,sizeof(DBENTRY));
    pdbentry->pdbbase = pdbbase;
    return(pdbentry);
}

void dbFreeEntry(DBENTRY *pdbentry)
{
    if(pdbentry->message) free((void *)pdbentry->message);
    if(pdbentry->formpvt) dbFreeForm(pdbentry);
    free((void *)pdbentry);
}

void dbInitEntry(dbBase *pdbbase,DBENTRY *pdbentry)
{
    memset((char *)pdbentry,'\0',sizeof(DBENTRY));
    pdbentry->pdbbase = pdbbase;
}

void dbFinishEntry(DBENTRY *pdbentry)
{
    if(pdbentry->message) {
	free((void *)pdbentry->message);
	pdbentry->message = NULL;
    }
    if(pdbentry->formpvt) dbFreeForm(pdbentry);
}

DBENTRY *dbCopyEntry(DBENTRY *pdbentry)
{
    DBENTRY *pnew;

    pnew = dbAllocEntry(pdbentry->pdbbase);
    *pnew = *pdbentry;
    pnew->message = NULL;
    pnew->formpvt = NULL;
    return(pnew);
}

long dbPath(DBBASE *pdbbase,const char *path)
{
    if(!pdbbase) return(-1);
    dbFreePath(pdbbase);
    return(dbAddPath(pdbbase,path));
}

long dbAddPath(DBBASE *pdbbase,const char *path)
{
    ELLLIST	*ppathList;
    dbPathNode	*pdbPathNode;
    const char	*pcolon;
    const char	*pdir;
    int		len;

    if(!pdbbase) return(-1);
    ppathList = (ELLLIST *)pdbbase->pathPvt;
    if(!ppathList) {
	ppathList= dbCalloc(1,sizeof(ELLLIST));
	ellInit(ppathList);
	pdbbase->pathPvt = (void *)ppathList;
    }
    pdir = path;
    while(pdir) {
	if(*pdir == ':') {
	    pdir++;
	    continue;
	}
	pdbPathNode = (dbPathNode *)dbCalloc(1,sizeof(dbPathNode));
	ellAdd(ppathList,&pdbPathNode->node);
	pcolon = strchr(pdir,':');
	len = (pcolon ? (pcolon - pdir) : strlen(pdir));
	pdbPathNode->directory = (char *)dbCalloc(1,len + 1);
	strncpy(pdbPathNode->directory,pdir,len);
	pdir = (pcolon ? (pcolon+1) : 0);
    }
    return(0);
}

long dbWriteRecord(DBBASE *ppdbbase,const char *filename,
	char *precordTypename,int level)
{
    FILE	*outFile;

    outFile = fopen(filename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",filename);
	return(-1);
    }
    dbWriteRecordFP(ppdbbase,outFile,precordTypename,level);
    return(fclose(outFile));
}

long dbWriteRecordFP(DBBASE *pdbbase,FILE *fp,char *precordTypename,int level)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    int		dctonly;

    dctonly = ((level>1) ? FALSE : TRUE);
    dbInitEntry(pdbbase,pdbentry);
    if(!precordTypename) {
	status = dbFirstRecordType(pdbentry);
	if(status) {
	    fprintf(stderr,"dbWriteRecordFP: No record descriptions\n");
	    return(status);
	}
    } else {
	status = dbFindRecordType(pdbentry,precordTypename);
	if(status) {
	    fprintf(stderr,"dbWriteRecordFP: No record description for %s\n",
		precordTypename);
	    return(status);
	}
    }
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
		if(dbIsVisibleRecord(pdbentry))
	    	fprintf(fp,"grecord(%s,\"%s\") {\n",
				dbGetRecordTypeName(pdbentry),dbGetRecordName(pdbentry));
	    else
	    	fprintf(fp,"record(%s,\"%s\") {\n",
				dbGetRecordTypeName(pdbentry),dbGetRecordName(pdbentry));
	    status = dbFirstField(pdbentry,dctonly);
	    while(!status) {
		if(!dbIsDefaultValue(pdbentry) || level>0) {
		    fprintf(fp,"\tfield(%s,\"%s\")\n",
			dbGetFieldName(pdbentry),dbGetString(pdbentry));
		}
		status=dbNextField(pdbentry,dctonly);
	    }
	    fprintf(fp,"}\n");
	    status = dbNextRecord(pdbentry);
	}
	if(precordTypename) break;
	status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return(0);
}

long dbWriteMenu(DBBASE *ppdbbase,const char *filename,char *menuName)
{
    FILE	*outFile;

    outFile = fopen(filename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",filename);
	return(-1);
    }
    dbWriteMenuFP(ppdbbase,outFile,menuName);
    if(fclose(outFile)) {
	errPrintf(0,__FILE__,__LINE__,"Error closing %s\n",filename);
    }
    return(0);
}

long dbWriteMenuFP(DBBASE *pdbbase,FILE *fp,char *menuName)
{
    dbMenu	*pdbMenu;
    int		gotMatch;
    int		i;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    pdbMenu = (dbMenu *)ellFirst(&pdbbase->menuList);
    while(pdbMenu) {
	if(menuName) {
	    gotMatch = (strcmp(menuName,pdbMenu->name)==0) ? TRUE : FALSE;
	}else {
	    gotMatch=TRUE;
	}
	if(gotMatch) {
	    fprintf(fp,"menu(%s) {\n",pdbMenu->name);
	    for(i=0; i<pdbMenu->nChoice; i++) {
		fprintf(fp,"\tchoice(%s,\"%s\")\n",pdbMenu->papChoiceName[i],
		    pdbMenu->papChoiceValue[i]);
	    }
	    fprintf(fp,"}\n");
	    if(menuName) break;
	}
	pdbMenu = (dbMenu *)ellNext(&pdbMenu->node);
    }
    return(0);
}

long dbWriteRecordType(DBBASE *pdbbase,const char *filename,char *recordTypeName)
{
    FILE	*outFile;

    outFile = fopen(filename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",filename);
	return(-1);
    }
    dbWriteRecordTypeFP(pdbbase,outFile,recordTypeName);
    if(fclose(outFile)) {
	errPrintf(0,__FILE__,__LINE__,"Error closing %s\n",filename);
    }
    return(0);
}

long dbWriteRecordTypeFP(DBBASE *pdbbase,FILE *fp,char *recordTypeName)
{
    dbRecordType	*pdbRecordType;
    dbFldDes	*pdbFldDes;
    int		gotMatch;
    int		i;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType; pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	if(recordTypeName) {
	    gotMatch = (strcmp(recordTypeName,pdbRecordType->name)==0)
		 ? TRUE : FALSE;
	}else {
	    gotMatch=TRUE;
	}
	if(!gotMatch) continue;
	fprintf(fp,"recordtype(%s) {\n",pdbRecordType->name);
	for(i=0; i<pdbRecordType->no_fields; i++) {
	    int	j;

	    pdbFldDes = pdbRecordType->papFldDes[i];
	    fprintf(fp,"\tfield(%s,",pdbFldDes->name);
	    for(j=0; j<DBF_NTYPES; j++) {
		if(pamapdbfType[j].value == pdbFldDes->field_type) break;
	    }
	    if(j>=DBF_NTYPES)
		fprintf(stderr,"\t     field_type: %d\n",
			pdbFldDes->field_type);
	    else
		fprintf(fp,"%s) {\n",pamapdbfType[j].strvalue);
	    if(pdbFldDes->prompt)
		fprintf(fp,"\t\tprompt(\"%s\")\n",pdbFldDes->prompt);
	    if(pdbFldDes->initial)
		fprintf(fp,"\t\tinitial(\"%s\")\n",pdbFldDes->initial);
	    if(pdbFldDes->promptgroup) {
		for(j=0; j<GUI_NTYPES; j++) {
		    if(pamapguiGroup[j].value == pdbFldDes->promptgroup) {
			fprintf(fp,"\t\tpromptgroup(%s)\n",
				pamapguiGroup[j].strvalue);
			break;
		    }
		}
	    }
	    if(pdbFldDes->special) {
		if(pdbFldDes->special >= SPC_NTYPES) {
		    fprintf(fp,"\t\tspecial(%d)\n",pdbFldDes->special);
		} else for(j=0; j<SPC_NTYPES; j++) {
		    if(pamapspcType[j].value == pdbFldDes->special) {
			fprintf(fp,"\t\tspecial(%s)\n",
				pamapspcType[j].strvalue);
			break;
		    }
		}
	    }
	    if(pdbFldDes->extra)
		fprintf(fp,"\t\textra(\"%s\")\n",pdbFldDes->extra);
	    if(pdbFldDes->field_type==DBF_MENU) {
		if(pdbFldDes->ftPvt)
		    fprintf(fp,"\t\tmenu(%s)\n",
			((dbMenu *)pdbFldDes->ftPvt)->name);
		else
		   fprintf(stderr,"\t\t  menu: NOT FOUND\n");
	    }
	    if(pdbFldDes->field_type==DBF_STRING) {
		fprintf(fp,"\t\tsize(%d)\n",
		    pdbFldDes->size);
	    }
	    if(pdbFldDes->process_passive) fprintf(fp,"\t\tpp(TRUE)\n");
	    if(pdbFldDes->base) fprintf(fp,"\t\tbase(HEX)\n");
	    if(pdbFldDes->interest)
		fprintf(fp,"\t\tinterest(%d)\n",pdbFldDes->interest);
	    if(!pdbFldDes->as_level) fprintf(fp,"\t\tasl(ASL0)\n");
	    fprintf(fp,"\t}\n");
	}
	fprintf(fp,"}\n");
	if(recordTypeName) break;
    }
    return(0);
}

long dbWriteDevice(DBBASE *pdbbase,const char *filename)
{
    FILE	*outFile;

    outFile = fopen(filename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",filename);
	return(-1);
    }
    dbWriteDeviceFP(pdbbase,outFile);
    if(fclose(outFile)) {
	errPrintf(0,__FILE__,__LINE__,"Error closing %s\n",filename);
    }
    return(0);
}

long dbWriteDeviceFP(DBBASE *pdbbase,FILE *fp)
{
    dbRecordType	*pdbRecordType;
    devSup	*pdevSup;

    if(!pdbbase) {
	fprintf(stderr,"dbWriteDeviceFP: pdbbase not specified\n");
	return(-1);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType; pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
	pdevSup; pdevSup = (devSup *)ellNext(&pdevSup->node)) {
	    int j;

	    for(j=0; j< LINK_NTYPES; j++) {
		if(pamaplinkType[j].value==pdevSup->link_type) break;
	    }
	    if(j>=LINK_NTYPES) {
		fprintf(fp,"link_type not valid\n");
		continue;
	    }
	    fprintf(fp,"device(%s,%s,%s,\"%s\")\n",
		pdbRecordType->name,
		pamaplinkType[j].strvalue,
		pdevSup->name,pdevSup->choice);
	}
    }
    return(0);
}

long dbWriteDriver(DBBASE *pdbbase,const char *filename)
{
    FILE	*outFile;

    outFile = fopen(filename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",filename);
	return(-1);
    }
    dbWriteDriverFP(pdbbase,outFile);
    if(fclose(outFile)) {
	errPrintf(0,__FILE__,__LINE__,"Error closing %s\n",filename);
    }
    return(0);
}

long dbWriteDriverFP(DBBASE *pdbbase,FILE *fp)
{
    drvSup	*pdrvSup;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    for(pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList);
    pdrvSup; pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
	fprintf(fp,"driver(%s)\n",pdrvSup->name);
    }
    return(0);
}

long dbWriteBreaktable(DBBASE *pdbbase,const char *filename)
{
    FILE	*outFile;

    outFile = fopen(filename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",filename);
	return(-1);
    }
    dbWriteBreaktableFP(pdbbase,outFile);
    if(fclose(outFile)) {
	errPrintf(0,__FILE__,__LINE__,"Error closing %s\n",filename);
    }
    return(0);
}

long dbWriteBreaktableFP(DBBASE *pdbbase,FILE *fp)
{
    brkTable	*pbrkTable;
    brkInt	*pbrkInt;
    int		ind;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    for(pbrkTable = (brkTable *)ellFirst(&pdbbase->bptList);
    pbrkTable; pbrkTable = (brkTable *)ellNext(&pbrkTable->node)) {
	fprintf(fp,"breaktable(%s) {\n",pbrkTable->name);
	for(ind=0; ind<pbrkTable->number; ind++) {
	    pbrkInt = pbrkTable->papBrkInt[ind];
	    fprintf(fp,"\t%f %f\n",pbrkInt->raw,pbrkInt->eng);
	}
	fprintf(fp,"}\n");
    }
    return(0);
}

long dbFindRecordType(DBENTRY *pdbentry,char *recordType)
{
    dbBase	*pdbbase = pdbentry->pdbbase;
    GPHENTRY	*phash;

    zeroDbentry(pdbentry);
    phash = gphFind(pdbbase->pgpHash,recordType,&pdbbase->recordTypeList);
    if(!phash) return(S_dbLib_recordTypeNotFound);
    pdbentry->precordType = phash->userPvt;
    return(0);
}

long dbFirstRecordType(DBENTRY *pdbentry)
{
    dbRecordType *precordType;

    zeroDbentry(pdbentry);
    precordType = (dbRecordType *)ellFirst(&pdbentry->pdbbase->recordTypeList);
    if(!precordType) return(S_dbLib_recordTypeNotFound);
    pdbentry->precordType = precordType;
    return(0);
}

long dbNextRecordType(DBENTRY *pdbentry)
{
    dbRecordType *precordType = pdbentry->precordType;

    zeroDbentry(pdbentry);
    precordType = (dbRecordType *)ellNext(&precordType->node);
    if(!precordType) return(S_dbLib_recordTypeNotFound);
    pdbentry->precordType = precordType;
    return(0);
}

char *dbGetRecordTypeName(DBENTRY *pdbentry)
{
    return(pdbentry->precordType->name);
}

int dbGetNRecordTypes(DBENTRY *pdbentry)
{
    return(ellCount(&pdbentry->pdbbase->recordTypeList));
}

long dbFirstField(DBENTRY *pdbentry,int dctonly)
{

    pdbentry->indfield = -1;
    return(dbNextField(pdbentry,dctonly));
}

long dbNextField(DBENTRY *pdbentry,int dctonly)
{
    dbRecordType		*precordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes  		*pflddes;
    short		indfield = pdbentry->indfield;
    long		status;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    indfield++;
    while(TRUE) {
	if(indfield>=precordType->no_fields) {
	    pdbentry->indfield = 0;
	    pdbentry->pflddes = NULL;
	    pdbentry->pfield = NULL;
	    return(S_dbLib_fieldNotFound);
	}
	pflddes = precordType->papFldDes[indfield];
	if(!dctonly || pflddes->promptgroup) {
	    /*Skip field if dctonly and no device support*/
	    if(!dctonly || (pflddes->field_type!=DBF_DEVICE)
	    || (ellCount(&precordType->devList)>0)) {
		pdbentry->indfield = indfield;
		pdbentry->pflddes = pflddes;
		pdbentry->indfield = indfield;
		if(precnode) {
		    status = dbGetFieldAddress(pdbentry);
		}else {
		    pdbentry->pfield = NULL;
		}
		return(0);
	    }
	}
	indfield++;
    }
}

int  dbGetFieldType(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    long		status;

    if(!pflddes){
	status = S_dbLib_flddesNotFound;
	entryErrMessage(pdbentry,status,"dbGetFieldType");
	return(status);
    }
    return(mapDBFtoDCT[pflddes->field_type]);
}

int dbGetNFields(DBENTRY *pdbentry,int dctonly)
{
    dbRecordType		*precordType = pdbentry->precordType;
    dbFldDes  		*pflddes;
    int			indfield,n;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    n = 0;
    for(indfield=0; indfield<precordType->no_fields; indfield++) {
	pflddes = precordType->papFldDes[indfield];
	if(dctonly && (pflddes->field_type==DBF_DEVICE) 
	&& (ellCount(&precordType->devList)==0) ) continue;
	if(!dctonly || pflddes->promptgroup) n++;
    }
    return(n);
}

char *dbGetFieldName(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(pflddes->name);
}

char *dbGetDefault(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(pflddes->initial);
}

char *dbGetPrompt(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(&pflddes->prompt[0]);
}

int dbGetPromptGroup(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(pflddes->promptgroup);
}

long dbCreateRecord(DBENTRY *pdbentry,char *precordName)
{
    dbRecordType		*precordType = pdbentry->precordType;
    dbFldDes		*pdbFldDes;
    PVDENTRY       	*ppvd;
    ELLLIST           	*preclist = NULL;
    dbRecordNode       	*precnode = NULL;
    dbRecordNode       	*pNewRecNode = NULL;
    long		status;
    devSup		*pdevSup;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    /*Get size of NAME field*/
    pdbFldDes = precordType->papFldDes[0];
    if(!pdbFldDes || (strcmp(pdbFldDes->name,"NAME")!=0))
	return(S_dbLib_nameLength);
    if((int)strlen(precordName)>=pdbFldDes->size) return(S_dbLib_nameLength);
    /* clear callers entry */
    zeroDbentry(pdbentry);
    if(!dbFindRecord(pdbentry,precordName)) return (S_dbLib_recExists);
    zeroDbentry(pdbentry);
    pdbentry->precordType = precordType;
    preclist = &precordType->recList;
    /* create a recNode */
    pNewRecNode = dbCalloc(1,sizeof(dbRecordNode));
    /* create a new record of this record type */
    pdbentry->precnode = pNewRecNode;
    if(status = dbAllocRecord(pdbentry,precordName)) return(status);
    pNewRecNode->recordname = dbRecordName(pdbentry);
    /* install record node in list in sorted postion */
    precnode = (dbRecordNode *)ellFirst(preclist);
    while(precnode && strcmp(precordName,(char*)precnode->precord) > 0)
	precnode = (dbRecordNode *)ellNext(&precnode->node);
    if(precnode) 
	ellInsert(preclist,ellPrevious(&precnode->node),&pNewRecNode->node);
    else
	ellAdd(preclist,&pNewRecNode->node);
    pdbentry->precnode = pNewRecNode;
    ppvd = dbPvdAdd(pdbentry->pdbbase,precordType,pNewRecNode);
    if(!ppvd) {errMessage(-1,"Logic Err: Could not add to PVD");return(-1);}
    /*If any device support exists let checkDevChoice give default for INP/OUT*/
    pdevSup = (devSup *)ellFirst(&precordType->devList);
    if(!pdevSup) return(0);
    status = checkDevChoice(pdbentry,pdevSup->link_type);
    return (status);
}

long dbDeleteRecord(DBENTRY *pdbentry)
{
    dbBase		*pdbbase = pdbentry->pdbbase;
    dbRecordType	*precordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    ELLLIST           	*preclist;
    long		status;

    if (!precnode) return (S_dbLib_recNotFound);
    preclist = &precordType->recList;
    ellDelete(preclist,&precnode->node);
    dbPvdDelete(pdbbase,precnode);
    if(status = dbFreeRecord(pdbentry)) return(status);
    free((void *)precnode);
    pdbentry->precnode = NULL;
    return (0);
}

long dbFreeRecords(DBBASE *pdbbase)
{
    DBENTRY		dbentry;
    dbRecordType	*pdbRecordType;
    dbRecordNode	*pdbRecordNode;
    dbRecordNode	*pdbRecordNodeNext;

    dbInitEntry(pdbbase,&dbentry);
    pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    while(pdbRecordType) {
	pdbRecordNode = (dbRecordNode *)ellFirst(&pdbRecordType->recList);
	while(pdbRecordNode) {
	    pdbRecordNodeNext = (dbRecordNode *)ellNext(&pdbRecordNode->node);
	    if(!dbFindRecord(&dbentry,pdbRecordNode->recordname))
		dbDeleteRecord(&dbentry);
	    pdbRecordNode = pdbRecordNodeNext;
	}
	pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node);
    }
    dbFinishEntry(&dbentry);
    return(0);
}

long dbFindRecord(DBENTRY *pdbentry,char *precordName)
{
    dbBase	*pdbbase = pdbentry->pdbbase;
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
    pdbentry->precordType = ppvdNode->precordType;
    if(*precordName++=='.') return(dbFindField(pdbentry, precordName));
    return (0);
}

long dbFirstRecord(DBENTRY *pdbentry)
{
    dbRecordType		*precordType = pdbentry->precordType;
    dbRecordNode	*precnode;

    zeroDbentry(pdbentry);
    if(!precordType) return(S_dbLib_recordTypeNotFound);
    pdbentry->precordType = precordType;
    precnode = (dbRecordNode *)ellFirst(&precordType->recList);
    if(!precnode) return(S_dbLib_recNotFound);
    pdbentry->precnode = precnode;
    return(0);
}

long dbNextRecord(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode=pdbentry->precnode;
    long		status=0;

    if(!precnode) return(S_dbLib_recNotFound);
    precnode = (dbRecordNode *)ellNext(&precnode->node);
    if(!precnode) status = S_dbLib_recNotFound;
    pdbentry->precnode = precnode;
    pdbentry->pfield = NULL;
    return(status);
}

int dbGetNRecords(DBENTRY *pdbentry)
{
    dbRecordType		*precordType = pdbentry->precordType;

    if(!precordType) return(0);
    return(ellCount(&precordType->recList));
}

char *dbGetRecordName(DBENTRY *pdbentry)
{
    return(dbRecordName(pdbentry));
}

long dbRenameRecord(DBENTRY *pdbentry,char *newName)
{
    dbBase		*pdbbase = pdbentry->pdbbase;
    dbRecordType	*precordType = pdbentry->precordType;
    dbFldDes		*pdbFldDes;
    dbRecordNode	*precnode = pdbentry->precnode;
    PVDENTRY		*ppvd;
    ELLLIST		*preclist;
    dbRecordNode	*plistnode;
    long		status;
    DBENTRY		dbentry;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    /*Get size of NAME field*/
    pdbFldDes = precordType->papFldDes[0];
    if(!pdbFldDes || (strcmp(pdbFldDes->name,"NAME")!=0))
	return(S_dbLib_nameLength);
    if((int)strlen(newName)>=pdbFldDes->size) return(S_dbLib_nameLength);
    if(!precnode) return(S_dbLib_recNotFound);
    dbInitEntry(pdbentry->pdbbase,&dbentry);
    status = dbFindRecord(&dbentry,newName);
    dbFinishEntry(&dbentry);
    if(!status) return(S_dbLib_recExists);
    dbPvdDelete(pdbbase,precnode);
    pdbentry->pflddes = precordType->papFldDes[0];
    if(status = dbGetFieldAddress(pdbentry)) return(status);
    strcpy(pdbentry->pfield,newName);
    ppvd = dbPvdAdd(pdbbase,precordType,precnode);
    if(!ppvd) {errMessage(-1,"Logic Err: Could not add to PVD");return(-1);}
    /*remove from record list and reinstall in sorted order*/
    preclist = &precordType->recList;
    ellDelete(preclist,&precnode->node);
    plistnode = (dbRecordNode *)ellFirst(preclist);
    while(plistnode) {
	pdbentry->precnode =  plistnode;
	if(strcmp(newName,dbRecordName(pdbentry)) >=0) break;
	plistnode = (dbRecordNode *)ellNext(&plistnode->node);
    }
    if(plistnode)
	ellInsert(preclist,ellPrevious(&plistnode->node),&precnode->node);
    else
	ellAdd(preclist,&precnode->node);
    return(0);
}

long dbVisibleRecord(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;
    if(!precnode) return(S_dbLib_recNotFound);
	precnode->visible=1;
	return 0;
}

long dbInvisibleRecord(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;
    if(!precnode) return(S_dbLib_recNotFound);
	precnode->visible=0;
	return 0;
}

int dbIsVisibleRecord(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;
    if(!precnode) return 0;
	return precnode->visible?1:0;
}

long dbCopyRecord(DBENTRY *pdbentry,char *newRecordName,int overWriteOK)
{
    dbRecordType	*precordType = pdbentry->precordType;
    dbFldDes		*pdbFldDes;
    dbRecordNode	*precnode = pdbentry->precnode;
    long		status;
    DBENTRY		dbentry;
    char		*pvalue;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    /*Get size of NAME field*/
    pdbFldDes = precordType->papFldDes[0];
    if(!pdbFldDes || (strcmp(pdbFldDes->name,"NAME")!=0))
	return(S_dbLib_nameLength);
    if((int)strlen(newRecordName)>=pdbFldDes->size) return(S_dbLib_nameLength);
    if(!precnode) return(S_dbLib_recNotFound);
    dbInitEntry(pdbentry->pdbbase,&dbentry);
    status = dbFindRecord(&dbentry,newRecordName);
    if(!status) {
	if(!overWriteOK) {
	    dbFinishEntry(&dbentry);
	    return(S_dbLib_recExists);
	}
	status = dbDeleteRecord(&dbentry);
	if(status) return(status);
    }
    dbFinishEntry(&dbentry);
    if(status = dbFindRecordType(&dbentry,precordType->name)) return(status);
    if(status = dbCreateRecord(&dbentry,newRecordName)) return(status);
    if(status = dbFirstField(pdbentry,TRUE)) return(status);
    if(status = dbFirstField(&dbentry,TRUE)) return(status);
    while(!status) {
	if(!dbIsDefaultValue(pdbentry)) {
	    pvalue = dbGetString(pdbentry);
	    if(status = dbPutString(&dbentry,pvalue)) return(status);
	}
	status = dbNextField(pdbentry,TRUE);
	if(!status) status = dbNextField(&dbentry,TRUE);
	if(!status && (pdbentry->pflddes!=dbentry.pflddes)) {
	    epicsPrintf("dbCopyRecord: Logic Error\n");
	    exit(1);
	}
    }
    return(0);
}

long dbFindField(DBENTRY *pdbentry,char *pname)
{
    dbRecordType	*precordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    char		*precord;
    dbFldDes  		*pflddes;
    short           	top, bottom, test;
    char  		**papsortFldName;
    short          	*sortFldInd;
    long		status;
    int			compare;
    char		fieldName[MAX_FIELD_NAME_LENGTH];
    char		*pfieldName;
    int			ind;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    precord = precnode->precord;
    papsortFldName = precordType->papsortFldName;
    sortFldInd = precordType->sortFldInd;
    /*copy field name. Stop at null or blank or tab*/
    pfieldName = &fieldName[0];
    for(ind=0; ind<MAX_FIELD_NAME_LENGTH; ind++) {
	if(*pname=='\0' || *pname==' ' || *pname=='\t') break;
	*pfieldName++ = *pname++;
    }
    *pfieldName = '\0';
    pfieldName = &fieldName[0];
    /* check for default field name or VAL to be supplied */
    if((*pfieldName==0) || (strcmp(pfieldName,"VAL")==0) ) {
	if(!(pflddes=precordType->pvalFldDes)) 
		return(S_dbLib_recordTypeNotFound);
	pdbentry->pflddes = pflddes;
	pdbentry->indfield = precordType->indvalFlddes;
	status = dbGetFieldAddress(pdbentry);
	return (status);
    }
    /* binary search through ordered field names */
    top = precordType->no_fields - 1;
    bottom = 0;
    test = (top + bottom) / 2;
    while (1) {
	/* check the field name */
	compare = strcmp(papsortFldName[test],pfieldName);
	if (compare == 0) {
	    if(!(pflddes=precordType->papFldDes[sortFldInd[test]]))
		return(S_dbLib_recordTypeNotFound);
	    pdbentry->pflddes = pflddes;
	    pdbentry->indfield = sortFldInd[test];
	    status = dbGetFieldAddress(pdbentry);
	    return (status);
	} else if (compare > 0) {
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

int dbFoundField(DBENTRY *pdbentry)
{
    return((pdbentry->pfield) ? TRUE : FALSE);
}

char *dbGetString(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    char	*message;
    unsigned char cvttype;

    message = getpMessage(pdbentry);
    *message = 0;
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
    cvttype = pflddes->base;
    switch (pflddes->field_type) {
    case DBF_STRING:
	if(!pfield) {strcpy(message,"Field not found"); return(message);}
	strcpy(message, (char *)pfield);
	break;
    case DBF_CHAR:
    case DBF_UCHAR:
    case DBF_SHORT:
    case DBF_USHORT:
    case DBF_ENUM:
    case DBF_LONG:
    case DBF_ULONG:
    case DBF_FLOAT:
    case DBF_DOUBLE:
	return(dbGetStringNum(pdbentry));
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;
	    short	choice_ind;
	    char	*pchoice;

	    if(!pfield) {strcpy(message,"Field not found"); return(message);}
	    choice_ind = *((short *) pdbentry->pfield);
	    if(!pdbMenu || choice_ind<0 || choice_ind>=pdbMenu->nChoice)
		return(NULL);
	    pchoice = pdbMenu->papChoiceValue[choice_ind];
	    strcpy(message, pchoice);
	}
	break;
    case DBF_DEVICE: {
	    dbDeviceMenu	*pdbDeviceMenu;
	    char		*pchoice;
	    short		choice_ind;

	    if(!pfield) {strcpy(message,"Field not found"); return(message);}
	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(NULL);
	    choice_ind = *((short *) pdbentry->pfield);
	    if(choice_ind<0 || choice_ind>=pdbDeviceMenu->nChoice)
		return(NULL);
	    pchoice = pdbDeviceMenu->papChoice[choice_ind];
	    strcpy(message, pchoice);
	}
	break;
    case DBF_INLINK:
    case DBF_OUTLINK: {
	    DBLINK *plink=(DBLINK *)pfield;

	    if(!pfield) {strcpy(message,"Field not found"); return(message);}
	    switch(plink->type) {
	    case CONSTANT:
		if(plink->value.constantStr) {
		    strcpy(message,plink->value.constantStr);
		} else {
		    strcpy(message,"");
		}
		break;
	    case PV_LINK:
	    case CA_LINK:
	    case DB_LINK: {
		int	ppind;
		short	pvlMask;

		pvlMask = plink->value.pv_link.pvlMask;
		if(pvlMask&pvlOptPP) ppind=1;
		else if(pvlMask&pvlOptCA) ppind=2;
		else if(pvlMask&pvlOptCP) ppind=3;
		else if(pvlMask&pvlOptCPP) ppind=4;
		else ppind=0;
		if(plink->value.pv_link.pvname)
		    strcpy(message,plink->value.pv_link.pvname);
		else
		    strcpy(message,"");
		strcat(message," ");
		strcat(message,ppstring[ppind]);
		strcat(message," ");
		strcat(message,msstring[pvlMask&pvlOptMS]);
		break;
	    }
	    case VME_IO:
		sprintf(message,"#C%d S%d @%s",
		    plink->value.vmeio.card,plink->value.vmeio.signal,
		    plink->value.vmeio.parm);
		break;
	    case CAMAC_IO:
		sprintf(message,"#B%d C%d N%d A%d F%d @%s",
		    plink->value.camacio.b,plink->value.camacio.c,
		    plink->value.camacio.n,plink->value.camacio.a,
		    plink->value.camacio.f,plink->value.camacio.parm);
		break;
	    case RF_IO:
		sprintf(message,"#R%d M%d D%d E%d",
		    plink->value.rfio.cryo,
		    plink->value.rfio.micro,
		    plink->value.rfio.dataset,
		    plink->value.rfio.element);
		break;
	    case AB_IO:
		sprintf(message,"#L%d A%d C%d S%d @%s",
		    plink->value.abio.link,plink->value.abio.adapter,
		    plink->value.abio.card,plink->value.abio.signal,
		    plink->value.abio.parm);
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
		sprintf(message,"@%s", plink->value.instio.string);
		break;
	    case VXI_IO :
		if (plink->value.vxiio.flag == VXIDYNAMIC)
		    sprintf(message,"#V%d C%d S%d @%s",
			plink->value.vxiio.frame,plink->value.vxiio.slot,
			plink->value.vxiio.signal,plink->value.vxiio.parm);
		else
		    sprintf(message,"#V%d S%d @%s",
			plink->value.vxiio.la,plink->value.vxiio.signal,
			plink->value.vxiio.parm);
		break;
	    default :
	        return(NULL);
	    }
	}
	break;
    case DBF_FWDLINK: {
	    DBLINK *plink=(DBLINK *)pfield;

	    if(!pfield) {strcpy(message,"Field not found"); return(message);}
	    switch(plink->type) {
	    case CONSTANT:
		strcpy(message,"0");
		break;
	    case PV_LINK:
	    case CA_LINK:
	    case DB_LINK: {
		int	ppind;
		short	pvlMask;

		pvlMask = plink->value.pv_link.pvlMask;
		if(pvlMask&pvlOptCA) ppind=2;
		else ppind=0;
		if(plink->value.pv_link.pvname)
		    strcpy(message,plink->value.pv_link.pvname);
		else
		    strcpy(message,"");
		if(ppind) {
		    strcat(message," ");
		    strcat(message,ppstring[ppind]);
		}
		break;
	    }
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

long dbPutString(DBENTRY *pdbentry,char *pstring)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    long	status=0;

    if(!pflddes) return(S_dbLib_flddesNotFound);
    switch (pflddes->field_type) {
    case DBF_STRING:
	if(!pfield) return(S_dbLib_fieldNotFound);
	strncpy((char *)pfield, pstring,pflddes->size);
	if(pflddes->special == SPC_CALC) {
	    char  rpcl[RPCL_LEN];
	    short error_number;

	    status = postfix(pstring,rpcl,&error_number);
	    if(status) status = S_dbLib_badField;
	}
	if((short)strlen(pstring) >= pflddes->size) status = S_dbLib_strLen;
	break;
    case DBF_CHAR :
    case DBF_SHORT :
    case DBF_LONG:
    case DBF_UCHAR:
    case DBF_USHORT:
    case DBF_ULONG:
    case DBF_ENUM:
    case DBF_FLOAT:
    case DBF_DOUBLE:
	return(dbPutStringNum(pdbentry,pstring));
    case DBF_MENU: {
	    dbMenu		*pdbMenu = (dbMenu *)pflddes->ftPvt;
	    char		*pchoice;
	    int			i;
	    
	    if(!pfield) return(S_dbLib_fieldNotFound);
	    if(!pdbMenu) return(S_dbLib_menuNotFound);
	    for (i = 0; i < pdbMenu->nChoice; i++) {
		if(!(pchoice = pdbMenu->papChoiceValue[i])) continue;
		if(strcmp(pchoice, pstring) == 0) {
		    *(unsigned short *)pfield = i;
		    return(0);
		}
	    }
	}
	return (S_dbLib_badField);
    case DBF_DEVICE: {
	    dbDeviceMenu	*pdbDeviceMenu;
	    char		*pchoice;
	    int			i;

	    if(!pfield) return(S_dbLib_fieldNotFound);
	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(S_dbLib_menuNotFound);
	    for (i = 0; i < pdbDeviceMenu->nChoice; i++) {
		if (!(pchoice = pdbDeviceMenu->papChoice[i]))
		    continue;
		if (strcmp(pchoice, pstring) == 0) {
		    long 	link_type;
		    devSup	*pdevSup;

		    pdevSup = (devSup *)ellNth(&pdbentry->precordType->devList,i+1);
		    link_type = pdevSup->link_type;
		    /*If no INP or OUT OK */
		    checkDevChoice(pdbentry,link_type);
		    *(unsigned short *)pfield = i;
		    return(0);
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
	    int		ind;

	    if(!pfield) return(S_dbLib_fieldNotFound);
	    if(strlen(pstring)>=sizeof(string)) {
	        status = S_dbLib_badField;
	        errMessage(status,"dbPutString received a LONG string");
	        return(status);
            }
	    strcpy(pstr,pstring);
	    /*strip off leading blanks and tabs*/
	    while(*pstr && (*pstr==' ' || *pstr=='\t')) pstr++;
	    /*strip off trailing blanks and tabs*/
	    if(pstr) for(ind = strlen(pstr)-1; ind>=0; ind--) {
		if(pstr[ind]!=' ' && pstr[ind]!='\t') break;
		pstr[ind] = '\0';
	    }
	    if(!pstr || (int)strlen(pstr)<=0 ) {
		if(plink->type==PV_LINK) dbCvtLinkToConstant(pdbentry);
		if(plink->type!=CONSTANT) return(S_dbLib_badField);
		free((void *)plink->value.constantStr);
		plink->value.constantStr = NULL;
		return(0);
	    }
	    switch(plink->type) {
	    case CONSTANT: 
	    case PV_LINK: {
	    	    short	ppOpt = 0;
		    short	msOpt = 0;
	    	    char	*end;
		    double	tempval;

		    /* Check first to see if string is a constant*/
		    /*It is a double if strtod eats entire string*/
		    /*leading and trailing blanks have already been stripped*/
		    tempval = strtod(pstr,&end);
		    if(*end == 0) {
			if(plink->type==PV_LINK) dbCvtLinkToConstant(pdbentry);
			if((!plink->value.constantStr) ||
			((int)strlen(plink->value.constantStr)<(int)strlen(pstr)
			)) {
			    free(plink->value.constantStr);
			    plink->value.constantStr =
				dbCalloc(strlen(pstr)+1,sizeof(char));
			}
			strcpy(plink->value.constantStr,pstr);
			return(0);
		    }
		    if(plink->type==CONSTANT) dbCvtLinkToPvlink(pdbentry);
	    	    end = strchr(pstr,' ');
		    if(end) {
			switch (pflddes->field_type) {
			case DBF_INLINK: {
			    if(strstr(end,"NPP")) ppOpt = 0;
			    else if(strstr(end,"CPP")) ppOpt = pvlOptCPP;
			    else if(strstr(end,"PP")) ppOpt = pvlOptPP;
			    else if(strstr(end,"CA")) ppOpt = pvlOptCA;
			    else if(strstr(end,"CP")) ppOpt = pvlOptCP;
			    else ppOpt = 0;
		            if(strstr(end,"NMS")) msOpt = 0;
		            else if(strstr(end,"MS")) msOpt = pvlOptMS;
			    else msOpt = 0;
		            *end = 0;
			}
			break;
			case DBF_OUTLINK: {
			    if(strstr(end,"NPP")) ppOpt = 0;
			    else if(strstr(end,"PP")) ppOpt = pvlOptPP;
			    else if(strstr(end,"CA")) ppOpt = pvlOptCA;
			    else ppOpt = 0;
		            if(strstr(end,"NMS")) msOpt = 0;
		            else if(strstr(end,"MS")) msOpt = pvlOptMS;
			    else msOpt = 0;
		            *end = 0;
			}
			break;
			case DBF_FWDLINK: {
			    if(strstr(end,"NPP")) ppOpt = 0;
			    else if(strstr(end,"CA")) ppOpt = pvlOptCA;
			    else ppOpt = 0;
			    msOpt = 0;
		            *end = 0;
			}
			break;
			default:
			epicsPrintf("dbPutString: Logic Error\n");
			}
		    }
		    status = putPvLink(pdbentry,ppOpt|msOpt,pstr);
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
		    status = putParmString(&plink->value.vmeio.parm,pstr);
		}
		break;
	    case CAMAC_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'B'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.camacio.b);
		    if(!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.camacio.c);
		    if(!(end = strchr(pstr,'N'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.camacio.n);
 		    if(!(end = strchr(pstr,'A')))  {
 			plink->value.camacio.a = 0;
 		    } else {
 		        pstr = end + 1;
 		        sscanf(pstr,"%hd",&plink->value.camacio.a);
 		    }
 		    if(!(end = strchr(pstr,'F'))) {
 			plink->value.camacio.f = 0;
 		    } else {
 		        pstr = end + 1;
 		        sscanf(pstr,"%hd",&plink->value.camacio.f);
 		    }
		    status = putParmString(&plink->value.camacio.parm,pstr);
		}
		break;
	    case RF_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'R'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.rfio.cryo);
		    if(!(end = strchr(pstr,'M'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.rfio.micro);
		    if(!(end = strchr(pstr,'D'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.rfio.dataset);
		    if(!(end = strchr(pstr,'E'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&plink->value.rfio.element);
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
		    status = putParmString(&plink->value.abio.parm,pstr);
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
		    status = putParmString(&plink->value.gpibio.parm,pstr);
		}
		break;
	    case BITBUS_IO: {
		    /* jbk - the bbgpib struct uses unsigned char's instead
		    of short, so read values into short and then convert */

	    	    char	*end;
		    short	tmp_val;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&tmp_val);
		    plink->value.bitbusio.link=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'N'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&tmp_val);
		    plink->value.bitbusio.node=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'P'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&tmp_val);
		    plink->value.bitbusio.port=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'S'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&tmp_val);
		    plink->value.bitbusio.signal=(unsigned char)tmp_val;
		    status = putParmString(&plink->value.bitbusio.parm,pstr);
		}
		break;
	    case BBGPIB_IO: {
		    /* jbk - the bbgpib struct uses unsigned char's instead
		    of short, so read values into short and then convert */

	    	    char	*end;
		    short	tmp_val;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&tmp_val);
		    plink->value.bbgpibio.link=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'B'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&tmp_val);
		    plink->value.bbgpibio.bbaddr=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'G'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    sscanf(pstr,"%hd",&tmp_val);
		    plink->value.bbgpibio.gpibaddr=tmp_val;
		    status = putParmString(&plink->value.bbgpibio.parm,pstr);
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
		    if(end = strchr(pstr,'S')) {
			pstr = end + 1;
			sscanf(pstr,"%hd",&plink->value.vxiio.signal);
		    } else {
			plink->value.vxiio.signal = 0;
		    }
		    status = putParmString(&plink->value.vxiio.parm,pstr);
		}
		break;
	    case INST_IO: {
		    status = putParmString(&plink->value.instio.string,pstr);
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

char *dbVerify(DBENTRY *pdbentry,char *pstring)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    char		*message;

    message = getpMessage(pdbentry);
    *message = 0;
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
    switch (pflddes->field_type) {
    case DBF_STRING: {
	    unsigned int length;

	    length=strlen(pstring);
	    if(length>=pflddes->size) {
		sprintf(message,"string to big. max=%hd",pflddes->size);
		return(message);
	    }
	    if(pflddes->special == SPC_CALC) {
		char  rpcl[RPCL_LEN];
		short error_number;
		long  status;

		status = postfix(pstring,rpcl,&error_number);
		if(status)  {
		    sprintf(message,"Illegal Calculation String");
		    return(message);
		}
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
	    default:
		errPrintf(-1,__FILE__, __LINE__,"Logic Error\n");
		return(NULL);
	    }
	}
    case DBF_UCHAR:
    case DBF_USHORT:
    case DBF_ULONG:
    case DBF_ENUM:{
	    unsigned long  value;
	    char  *endp;

	    if(strchr(pstring,'-')) {
		strcpy(message,"not an unsigned number");
		return(message);
	    }
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
	    default:
		errPrintf(-1,__FILE__, __LINE__,"Logic Error\n");
		return(NULL);
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
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;
	    char	*pchoice;
	    int		i;
	    
	    if(!pdbMenu) return(NULL);
	    for (i = 0; i < pdbMenu->nChoice; i++) {
		if(!(pchoice = pdbMenu->papChoiceValue[i])) continue;
		if(strcmp(pchoice, pstring) == 0) {
		    return(NULL);
		}
	    }
	}
	strcpy(message,"Not a valid menu choice");
	return (message);
    case DBF_DEVICE: {
	    dbDeviceMenu	*pdbDeviceMenu;
	    char		*pchoice;
	    int			i;

	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(NULL);
	    if(pdbDeviceMenu->nChoice == 0) return(NULL);
	    for (i = 0; i < pdbDeviceMenu->nChoice; i++) {
		if (!(pchoice = pdbDeviceMenu->papChoice[i]))
		    continue;
		if (strcmp(pchoice, pstring) == 0) {
		    return(NULL);
		}
	    }
	}
	strcpy(message,"Not a valid menu choice");
	return (message);
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	return(NULL);
    default: break;
    }
    strcpy(message,"Not a valid field type");
    return (message);
}

char *dbGetRange(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    char		*message;

    message = getpMessage(pdbentry);
    *message = 0;
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
    switch (pflddes->field_type) {
    case DBF_STRING: {strcpy(message,"STRING"); return(message);}
    case DBF_CHAR : {strcpy(message,"CHAR"); return(message);}
    case DBF_SHORT : {strcpy(message,"SHORT");return(message);}
    case DBF_LONG: {strcpy(message,"LONG"); return(message);}
    case DBF_UCHAR: {strcpy(message,"UCHAR");return(message);}
    case DBF_USHORT:{strcpy(message,"USHORT");return(message);}
    case DBF_ULONG:{strcpy(message,"ULONG:");return(message);}
    case DBF_ENUM: {strcpy(message,"ENUM");return(message);}
    case DBF_FLOAT: {strcpy(message,"FLOAT");return(message);}
    case DBF_DOUBLE: {strcpy(message,"DOUBLE");return(message);}
    case DBF_MENU: {strcpy(message,"MENU");return(message);}
    case DBF_DEVICE: {strcpy(message,"DEVICE");return(message);}
    case DBF_INLINK: {strcpy(message,"INLINK");return(message);}
    case DBF_OUTLINK: {strcpy(message,"OUTLINK");return(message);}
    case DBF_FWDLINK: {strcpy(message,"FWDLINK");return(message);}
    default:
	errPrintf(-1,__FILE__, __LINE__,"Logic Error\n");
    }
    strcpy(message,"Not a valid field type");
    return (message);
}

brkTable *dbFindBrkTable(dbBase *pdbbase,char *name)
{
    GPHENTRY *pgph;

    pgph = gphFind(pdbbase->pgpHash,name,(void *)&pdbbase->bptList);
    if(!pgph) return(NULL);
    return((brkTable *)pgph->userPvt);
}

dbMenu *dbFindMenu(dbBase *pdbbase,char *name)
{
    GPHENTRY *pgph;

    pgph = gphFind(pdbbase->pgpHash,name,(void *)&pdbbase->menuList);
    if(!pgph) return(NULL);
    return((dbMenu *)pgph->userPvt);
}

char   **dbGetMenuChoices(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    switch (pflddes->field_type) {
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;

	    if(!pdbMenu) return(NULL);
	    return(pdbMenu->papChoiceValue);
	}
    case DBF_DEVICE: {
	    dbDeviceMenu *pdbDeviceMenu;

	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(NULL);
	    return(pdbDeviceMenu->papChoice);
	}
    default:
	return(NULL);
    }
}

int dbGetMenuIndex(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;

    if(!pflddes) return(-1);
    if(!pfield) return(-1);
    switch (pflddes->field_type) {
	case (DBF_MENU):
	case (DBF_DEVICE):
    	    return((int)(*(unsigned short *)pfield));
	default:
	    errPrintf(-1,__FILE__, __LINE__,"Logic Error\n");
    }
    return(-1);
}

long dbPutMenuIndex(DBENTRY *pdbentry,int index)
{
    dbFldDes  		*pflddes = pdbentry->pflddes;
    unsigned short	*pfield = pdbentry->pfield;

    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!pfield) return(S_dbLib_fieldNotFound);
    switch (pflddes->field_type) {
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;

	    if(!pdbMenu) return(S_dbLib_menuNotFound);
	    if(index<0 | index>=pdbMenu->nChoice) return(S_dbLib_badField);
	    *pfield = (unsigned short)index;
	    return(0);
	}
    case DBF_DEVICE: {
	    dbDeviceMenu *pdbDeviceMenu;

	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(S_dbLib_menuNotFound);
	    if(index<0 | index>=pdbDeviceMenu->nChoice)
		return(S_dbLib_badField);
	    return(dbPutString(pdbentry,pdbDeviceMenu->papChoice[index]));
	}
    default:
	break;
    }
    return (S_dbLib_badField);
}

int dbGetNMenuChoices(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(-1);
    switch (pflddes->field_type) {
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;

	    if(!pdbMenu) return(0);
	    return(pdbMenu->nChoice);
	}
    case DBF_DEVICE: {
	    dbDeviceMenu *pdbDeviceMenu;

	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(0);
	    return(pdbDeviceMenu->nChoice);
	}
    default:
	break;
    }
    return (-1);
}

int dbAllocForm(DBENTRY *psave)
{
    DBENTRY	dbEntry;
    DBENTRY	*pdbentry= &dbEntry;
    DBENTRY	*plinkentry;
    dbFldDes	*pflddes;
    dbFldDes	*plinkflddes;
    DBLINK	*plink;
    int		nlines=0;
    char	*pstr;
    struct form	*pform;
    int		linkType;
    long	status = 0;
    int		nbytes,i;

    if(psave->formpvt) {
	epicsPrintf("dbAllocForm called but form already exists\n");
	return(0);
    }
    copyEntry(psave,pdbentry);
    pflddes = pdbentry->pflddes;
    if(pflddes->field_type == DBF_DEVICE) {
        status = dbFindField(pdbentry,"INP");
        if(status) status = dbFindField(pdbentry,"OUT");
	if(status) goto done;
    } else {
	if((pflddes->field_type!=DBF_INLINK)
	&& (pflddes->field_type!=DBF_OUTLINK)
	&& (pflddes->field_type!=DBF_FWDLINK)) {
	    epicsPrintf("dbAllocForm called but not DBF_DEVICE or DBF_xxxLINK\n");
	    goto done;
	}
    }
    plinkentry = pdbentry;
    plinkflddes = plinkentry->pflddes;
    plink = (DBLINK *)(plinkentry->pfield);
    status = mapLINKTtoFORMT(plink,plinkflddes,&linkType);
    if(status) goto done;
    nlines = formlines[linkType];
    /*Dont know how to handle string size. Just use messagesize*/
    nbytes = sizeof(struct form) + 2*nlines*(sizeof(char *) + messagesize);
    pform = dbCalloc(1,nbytes);
    pform->linkType = linkType;
    psave->formpvt = pform;
    pform->plink = plink ;
    pform->prompt = promptAddr[linkType];
    pform->value = (char **)((char *)pform + sizeof(struct form));
    pform->verify = (char **)((char *)(pform->value)+nlines*sizeof(char *));
    pstr = (char *)(pform->verify) + nlines*sizeof(char *);
    for(i=0; i<nlines; i++) {
	pform->value[i] = pstr;
	pstr += messagesize;
    }
    for(i=0; i<nlines; i++) {
	pform->verify[i] = pstr;
	pstr += messagesize;
    }
done:
    dbFinishEntry(pdbentry);
    return(nlines);
}

long  dbFreeForm(DBENTRY *pdbentry)
{
    if(pdbentry->formpvt) {
	free(pdbentry->formpvt);
	pdbentry->formpvt = NULL;
    }
    return(0);
}

char  **dbGetFormPrompt(DBENTRY *pdbentry)
{
    struct form *pform = pdbentry->formpvt;

    if(!pform) return(NULL);
    return(pform->prompt);
}

char  **dbGetFormValue(DBENTRY *pdbentry)
{
    struct form *pform = pdbentry->formpvt;
    DBLINK	*plink;
    char	**value;

    if(!pform) return(NULL);
    plink = pform->plink;
    if(!plink) return(NULL);
    value = pform->value;
    switch(pform->linkType) {
    case FORM_CONSTANT: 
	if(plink->value.constantStr) {
	    strcpy(*value,plink->value.constantStr);
	} else {
	    strcpy(*value,"");
	}
	break;
    case FORM_INLINK: {
        short 	pvlMask = plink->value.pv_link.pvlMask;

	if(plink->value.pv_link.pvname)
	    strcpy(*value,plink->value.pv_link.pvname);
	else
	    strcpy(*value,"");
	value++;
	if(pvlMask&pvlOptPP) strcpy(*value,"PP");
	else if(pvlMask&pvlOptCA) strcpy(*value,"CA");
	else if(pvlMask&pvlOptCP) strcpy(*value,"CP");
	else if(pvlMask&pvlOptCPP) strcpy(*value,"CPP");
	else strcpy(*value,"NPP");
	value++;
	if(pvlMask&pvlOptMS) strcpy(*value,"MS");
	else strcpy(*value,"NMS");
	value++;
	}
	break;
    case FORM_OUTLINK: {
        short 	pvlMask = plink->value.pv_link.pvlMask;

	if(plink->value.pv_link.pvname)
	    strcpy(*value,plink->value.pv_link.pvname);
	else
	    strcpy(*value,"");
	value++;
	if(pvlMask&pvlOptPP) strcpy(*value,"PP");
	else if(pvlMask&pvlOptCA) strcpy(*value,"CA");
	else strcpy(*value,"NPP");
	value++;
	if(pvlMask&pvlOptMS) strcpy(*value,"MS");
	else strcpy(*value,"NMS");
	value++;
	}
	break;
    case FORM_FWDLINK: {
        short 	pvlMask = plink->value.pv_link.pvlMask;

	if(plink->value.pv_link.pvname)
	    strcpy(*value,plink->value.pv_link.pvname);
	else
	    strcpy(*value,"");
	value++;
	if(pvlMask&pvlOptCA) strcpy(*value,"CA");
	else strcpy(*value,"");
	}
	break;
    case FORM_VME_IO:
	cvtShortToString(plink->value.vmeio.card,*value);
	value++;
	cvtShortToString(plink->value.vmeio.signal,*value);
	value++;
	strcpy(*value,"@");
	strcat(*value,plink->value.vmeio.parm);
	break;
    case FORM_CAMAC_IO:
	cvtShortToString(plink->value.camacio.b,*value);
	value++;
	cvtShortToString(plink->value.camacio.c,*value);
	value++;
	cvtShortToString(plink->value.camacio.n,*value);
	value++;
	cvtShortToString(plink->value.camacio.a,*value);
	value++;
	cvtShortToString(plink->value.camacio.f,*value);
	value++;
	strcpy(*value,"@");
	strcat(*value,plink->value.camacio.parm);
	break;
    case FORM_RF_IO:
	cvtShortToString(plink->value.rfio.cryo,*value);
	value++;
	cvtShortToString(plink->value.rfio.micro,*value);
	value++;
	cvtShortToString(plink->value.rfio.dataset,*value);
	value++;
	cvtShortToString(plink->value.rfio.element,*value);
	break;
    case FORM_AB_IO:
	cvtShortToString(plink->value.abio.link,*value);
	value++;
	cvtShortToString(plink->value.abio.adapter,*value);
	value++;
	cvtShortToString(plink->value.abio.card,*value);
	value++;
	cvtShortToString(plink->value.abio.signal,*value);
	value++;
	strcpy(*value,"@");
	strcat(*value,plink->value.abio.parm);
	break;
    case FORM_GPIB_IO:
	cvtShortToString(plink->value.gpibio.link,*value);
	value++;
	cvtShortToString(plink->value.gpibio.addr,*value);
	value++;
	strcpy(*value,"@");
	strcat(*value,plink->value.gpibio.parm);
	break;
    case FORM_BITBUS_IO:
	cvtCharToString(plink->value.bitbusio.link,*value);
	value++;
	cvtCharToString(plink->value.bitbusio.node,*value);
	value++;
	cvtCharToString(plink->value.bitbusio.port,*value);
	value++;
	cvtCharToString(plink->value.bitbusio.signal,*value);
	value++;
	strcpy(*value,"@");
	strcat(*value,plink->value.bitbusio.parm);
	break;
    case FORM_INST_IO:
	strcpy(*value,"@");
	strcat(*value,plink->value.instio.string);
	break;
    case FORM_BBGPIB_IO:
	cvtCharToString(plink->value.bbgpibio.link,*value);
	value++;
	cvtCharToString(plink->value.bbgpibio.bbaddr,*value);
	value++;
	cvtCharToString(plink->value.bbgpibio.gpibaddr,*value);
	value++;
	strcpy(*value,"@");
	strcat(*value,plink->value.bbgpibio.parm);
	break;
    case FORM_VXI_IO:
	strcpy(*value,(plink->value.vxiio.flag == VXIDYNAMIC ? "Yes" : "No"));
	value++;
	if(plink->value.vxiio.flag == VXIDYNAMIC)
	    cvtShortToString(plink->value.vxiio.frame,*value);
	else
	    **value = 0;
	value++;
	if(plink->value.vxiio.flag == VXIDYNAMIC)
	    cvtShortToString(plink->value.vxiio.slot,*value);
	else
	    **value = 0;
	value++;
	if(plink->value.vxiio.flag == VXISTATIC)
	    cvtShortToString(plink->value.vxiio.la,*value);
	else
	    **value = 0;
	value++;
	cvtShortToString(plink->value.vxiio.signal,*value);
	value++;
	strcpy(*value,"@");
	strcat(*value,plink->value.vxiio.parm);
	break;
    default :
	return(NULL);
    }
    return(pform->value);
}

long  dbPutForm(DBENTRY *pdbentry,char **value)
{
    struct form *pform = pdbentry->formpvt;
    DBLINK	*plink;
    char	**verify;
    long	lvalue;
    double	dvalue;
    char	*endp;
    long	status = 0;

    if(!pform) return(S_dbLib_badLink);
    plink = pform->plink;
    if(!plink) return(S_dbLib_badLink);
    verify = pform->verify;
    switch(pform->linkType) {
    case FORM_CONSTANT: 
	**verify = 0; /*Initialize to no error*/
	if(**value == '\0') break;
	dvalue = strtod(*value,&endp);
	if(*endp!=0) {
	    strcpy(*verify,"Illegal. Must be number");
	    break;
	}
	if((!plink->value.constantStr)
	|| ((int)strlen(plink->value.constantStr)<(int)strlen(*value))) {
	    free(plink->value.constantStr);
	    plink->value.constantStr = dbCalloc(strlen(*value)+1,sizeof(char));
	}
	strcpy(plink->value.constantStr,*value);
	break;
    case FORM_INLINK: {
	    DBENTRY	dbEntry;
	    DBENTRY	*plinkentry = &dbEntry;
	    short	ppOpt = 0;
	    short	msOpt = 0;
	    char	*pstr;

	    pstr = *value;
	    **verify = 0;
	    value++; verify++;
	    **verify = 0;  /*Initialize verify to NULL*/
	    if((*value==NULL) || (strcmp(*value,"")==0)) ppOpt = 0;
	    else if(strstr(*value,"NPP")) ppOpt = 0;
	    else if(strstr(*value,"PP")) ppOpt = pvlOptPP;
	    else if(strstr(*value,"CA")) ppOpt = pvlOptCA;
	    else if(strstr(*value,"CP")) ppOpt = pvlOptCP;
	    else if(strstr(*value,"CPP")) ppOpt = pvlOptCPP;
	    else strcpy(*verify,"Illegal. Chose a value");
	    value++; verify++;
	    **verify = 0;  /*Initialize verify to NULL*/
	    if((*value==NULL) || (strcmp(*value,"")==0)) msOpt = 0;
	    else if(strstr(*value,"NMS")) msOpt = 0;
	    else if(strstr(*value,"MS")) msOpt = pvlOptMS;
	    else strcpy(*verify,"Illegal. Chose a value");
	    copyEntry(pdbentry,plinkentry);
	    if(pdbentry->pflddes->field_type == DBF_DEVICE) {
        	status = dbFindField(plinkentry,"INP");
		if(status) {
		    dbFinishEntry(plinkentry);
		    return(status);
		}
	    }
	    putPvLink(plinkentry,ppOpt|msOpt,pstr);
	    dbFinishEntry(plinkentry);
	}
	break;
    case FORM_OUTLINK: {
	    DBENTRY	dbEntry;
	    DBENTRY	*plinkentry = &dbEntry;
	    short	ppOpt = 0;
	    short	msOpt = 0;
	    char	*pstr;

	    pstr = *value;
	    **verify = 0;
	    value++; verify++;
	    **verify = 0;  /*Initialize verify to NULL*/
	    if((*value==NULL) || (strcmp(*value,"")==0)) ppOpt = 0;
	    else if(strstr(*value,"NPP")) ppOpt = 0;
	    else if(strstr(*value,"PP")) ppOpt = pvlOptPP;
	    else if(strstr(*value,"CA")) ppOpt = pvlOptCA;
	    else strcpy(*verify,"Illegal. Chose a value");
	    value++; verify++;
	    **verify = 0;  /*Initialize verify to NULL*/
	    if((*value==NULL) || (strcmp(*value,"")==0)) msOpt = 0;
	    else if(strstr(*value,"NMS")) msOpt = 0;
	    else if(strstr(*value,"MS")) msOpt = pvlOptMS;
	    else strcpy(*verify,"Illegal. Chose a value");
	    copyEntry(pdbentry,plinkentry);
	    if(pdbentry->pflddes->field_type == DBF_DEVICE) {
        	status = dbFindField(plinkentry,"OUT");
		if(status) {
		    dbFinishEntry(plinkentry);
		    return(status);
		}
	    }
	    putPvLink(plinkentry,ppOpt|msOpt,pstr);
	    dbFinishEntry(plinkentry);
	}
	break;
    case FORM_FWDLINK: {
	    short	ppOpt = 0;
	    short	msOpt = 0;
	    char	*pstr;

	    pstr = *value;
	    **verify = 0;
	    value++; verify++;
	    **verify = 0;  /*Initialize verify to NULL*/
	    if((*value==NULL) || (strcmp(*value,"")==0)) ppOpt = 0;
	    else if(strstr(*value,"CA")) ppOpt = pvlOptCA;
	    else strcpy(*verify,"Illegal. Chose a value");
	    putPvLink(pdbentry,ppOpt|msOpt,pstr);
	}
	break;
    case FORM_VME_IO:
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
	value++; verify++;
	status = putParmString(&plink->value.vmeio.parm,*value);
	break;
    case FORM_CAMAC_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.b = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.c = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.n = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.a = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.camacio.f = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	status = putParmString(&plink->value.camacio.parm,*value);
	break;
    case FORM_RF_IO:
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.rfio.cryo = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.rfio.micro = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.rfio.dataset = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.rfio.element = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	break;
    case FORM_AB_IO:
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
	status = putParmString(&plink->value.abio.parm,*value);
	break;
    case FORM_GPIB_IO:
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
	status = putParmString(&plink->value.gpibio.parm,*value);
	**verify = 0;
	break;
    case FORM_BITBUS_IO:
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
	status = putParmString(&plink->value.bitbusio.parm,*value);
	**verify = 0;
	break;
    case FORM_INST_IO:
	status = putParmString(&plink->value.instio.string,*value);
	**verify = 0;
	break;
    case FORM_BBGPIB_IO:
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
	status = putParmString(&plink->value.bbgpibio.parm,*value);
	**verify = 0;
	break;
    case FORM_VXI_IO:
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
	lvalue = strtol(*value,&endp,0);
	if(*endp==0) {
	    plink->value.vxiio.signal = lvalue; **verify = 0;
	} else {
	    strcpy(*verify,"Illegal. Must be number");
	}
	value++; verify++;
	status = putParmString(&plink->value.vxiio.parm,*value);
	**verify = 0;
	break;
    default :
	status = S_dbLib_badLink;
    }
    return(status);
}

char  **dbVerifyForm(DBENTRY *pdbentry,char **value)
{
    struct form *pform = pdbentry->formpvt;
    DBLINK	*plink;
    DBLINK	templink;
    int		nlines,i;

    if(!pform) return(NULL);
    plink = pform->plink;
    if(!plink) return(NULL);
    templink = *plink;
    if(plink->type==CONSTANT) templink.value.constantStr = NULL;
    if(plink->type==PV_LINK) templink.value.pv_link.pvname = NULL;
    pform->plink = &templink;
    dbPutForm(pdbentry,value);
    if(plink->type==CONSTANT) free((void *)templink.value.constantStr);
    if(plink->type==PV_LINK) free((void *)templink.value.pv_link.pvname);
    pform->plink = plink;
    nlines = pform->nlines;
    for(i=0; i<nlines; i++) if(pform->verify[i]) return(pform->verify);
    return(NULL);
}

char *dbGetRelatedField(DBENTRY *psave)
{
    DBENTRY	dbEntry;
    DBENTRY	*pdbentry= &dbEntry;
    dbFldDes	*pflddes;
    char	*rtnval = NULL;
    long	status;

    pflddes = psave->pflddes;
    if(pflddes->field_type !=DBF_DEVICE) return(NULL);
    copyEntry(psave,pdbentry);
    pflddes = pdbentry->pflddes;
    status = dbFindField(pdbentry,"INP");
    if(status) status = dbFindField(pdbentry,"OUT");
    if(!status) rtnval = pdbentry->pflddes->name;
    dbFinishEntry(pdbentry);
    return(rtnval);
}

int dbGetNLinks(DBENTRY *pdbentry)
{
    dbRecordType	*precordType = pdbentry->precordType;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    return((int)precordType->no_links);
}

long dbGetLinkField(DBENTRY *pdbentry,int index)
{
    dbRecordType	*precordType = pdbentry->precordType;
    dbFldDes	*pflddes;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    if(index<0 || index>=precordType->no_links) return(S_dbLib_badLink);
    pdbentry->indfield = precordType->link_ind[index];
    pdbentry->pflddes = pflddes = precordType->papFldDes[pdbentry->indfield];
    dbGetFieldAddress(pdbentry);
    return(0);
}

int dbGetLinkType(DBENTRY *pdbentry)
{
    dbFldDes	*pflddes;
    DBLINK	*plink;
    int		field_type;

    dbGetFieldAddress(pdbentry);
    pflddes = pdbentry->pflddes;
    if(!pflddes) return(-1);
    plink = (DBLINK *)pdbentry->pfield;
    if(!plink) return(-1);
    field_type = pflddes->field_type;
    switch (field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	switch(plink->type) {
	case CONSTANT:
	    return(DCT_LINK_CONSTANT);
	case PV_LINK:
	case DB_LINK:
	case CA_LINK:
	    return(DCT_LINK_PV);
	default:
	    return(DCT_LINK_FORM);
	}
    }
    return(-1);
}

long dbCvtLinkToConstant(DBENTRY *pdbentry)
{
    dbFldDes	*pflddes;
    DBLINK	*plink;

    dbGetFieldAddress(pdbentry);
    pflddes = pdbentry->pflddes;
    if(!pflddes) return(-1);
    plink = (DBLINK *)pdbentry->pfield;
    if(!plink) return(-1);
    switch (pflddes->field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	if(plink->type == CONSTANT) return(0);
	if(plink->type != PV_LINK) return(S_dbLib_badLink);
	free((void *)plink->value.pv_link.pvname);
	plink->value.pv_link.pvname = NULL;
	dbFreeForm(pdbentry);
	plink->type = CONSTANT;
	if(pflddes->initial) {
	    plink->value.constantStr =
		dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
	    strcpy(plink->value.constantStr,pflddes->initial);
	} else {
	    plink->value.constantStr = NULL;
	}
	return(0);
    default:
	epicsPrintf("dbCvtLinkToConstant called for non link field\n");
    }
    return(S_dbLib_badLink);
}

long dbCvtLinkToPvlink(DBENTRY *pdbentry)
{
    dbFldDes	*pflddes;
    DBLINK	*plink;

    dbGetFieldAddress(pdbentry);
    pflddes = pdbentry->pflddes;
    if(!pflddes) return(-1);
    if(!pdbentry->precnode || !pdbentry->precnode->precord) return(-1);
    plink = (DBLINK *)pdbentry->pfield;
    if(!plink) return(-1);
    switch (pflddes->field_type) {
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
	if(plink->type == PV_LINK) return(0);
	if(plink->type != CONSTANT) return(S_dbLib_badLink);
	free(plink->value.constantStr);
	dbFreeForm(pdbentry);
	plink->type = PV_LINK;
	plink->value.pv_link.pvlMask = 0;
	plink->value.pv_link.pvname = 0;
	plink->value.pv_link.precord = pdbentry->precnode->precord;
	return(0);
    default:
	epicsPrintf("dbCvtLinkToPvlink called for non link field\n");
    }
    return(S_dbLib_badLink);
}

void dbDumpPath(DBBASE *pdbbase)
{
    ELLLIST	*ppathList;
    dbPathNode	*pdbPathNode;

    if(!pdbbase) return;
    ppathList = (ELLLIST *)pdbbase->pathPvt;
    if(!ppathList || !(pdbPathNode = (dbPathNode *)ellFirst(ppathList))) {
	printf("no path defined\n");
	return;
    }
    while(pdbPathNode) {
	printf("%s",pdbPathNode->directory);
	pdbPathNode = (dbPathNode *)ellNext(&pdbPathNode->node);
	if(pdbPathNode) printf(":");
    }
    printf("\n");
    return;
}

void dbDumpRecord(dbBase *pdbbase,char *precordTypename,int level)
{
    if(!pdbbase) {
	printf("pdbbase not specified\n");
	return;
    }
    dbWriteRecordFP(pdbbase,stdout,precordTypename,level);
}

void dbDumpMenu(DBBASE *pdbbase,char *menuName)
{
    if(!pdbbase) {
	printf("pdbbase not specified\n");
	return;
    }
    dbWriteMenuFP(pdbbase,stdout,menuName);
}

void dbDumpRecordType(DBBASE *pdbbase,char *recordTypeName)
{
    dbRecordType	*pdbRecordType;
    dbFldDes	*pdbFldDes;
    int		gotMatch;
    int		i;

    if(!pdbbase) {
	printf("pdbbase not specified\n");
	return;
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType; pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	if(recordTypeName) {
	    gotMatch = (strcmp(recordTypeName,pdbRecordType->name)==0)
		? TRUE : FALSE;
	}else {
	    gotMatch=TRUE;
	}
	if(!gotMatch) continue;
	printf("name(%s) no_fields(%hd) no_prompt(%hd) no_links(%hd)\n",
	    pdbRecordType->name,pdbRecordType->no_fields,
	    pdbRecordType->no_prompt,pdbRecordType->no_links);
	printf("index name\tsortind sortname\n");
	for(i=0; i<pdbRecordType->no_fields; i++) {
	    pdbFldDes = pdbRecordType->papFldDes[i];
	    printf("%5d %s\t%7d %s\n",
		i,pdbFldDes->name,
		pdbRecordType->sortFldInd[i],pdbRecordType->papsortFldName[i]);
	}
	printf("link_ind ");
	for(i=0; i<pdbRecordType->no_links; i++)
	    printf(" %hd",pdbRecordType->link_ind[i]);
	printf("\n");
	printf("indvalFlddes %d name %s\n",pdbRecordType->indvalFlddes,
	    pdbRecordType->pvalFldDes->name);
	printf("struct rset * %p rec_size %d\n",
	    pdbRecordType->prset,pdbRecordType->rec_size);
	if(recordTypeName) break;
    }
}

void dbDumpFldDes(DBBASE *pdbbase,char *recordTypeName,char *fname)
{
    dbRecordType	*pdbRecordType;
    dbFldDes	*pdbFldDes;
    int		gotMatch;
    int		i;

    if(!pdbbase) {
	printf("pdbbase not specified\n");
	return;
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType; pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	if(recordTypeName) {
	    gotMatch = (strcmp(recordTypeName,pdbRecordType->name)==0)
		? TRUE : FALSE;
	}else {
	    gotMatch=TRUE;
	}
	if(!gotMatch) continue;
	printf("recordtype(%s) \n",pdbRecordType->name);
	for(i=0; i<pdbRecordType->no_fields; i++) {
	    int	j;

	    pdbFldDes = pdbRecordType->papFldDes[i];
	    if(fname && strcmp(fname,pdbFldDes->name)!=0) continue;
	    printf("    %s\n", pdbFldDes->name);
	    printf("\t         prompt: %s\n",pdbFldDes->prompt);
	    printf("\t          extra: %s\n",pdbFldDes->extra);
	    printf("\t      indRecordType: %hd\n",pdbFldDes->indRecordType);
	    printf("\t        special: %hd ",pdbFldDes->special);
	    if(pdbFldDes->special) {
		for(j=0; j<SPC_NTYPES; j++) {
		    if(pamapspcType[j].value == pdbFldDes->special) {
			printf("%s",pamapspcType[j].strvalue);
			break;
		    }
		}
	    }
	    printf("\n");
	    for(j=0; j<DBF_NTYPES; j++) {
		if(pamapdbfType[j].value == pdbFldDes->field_type) break;
	    }
	    if(j>=DBF_NTYPES)
		printf("\t     field_type: %d\n", pdbFldDes->field_type);
	    else
		printf("\t     field_type: %s\n", pamapdbfType[j].strvalue);
	    printf("\tprocess_passive: %hd\n",pdbFldDes->process_passive);
	    printf("\t           base: %hd\n",pdbFldDes->base);
	    if(!pdbFldDes->promptgroup) {
		printf("\t    promptgroup: %d\n",pdbFldDes->promptgroup);
	    } else {
		for(j=0; j<GUI_NTYPES; j++) {
		    if(pamapguiGroup[j].value == pdbFldDes->promptgroup) {
			printf("\t    promptgroup: %s\n",
				pamapguiGroup[j].strvalue);
			break;
		    }
		}
	    }
	    printf("\t       interest: %hd\n", pdbFldDes->interest);
	    printf("\t       as_level: %hd\n",pdbFldDes->as_level);
	    printf("\t        initial: %s\n",pdbFldDes->initial);
	    if(pdbFldDes->field_type==DBF_MENU) {
		if(pdbFldDes->ftPvt)
		    printf("\t\t  menu: %s\n",
			((dbMenu *)pdbFldDes->ftPvt)->name);
		else
		    printf("\t\t  menu: NOT FOUND\n");
	    }
	    if(pdbFldDes->field_type==DBF_DEVICE) {
		printf("\t          ftPvt: %p\n",pdbFldDes->ftPvt);
	    }
	    printf("\t           size: %hd\n",pdbFldDes->size);
	    printf("\t         offset: %hd\n",pdbFldDes->offset);
	}
	if(recordTypeName) break;
    }
}

void dbDumpDevice(DBBASE *pdbbase,char *recordTypeName)
{
    dbRecordType	*pdbRecordType;
    devSup	*pdevSup;
    int		gotMatch;

    if(!pdbbase) {
	printf("pdbbase not specified\n");
	return;
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType; pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	if(recordTypeName) {
	    gotMatch = (strcmp(recordTypeName,pdbRecordType->name)==0)
		? TRUE : FALSE;
	}else {
	    gotMatch=TRUE;
	}
	if(!gotMatch) continue;
	printf("recordtype(%s) \n",pdbRecordType->name);
	for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
	pdevSup; pdevSup = (devSup *)ellNext(&pdevSup->node)) {
	    printf("\t     name: %s\n",pdevSup->name);
	    printf("\t   choice: %s\n",pdevSup->choice);
	    printf("\tlink_type: %d\n",pdevSup->link_type);
	    printf("\t    pdset: %p\n",pdevSup->pdset);
	}
	if(recordTypeName) break;
    }
}

void dbDumpDriver(DBBASE *pdbbase)
{
    if(!pdbbase) {
	printf("pdbbase not specified\n");
	return;
    }
    dbWriteDriverFP(pdbbase,stdout);
}

void dbDumpBreaktable(DBBASE *pdbbase,char *name)
{
    brkTable	*pbrkTable;
    brkInt	*pbrkInt;
    int		ind;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    for(pbrkTable = (brkTable *)ellFirst(&pdbbase->bptList);
    pbrkTable; pbrkTable = (brkTable *)ellNext(&pbrkTable->node)) {
	if(name && strcmp(name,pbrkTable->name)!=0) continue;
	printf("breaktable(%s) {\n",pbrkTable->name);
	for(ind=0; ind<pbrkTable->number; ind++) {
	    pbrkInt = pbrkTable->papBrkInt[ind];
	    printf("\t%f %e %f\n",pbrkInt->raw,pbrkInt->slope,pbrkInt->eng);
	}
	printf("}\n");
    }
    return;
}

static char *bus[VXI_IO+1] = {"","","VME","CAMAC","AB",
	"GPIB","BITBUS","","","","","","INST","BBGPIB","VXI"};
void dbReportDeviceConfig(dbBase *pdbbase,FILE *report)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    char	busName[40];
    char	linkValue[40];
    char	dtypValue[40];
    char	cvtValue[40];
    int		ilink,nlinks;
    struct link	*plink;

    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecordType(pdbentry);
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    nlinks = dbGetNLinks(pdbentry);
	    for(ilink=0; ilink<nlinks; ilink++) {
		status = dbGetLinkField(pdbentry,ilink);
		if(status || dbGetLinkType(pdbentry)!=DCT_LINK_FORM) continue;
		plink = pdbentry->pfield;
		strcpy(busName,bus[plink->type]);
		if(strlen(busName)==0) continue;
		strcpy(linkValue,dbGetString(pdbentry));
		status = dbFindField(pdbentry,"DTYP");
		if(status) break;
		strcpy(dtypValue,dbGetString(pdbentry));
		status = dbFindField(pdbentry,"LINR");
		if(status || *((short *)pdbentry->pfield) <=1 ) {
			cvtValue[0] = 0;
		} else {
			strcpy(cvtValue,"cvt(");
			status = dbFindField(pdbentry,"EGUL");
			if(!status) strcat(cvtValue,dbGetString(pdbentry));
			status = dbFindField(pdbentry,"EGUH");
			if(!status) strcat(cvtValue,dbGetString(pdbentry));
			strcat(cvtValue,")");
		}
		fprintf(report,"%-8s %-20s %-20s %-20s %-s\n",
			busName,linkValue,dtypValue,
			dbGetRecordName(pdbentry),cvtValue);
		break;
	    }
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return;
}
