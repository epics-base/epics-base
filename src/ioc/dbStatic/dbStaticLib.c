/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "cantProceed.h"
#include "cvtFast.h"
#include "dbDefs.h"
#include "dbmf.h"
#include "ellLib.h"
#include "epicsPrint.h"
#include "epicsStdio.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "errlog.h"
#include "gpHash.h"
#include "osiFileName.h"
#include "postfix.h"

#define DBFLDTYPES_GBLSOURCE
#define SPECIAL_GBLSOURCE

#define epicsExportSharedSymbols
#include "dbChannel.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "devSup.h"
#include "drvSup.h"
#include "link.h"
#include "special.h"

int dbStaticDebug = 0;
static char *pNullString = "";
#define messagesize	276
#define RPCL_LEN INFIX_TO_POSTFIX_SIZE(80)

static char *ppstring[5]={"NPP","PP","CA","CP","CPP"};
static char *msstring[4]={"NMS","MS","MSI","MSS"};

epicsShareDef maplinkType pamaplinkType[LINK_NTYPES] = {
	{"CONSTANT",CONSTANT},
	{"PV_LINK",PV_LINK},
	{"VME_IO",VME_IO},
	{"CAMAC_IO",CAMAC_IO},
	{"AB_IO",AB_IO},
	{"GPIB_IO",GPIB_IO},
	{"BITBUS_IO",BITBUS_IO},
	{"MACRO_LINK",MACRO_LINK},
        {"PN_LINK",PN_LINK},
	{"DB_LINK",DB_LINK},
	{"CA_LINK",CA_LINK},
	{"INST_IO",INST_IO},
	{"BBGPIB_IO",BBGPIB_IO},
	{"RF_IO",RF_IO},
	{"VXI_IO",VXI_IO}
};

static int mapDBFtoDCT[DBF_NOACCESS+1] = {
	DCT_STRING,
	DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,DCT_INTEGER,
	DCT_REAL,DCT_REAL,
	DCT_INTEGER,
	DCT_MENU,
	DCT_MENUFORM,
	DCT_INLINK,DCT_OUTLINK,DCT_FWDLINK,
	DCT_NOACCESS};


/*forward references for private routines*/
static FILE *openOutstream(const char *filename);
static void finishOutstream(FILE *stream);
static long setLinkType(DBENTRY *pdbentry);
static long putParmString(char **pparm,const char *pstring);
static void entryErrMessage(DBENTRY *pdbentry,long status,char *mess);
static void zeroDbentry(DBENTRY *pdbentry);
static char *getpMessage(DBENTRY *pdbentry);
static long putPvLink(DBENTRY *pdbentry,short pvlMask,const char *pvname);
static long dbAddOnePath (DBBASE *pdbbase, const char *path, unsigned length);

/* internal routines*/
static FILE *openOutstream(const char *filename)
{
    FILE *stream;
    errno = 0;
    stream = fopen(filename,"w");
    if(!stream) {
        fprintf(stderr,"error opening %s %s\n",filename,strerror(errno));
        return 0;
    }
    return stream;
}

static void finishOutstream(FILE *stream)
{
    if(stream==stdout) {
        fflush(stdout);
    } else {
        if(fclose(stream)) fprintf(stderr,"fclose error %s\n",strerror(errno));
    }
}

static long setLinkType(DBENTRY *pdbentry)
{
    DBENTRY	dbEntry;
    dbRecordType *precordType;
    devSup	*pdevSup;
    DBLINK	*plink;
    long	status=0;
    int		link_type,ind,type;

    dbCopyEntryContents(pdbentry, &dbEntry);
    status = dbFindField(&dbEntry, "DTYP");
    if (status) {
	epicsPrintf("field DTYP does not exist for recordtype %s\n",
		dbGetRecordTypeName(&dbEntry));
	status = S_dbLib_fieldNotFound;
	goto done;
    }

    precordType = dbEntry.precordType;
    if (!precordType) {
	status = S_dbLib_badField;
	goto done;
    }

    if (ellCount(&precordType->devList) == 0) goto done;

    ind = dbGetMenuIndex(&dbEntry);
    if (ind == -1) {
	char *pstring;

	pstring = dbGetString(&dbEntry);
	if (strstr(pstring, "$(") || strstr(pstring, "${")) {
	    link_type = MACRO_LINK;
	} else {
	    status = S_dbLib_badField;
	    goto done;
	}
    } else {
	pdevSup = (devSup *)ellNth(&precordType->devList, ind + 1);
	if (!pdevSup) {
	    status = S_dbLib_badField;
	    goto done;
	}
	link_type = pdevSup->link_type;
    }

    plink = (DBLINK *)pdbentry->pfield;
    if (plink->type == link_type) goto done;

    if (plink->text)
    {
        /* re-parse link text when DTYP has changed */
        char * link_text;
        link_text = plink->text;
        plink->text = NULL;
        dbFreeLinkContents(plink);
        plink->type = link_type;
        dbPutString(pdbentry, link_text);
        free(link_text);
        goto done;
    }

    type = plink->type;
    if ((type == CONSTANT || type == PV_LINK ||
         type == PN_LINK || type == DB_LINK || type == CA_LINK) &&
	(link_type == CONSTANT || link_type == PV_LINK)) goto done;

    dbFreeLinkContents(plink);
    plink->type = link_type;
    switch (plink->type) {
        case VME_IO: plink->value.vmeio.parm = pNullString; break;
        case CAMAC_IO: plink->value.camacio.parm = pNullString; break;
	case AB_IO: plink->value.abio.parm = pNullString; break;
	case GPIB_IO: plink->value.gpibio.parm = pNullString; break;
	case BITBUS_IO: plink->value.bitbusio.parm = pNullString; break;
	case INST_IO: plink->value.instio.string = pNullString; break;
	case BBGPIB_IO: plink->value.bbgpibio.parm = pNullString; break;
	case VXI_IO: plink->value.vxiio.parm = pNullString; break;
    }
done:
    dbFinishEntry(&dbEntry);
    return(status);
}

static long putParmString(char **pparm,const char *pstring)
{
    if (*pparm && *pparm != pNullString) {
        free(*pparm);
        *pparm = pNullString;
    }
    if (!pstring) return 0;
    pstring = strchr(pstring, '@');
    if (!pstring || !*++pstring) return 0;
    *pparm = epicsStrDup(pstring);
    return 0;
}

void dbFreeLinkContents(struct link *plink)
{
    char *parm = NULL;

    switch(plink->type) {
	case CONSTANT: free((void *)plink->value.constantStr); break;
	case MACRO_LINK: free((void *)plink->value.macro_link.macroStr); break;
	case PV_LINK: free((void *)plink->value.pv_link.pvname); break;
	case VME_IO: parm = plink->value.vmeio.parm; break;
	case CAMAC_IO: parm = plink->value.camacio.parm; break;
	case AB_IO: parm = plink->value.abio.parm; break;
	case GPIB_IO: parm = plink->value.gpibio.parm; break;
	case BITBUS_IO: parm = plink->value.bitbusio.parm;break;
	case INST_IO: parm = plink->value.instio.string; break;
	case BBGPIB_IO: parm = plink->value.bbgpibio.parm;break;
	case RF_IO: break;
	case VXI_IO: parm = plink->value.vxiio.parm; break;
	default:
         epicsPrintf("dbFreeLink called but link type %d unknown\n", plink->type);
    }
    if(parm && (parm != pNullString)) free((void *)parm);
    if(plink->text) free(plink->text);
    memset((char *)plink,0,sizeof(struct link));
}

void dbFreePath(DBBASE *pdbbase)
{
    ELLLIST	*ppathList;
    dbPathNode	*pdbPathNode;

    if(!pdbbase) return;
    ppathList = (ELLLIST *)pdbbase->pathPvt;
    if(!ppathList) return;
    while((pdbPathNode = (dbPathNode *)ellFirst(ppathList))) {
	ellDelete(ppathList,&pdbPathNode->node);
	free((void *)pdbPathNode->directory);
	free((void *)pdbPathNode);
    }
    free((void *)ppathList);
    pdbbase->pathPvt = 0;
    return;
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
        if (dbIsAlias(pdbentry))
            strcat(pmessage," Record Alias:");
        else
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
    /*NOTE that pdbbase and message MUST NOT be set to NULL*/
    pdbentry->precordType=NULL;
    pdbentry->pflddes=NULL;
    pdbentry->precnode=NULL;
    pdbentry->pfield=NULL;
    pdbentry->indfield=0;
}

static char *getpMessage(DBENTRY *pdbentry)
{
    char *msg = pdbentry->message;
    if (!msg) {
        msg = dbCalloc(1, messagesize);
        pdbentry->message = msg;
    }
    *msg = 0;
    return msg;
}

static long putPvLink(DBENTRY *pdbentry,short pvlMask,const char *pvname)
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
    if(pflddes->ftPvt){
	pdbDeviceMenu = (dbDeviceMenu *)pflddes->ftPvt;
	if(pdbDeviceMenu->nChoice == ellCount(&precordType->devList))
	    return(pdbDeviceMenu);
	free((void *)pdbDeviceMenu->papChoice);
	free((void *)pdbDeviceMenu);
	pflddes->ftPvt = NULL;
    }
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

#define INC_SIZE	256
void dbCatString(char **string,int *stringLength,char *src,char *separator)
{
    if((*string==NULL)
    || ((strlen(*string)+strlen(src)+2) > (size_t)*stringLength)) {
	char	*newString;
	size_t	size;

        size = strlen(src);
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
    *stringLength += (int) strlen(separator);
    }
    strcat(*string,src);
    *stringLength += (int) strlen(src);
}

dbBase * dbAllocBase(void)
{
    dbBase	*pdbbase;

    pdbbase = dbCalloc(1,sizeof(dbBase));
    ellInit(&pdbbase->menuList);
    ellInit(&pdbbase->recordTypeList);
    ellInit(&pdbbase->drvList);
    ellInit(&pdbbase->registrarList);
    ellInit(&pdbbase->functionList);
    ellInit(&pdbbase->variableList);
    ellInit(&pdbbase->bptList);
    ellInit(&pdbbase->filterList);
    ellInit(&pdbbase->guiGroupList);
    gphInitPvt(&pdbbase->pgpHash,256);
    dbPvdInitPvt(pdbbase);
    return (pdbbase);
}
void dbFreeBase(dbBase *pdbbase)
{
    dbMenu		*pdbMenu;
    dbMenu		*pdbMenuNext;
    dbRecordType	*pdbRecordType;
    dbRecordType	*pdbRecordTypeNext;
    dbFldDes	*	pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    dbRecordNode 	*pdbRecordNodeNext;
    dbRecordAttribute	*pAttribute;
    dbRecordAttribute	*pAttributeNext;
    devSup		*pdevSup;
    devSup		*pdevSupNext;
    dbText		*ptext;
    dbText		*ptextNext;
    dbVariableDef       *pvar;
    dbVariableDef       *pvarNext;
    drvSup		*pdrvSup;
    drvSup		*pdrvSupNext;
    brkTable		*pbrkTable;
    brkTable		*pbrkTableNext;
    chFilterPlugin  *pfilt;
    chFilterPlugin  *pfiltNext;
    dbGuiGroup      *pguiGroup;
    dbGuiGroup      *pguiGroupNext;
    int			i;
    DBENTRY		dbentry;


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
	ptext = (dbText *)ellFirst(&pdbRecordType->cdefList);
	while(ptext) {
	    ptextNext = (dbText *)ellNext(&ptext->node);
	    ellDelete(&pdbRecordType->cdefList,&ptext->node);
	    free((void *)ptext->text);
	    free((void *)ptext);
	    ptext = ptextNext;
	}
	pAttribute =
	    (dbRecordAttribute *)ellFirst(&pdbRecordType->attributeList);
	while(pAttribute) {
	    pAttributeNext = (dbRecordAttribute *)ellNext(&pAttribute->node);
	    ellDelete(&pdbRecordType->attributeList,&pAttribute->node);
	    free((void *)pAttribute->name);
	    free((void *)pAttribute->pdbFldDes);
	    free(pAttribute);
	    pAttribute = pAttributeNext;
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
    ptext = (dbText *)ellFirst(&pdbbase->registrarList);
    while(ptext) {
	ptextNext = (dbText *)ellNext(&ptext->node);
	ellDelete(&pdbbase->registrarList,&ptext->node);
	free((void *)ptext->text);
	free((void *)ptext);
	ptext = ptextNext;
    }
    ptext = (dbText *)ellFirst(&pdbbase->functionList);
    while(ptext) {
	ptextNext = (dbText *)ellNext(&ptext->node);
	ellDelete(&pdbbase->functionList,&ptext->node);
	free((void *)ptext->text);
	free((void *)ptext);
	ptext = ptextNext;
    }
    pvar = (dbVariableDef *)ellFirst(&pdbbase->variableList);
    while(pvar) {
	pvarNext = (dbVariableDef *)ellNext(&pvar->node);
	ellDelete(&pdbbase->variableList,&pvar->node);
	free((void *)pvar->name);
        free((void *)pvar->type);
	free((void *)pvar);
	pvar = pvarNext;
    }
    pbrkTable = (brkTable *)ellFirst(&pdbbase->bptList);
    while(pbrkTable) {
	pbrkTableNext = (brkTable *)ellNext(&pbrkTable->node);
	gphDelete(pdbbase->pgpHash,pbrkTable->name,&pdbbase->bptList);
	ellDelete(&pdbbase->bptList,&pbrkTable->node);
	free(pbrkTable->name);
	free((void *)pbrkTable->paBrkInt);
	free((void *)pbrkTable);
	pbrkTable = pbrkTableNext;
    }
    pfilt = (chFilterPlugin *)ellFirst(&pdbbase->filterList);
    while(pfilt) {
        pfiltNext = (chFilterPlugin *)ellNext(&pfilt->node);
        free((char*)pfilt->name);
        if(pfilt->fif->priv_free)
            (*pfilt->fif->priv_free)(pfilt->puser);
        free(pfilt);
        pfilt = pfiltNext;
    }
    pguiGroup = (dbGuiGroup *)ellFirst(&pdbbase->guiGroupList);
    while (pguiGroup) {
        pguiGroupNext = (dbGuiGroup *)ellNext(&pguiGroup->node);
        gphDelete(pdbbase->pgpHash, pguiGroup->name, &pdbbase->guiGroupList);
        ellDelete(&pdbbase->guiGroupList, &pguiGroup->node);
        free(pguiGroup->name);
        free((void *)pguiGroup);
        pguiGroup = pguiGroupNext;
    }
    gphFreeMem(pdbbase->pgpHash);
    dbPvdFreeMem(pdbbase);
    dbFreePath(pdbbase);
    free((void *)pdbbase);
    pdbbase = NULL;
    return;
}

DBENTRY * dbAllocEntry(dbBase *pdbbase)
{
    DBENTRY *pdbentry;

    pdbentry = dbmfMalloc(sizeof(DBENTRY));
    memset(pdbentry,'\0',sizeof(DBENTRY));
    pdbentry->pdbbase = pdbbase;
    return(pdbentry);
}

void dbFreeEntry(DBENTRY *pdbentry)
{
    if (!pdbentry)
        return;
    if (pdbentry->message)
        free((void *)pdbentry->message);
    dbmfFree(pdbentry);
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
}

DBENTRY * dbCopyEntry(DBENTRY *pdbentry)
{
    DBENTRY *pnew;

    pnew = dbAllocEntry(pdbentry->pdbbase);
    *pnew = *pdbentry;
    pnew->message = NULL;
    return(pnew);
}

void dbCopyEntryContents(DBENTRY *pfrom,DBENTRY *pto)
{
    *pto = *pfrom;
    pto->message = NULL;
}


long dbPath(DBBASE *pdbbase,const char *path)
{
    if(!pdbbase) return(-1);
    dbFreePath(pdbbase);
    if(!path || strlen(path)==0) return(dbAddPath(pdbbase,"."));
    return(dbAddPath(pdbbase,path));
}

long dbAddPath(DBBASE *pdbbase,const char *path)
{
    ELLLIST	*ppathList;
    const char	*pcolon;
    const char	*plast;
    unsigned	expectingPath;
    unsigned	sawMissingPath;

    if(!pdbbase) return(-1);
    ppathList = (ELLLIST *)pdbbase->pathPvt;
    if(!ppathList) {
	ppathList = dbCalloc(1,sizeof(ELLLIST));
	ellInit(ppathList);
	pdbbase->pathPvt = (void *)ppathList;
    }
    if (!path) return(0); /* Empty path strings are ignored */
    /* care is taken to properly deal with white space
     * 1) preceding and trailing white space is removed from paths
     * 2) white space inbetween path separator counts as an empty name
     *		(see below)
     */
    expectingPath = FALSE;
    sawMissingPath = FALSE;
    while (*path) {
	size_t len;

	/* preceding white space is removed */
	if (isspace((int)*path)) {
	    path++;
	    continue;
	}
	pcolon = strstr (path, OSI_PATH_LIST_SEPARATOR);
	if (pcolon==path) {
	    sawMissingPath = TRUE;
	    path += strlen (OSI_PATH_LIST_SEPARATOR);
	    continue;
	}
	if (pcolon) {
	    plast = pcolon - 1;
	    expectingPath = TRUE;
	} else {
	    plast = strlen (path) + path - 1;
	    expectingPath = FALSE;
	}
	/* trailing white space is removed */
	while (isspace((int)*plast)) {
		plast--;
	}

	/*
	 * len is always nonzero because we found something that
	 * 1) isnt white space
	 * 2) isnt a path separator
	 */
	len = (plast - path) + 1;
    if (dbAddOnePath (pdbbase, path, (unsigned) len)) return (-1);
	path += len;
	if (pcolon) {
	    path += strlen(OSI_PATH_LIST_SEPARATOR);
	}
    }

    /*
     * an empty name at beginning, middle, or end of a path string that isnt
     * empty means current directory
     */
    if (expectingPath||sawMissingPath) {
	return dbAddOnePath (pdbbase, ".", 1);
    }
    return(0);
}

static long dbAddOnePath (DBBASE *pdbbase, const char *path, unsigned length)
{
    ELLLIST	*ppathList;
    dbPathNode *pdbPathNode;

    if(!pdbbase) return(-1);
    ppathList = (ELLLIST *)pdbbase->pathPvt;

    pdbPathNode = (dbPathNode *)dbCalloc(1, sizeof(dbPathNode));
    pdbPathNode->directory = (char *)dbCalloc(length+1, sizeof(char));
    strncpy(pdbPathNode->directory, path, length);
    pdbPathNode->directory[length] = '\0';
    ellAdd(ppathList, &pdbPathNode->node);
    return 0;
}

char *dbGetPromptGroupNameFromKey(DBBASE *pdbbase, const short key)
{
    dbGuiGroup *pdbGuiGroup;

    if (!pdbbase) return NULL;
    for (pdbGuiGroup = (dbGuiGroup *)ellFirst(&pdbbase->guiGroupList);
        pdbGuiGroup; pdbGuiGroup = (dbGuiGroup *)ellNext(&pdbGuiGroup->node)) {
        if (pdbGuiGroup->key == key) return pdbGuiGroup->name;
    }
    return NULL;
}

short dbGetPromptGroupKeyFromName(DBBASE *pdbbase, const char *name)
{
    GPHENTRY   *pgphentry;

    if (!pdbbase) return 0;
    pgphentry = gphFind(pdbbase->pgpHash, name, &pdbbase->guiGroupList);
    if (!pgphentry) {
        return 0;
    } else {
        return ((dbGuiGroup*)pgphentry->userPvt)->key;
    }
}


long dbWriteRecord(DBBASE *ppdbbase,const char *filename,
	const char *precordTypename,int level)
{
    FILE *stream;
    long status;

    stream = openOutstream(filename);
    if(!stream) return -1;
    status = dbWriteRecordFP(ppdbbase,stream,precordTypename,level);
    finishOutstream(stream);
    return status;
}

long dbWriteRecordFP(
    DBBASE *pdbbase,FILE *fp,const char *precordTypename,int level)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    int		dctonly;

    dctonly = ((level>1) ? FALSE : TRUE);
    dbInitEntry(pdbbase,pdbentry);
    if (precordTypename) {
        if (*precordTypename == 0 || *precordTypename == '*')
            precordTypename = 0;
    }

    if(!precordTypename) {
	status = dbFirstRecordType(pdbentry);
	if(status) {
	    /* No record descriptions, so no record instances */
    	    dbFinishEntry(pdbentry);
	    return(0);
	}
    } else {
	status = dbFindRecordType(pdbentry,precordTypename);
	if(status) {
	    fprintf(stderr,"dbWriteRecordFP: No record description for %s\n",
		precordTypename);
    	    dbFinishEntry(pdbentry);
	    return(status);
	}
    }
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
            if (dbIsAlias(pdbentry)) {
                status = dbNextRecord(pdbentry);
                continue;
            }
	    if(dbIsVisibleRecord(pdbentry))
	    	fprintf(fp,"grecord(%s,\"%s\") {\n",
		    dbGetRecordTypeName(pdbentry),dbGetRecordName(pdbentry));
	    else
	    	fprintf(fp,"record(%s,\"%s\") {\n",
		    dbGetRecordTypeName(pdbentry),dbGetRecordName(pdbentry));
	    status = dbFirstField(pdbentry,dctonly);
	    while(!status) {
		if (!dbIsDefaultValue(pdbentry) || level>0) {
		    char *pvalstring = dbGetString(pdbentry);

		    if (!pvalstring) {
			fprintf(fp,"\tfield(%s,\"\")\n",
			    dbGetFieldName(pdbentry));
		    } else {
		        fprintf(fp,"\tfield(%s,\"",
			    dbGetFieldName(pdbentry));
			epicsStrPrintEscaped(fp,pvalstring,strlen(pvalstring));
			fprintf(fp,"\")\n");
		    }
		} else if(level>0) { /*generate 0 length string*/
		    fprintf(fp,"\tfield(%s,\"\")\n",dbGetFieldName(pdbentry));
		}
		status=dbNextField(pdbentry,dctonly);
	    }
	    status = dbFirstInfo(pdbentry);
	    while(!status) {
		fprintf(fp,"\tinfo(\"%s\",\"%s\")\n",
			dbGetInfoName(pdbentry), dbGetInfoString(pdbentry));
		status=dbNextInfo(pdbentry);
	    }
	    fprintf(fp,"}\n");
	    status = dbNextRecord(pdbentry);
	}
        status = dbFirstRecord(pdbentry);
        while (!status) {
            if (!dbIsAlias(pdbentry)) {
                status = dbNextRecord(pdbentry);
                continue;
            }
            fprintf(fp, "alias(\"%s\",\"%s\")\n",
                dbRecordName(pdbentry), dbGetRecordName(pdbentry));
            status = dbNextRecord(pdbentry);
        }
	if(precordTypename) break;
	status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return(0);
}

long dbWriteMenu(
    DBBASE *ppdbbase,const char *filename,const char *menuName)
{
    FILE *stream;
    long status;

    stream = openOutstream(filename);
    status = dbWriteMenuFP(ppdbbase,stream,menuName);
    finishOutstream(stream);
    return status;
}

long dbWriteMenuFP(DBBASE *pdbbase,FILE *fp,const char *menuName)
{
    dbMenu	*pdbMenu;
    int		gotMatch;
    int		i;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    if (menuName) {
        if (*menuName == 0 || *menuName == '*')
            menuName = 0;
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

long dbWriteRecordType(
    DBBASE *pdbbase,const char *filename,const char *recordTypeName)
{
    FILE *stream;
    long status;

    stream = openOutstream(filename);
    status = dbWriteRecordTypeFP(pdbbase,stream,recordTypeName);
    finishOutstream(stream);
    return status;
}

long dbWriteRecordTypeFP(
    DBBASE *pdbbase,FILE *fp,const char *recordTypeName)
{
    dbRecordType	*pdbRecordType;
    dbFldDes	*pdbFldDes;
    int		gotMatch;
    int		i;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    if (recordTypeName) {
        if (*recordTypeName == 0 || *recordTypeName == '*')
            recordTypeName = 0;
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
	    fprintf(fp,"\tfield(%s,%s) {\n",pdbFldDes->name,
	        dbGetFieldTypeString(pdbFldDes->field_type));
	    if(pdbFldDes->prompt)
		fprintf(fp,"\t\tprompt(\"%s\")\n",pdbFldDes->prompt);
	    if(pdbFldDes->initial)
		fprintf(fp,"\t\tinitial(\"%s\")\n",pdbFldDes->initial);
        if (pdbFldDes->promptgroup) {
            fprintf(fp,"\t\tpromptgroup(\"%s\")\n",
                    dbGetPromptGroupNameFromKey(pdbbase, pdbFldDes->promptgroup));
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
	    if(pdbFldDes->prop) fprintf(fp,"\t\tprop(YES)\n");
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
    FILE *stream;
    long status;

    stream = openOutstream(filename);
    status = dbWriteDeviceFP(pdbbase,stream);
    finishOutstream(stream);
    return status;
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
    FILE *stream;
    long status;

    stream = openOutstream(filename);
    status = dbWriteDriverFP(pdbbase,stream);
    finishOutstream(stream);
    return status;
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

long dbWriteRegistrarFP(DBBASE *pdbbase,FILE *fp)
{
    dbText	*ptext;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    for(ptext = (dbText *)ellFirst(&pdbbase->registrarList);
    ptext; ptext = (dbText *)ellNext(&ptext->node)) {
	fprintf(fp,"registrar(%s)\n",ptext->text);
    }
    return(0);
}

long dbWriteFunctionFP(DBBASE *pdbbase,FILE *fp)
{
    dbText     *ptext;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    for(ptext = (dbText *)ellFirst(&pdbbase->functionList);
    ptext; ptext = (dbText *)ellNext(&ptext->node)) {
	fprintf(fp,"function(%s)\n",ptext->text);
    }
    return(0);
}

long dbWriteVariableFP(DBBASE *pdbbase,FILE *fp)
{
    dbVariableDef	*pvar;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    for(pvar = (dbVariableDef *)ellFirst(&pdbbase->variableList);
        pvar; pvar = (dbVariableDef *)ellNext(&pvar->node)) {
	fprintf(fp,"variable(%s,%s)\n",pvar->name,pvar->type);
    }
    return(0);
}

long dbWriteBreaktable(DBBASE *pdbbase,const char *filename)
{
    FILE *stream;
    long status;

    stream = openOutstream(filename);
    status = dbWriteBreaktableFP(pdbbase,stream);
    finishOutstream(stream);
    return status;
}

long dbWriteBreaktableFP(DBBASE *pdbbase,FILE *fp)
{
    brkTable	*pbrkTable;
    brkInt	*pbrkInt;
    int		i;

    if (!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return(-1);
    }
    for (pbrkTable = (brkTable *)ellFirst(&pdbbase->bptList);
	 pbrkTable;
	 pbrkTable = (brkTable *)ellNext(&pbrkTable->node)) {
	fprintf(fp,"breaktable(%s) {\n",pbrkTable->name);
	pbrkInt = pbrkTable->paBrkInt;
	for(i=0; i < pbrkTable->number; i++) {
	    fprintf(fp,"\t%e, %e\n",pbrkInt->raw,pbrkInt->eng);
	    pbrkInt++;
	}
	fprintf(fp,"}\n");
    }
    return(0);
}

long dbFindRecordType(DBENTRY *pdbentry,const char *recordType)
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

char * dbGetRecordTypeName(DBENTRY *pdbentry)
{
    return(pdbentry->precordType->name);
}

int dbGetNRecordTypes(DBENTRY *pdbentry)
{
    return(ellCount(&pdbentry->pdbbase->recordTypeList));
}

long dbPutRecordAttribute(
    DBENTRY *pdbentry, const char *name, const char*value)
{
    dbRecordType	*precordType = pdbentry->precordType;
    int			createNew = TRUE;
    int			compare;
    dbRecordAttribute	*pattribute;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    pattribute = (dbRecordAttribute *)ellFirst(&precordType->attributeList);
    /*put new attribute name in sort order*/
    while(pattribute) {
	compare = strcmp(pattribute->name,name);
	if(compare==0) {
	    createNew = FALSE;
	}
	if(compare>=0) break;
	pattribute = (dbRecordAttribute *)ellNext(&pattribute->node);
    }
    if(createNew) {
	dbRecordAttribute *pnew;
	dbFldDes	  *pdbFldDes;

	pnew = dbCalloc(1,sizeof(dbRecordAttribute));
	if(pattribute) {
	    ellInsert(&precordType->attributeList,pattribute->node.previous,
		&pnew->node);
	} else {
	    ellAdd(&precordType->attributeList,&pnew->node);
	}
	pattribute = pnew;
	pattribute->name = dbCalloc(strlen(name)+1,sizeof(char));
	strcpy(pattribute->name,name);
	pdbFldDes = dbCalloc(1,sizeof(dbFldDes));
	pdbFldDes->name = pattribute->name;
	pdbFldDes->pdbRecordType = precordType;
	pdbFldDes->special = SPC_ATTRIBUTE;
	pdbFldDes->field_type = DBF_STRING;
	pdbFldDes->as_level = ASL1;
	pdbFldDes->size = MAX_STRING_SIZE;
	pattribute->pdbFldDes = pdbFldDes;
    }
    strncpy(pattribute->value,value,MAX_STRING_SIZE);
    pattribute->value[MAX_STRING_SIZE-1] = 0;
    return(0);
}

long dbGetAttributePart(DBENTRY *pdbentry, const char **ppname)
{
    dbRecordType *precordType = pdbentry->precordType;
    const char *pname = *ppname;
    dbRecordAttribute *pattribute;

    if (!precordType)
        return S_dbLib_recordTypeNotFound;

    pattribute = (dbRecordAttribute *)ellFirst(&precordType->attributeList);
    while (pattribute) {
        size_t nameLen = strlen(pattribute->name);
        int compare = strncmp(pattribute->name, pname, nameLen);

        if (compare == 0) {
            int ch = pname[nameLen];

            if (ch != '_' && !isalnum(ch)) {
                /* Any other character can't be in the attribute name */
                pdbentry->pflddes = pattribute->pdbFldDes;
                pdbentry->pfield = pattribute->value;
                *ppname = &pname[nameLen];
                return 0;
            }
            if (strlen(pname) > nameLen) {
                compare = -1;
            }
        }
        if (compare >= 0) break;
        pattribute = (dbRecordAttribute *)ellNext(&pattribute->node);
    }
    return S_dbLib_fieldNotFound;
}

long dbGetRecordAttribute(DBENTRY *pdbentry, const char *pname)
{
    return dbGetAttributePart(pdbentry, &pname);
}

long dbFirstField(DBENTRY *pdbentry,int dctonly)
{

    pdbentry->indfield = -1;
    return(dbNextField(pdbentry,dctonly));
}

long dbNextField(DBENTRY *pdbentry,int dctonly)
{
    dbRecordType	*precordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes  		*pflddes;
    short		indfield = pdbentry->indfield;

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
		    dbGetFieldAddress(pdbentry);
		}else {
		    pdbentry->pfield = NULL;
		}
		return(0);
	    }
	}
	indfield++;
    }
}

int dbGetFieldType(DBENTRY *pdbentry)
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

char * dbGetFieldName(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(pflddes->name);
}

char * dbGetDefault(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(pflddes->initial);
}

char * dbGetPrompt(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    return(&pflddes->prompt[0]);
}

int dbGetPromptGroup(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(0);
    return(pflddes->promptgroup);
}

long dbCreateRecord(DBENTRY *pdbentry,const char *precordName)
{
    dbRecordType	*precordType = pdbentry->precordType;
    dbFldDes		*pdbFldDes;
    PVDENTRY       	*ppvd;
    ELLLIST           	*preclist = NULL;
    dbRecordNode       	*pNewRecNode = NULL;
    long		status = 0;

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
    if((status = dbAllocRecord(pdbentry,precordName))) return(status);
    pNewRecNode->recordname = dbRecordName(pdbentry);
    ellInit(&pNewRecNode->infoList);
    ellAdd(preclist, &pNewRecNode->node);
    pdbentry->precnode = pNewRecNode;
    ppvd = dbPvdAdd(pdbentry->pdbbase,precordType,pNewRecNode);
    if(!ppvd) {errMessage(-1,"Logic Err: Could not add to PVD");return(-1);}
    return(0);
}

long dbDeleteAliases(DBENTRY *pdbentry)
{
    dbBase		*pdbbase = pdbentry->pdbbase;
    dbRecordType	*precordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    ELLLIST     	*preclist = &precordType->recList;
    dbRecordNode	*pAliasNode, *pAliasNodeNext;
    DBENTRY		dbentry;
    void *precord;

    if (!precnode) return S_dbLib_recNotFound;
    if (precnode->flags & DBRN_FLAGS_ISALIAS) return S_dbLib_recExists;
    precord = precnode->precord;

    dbInitEntry(pdbbase, &dbentry);
    pAliasNode = (dbRecordNode *)ellFirst(preclist);
    while (pAliasNode) {
        pAliasNodeNext = (dbRecordNode *)ellNext(&pAliasNode->node);
        if (pAliasNode->flags & DBRN_FLAGS_ISALIAS &&
            pAliasNode->precord == precord &&
            !dbFindRecord(&dbentry, pAliasNode->recordname)) {
            dbDeleteRecord(&dbentry);
        }
        pAliasNode = pAliasNodeNext;
    }
    precnode->flags &= ~DBRN_FLAGS_HASALIAS;
    return 0;
}

long dbDeleteRecord(DBENTRY *pdbentry)
{
    dbBase		*pdbbase = pdbentry->pdbbase;
    dbRecordType	*precordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    ELLLIST     	*preclist;
    long		status;

    if (!precnode) return S_dbLib_recNotFound;
    if (precnode->flags & DBRN_FLAGS_HASALIAS)
        dbDeleteAliases(pdbentry);

    preclist = &precordType->recList;
    ellDelete(preclist, &precnode->node);
    dbPvdDelete(pdbbase, precnode);
    while (!dbFirstInfo(pdbentry)) {
        dbDeleteInfo(pdbentry);
    }
    if (precnode->flags & DBRN_FLAGS_ISALIAS) {
        free(precnode->recordname);
        precordType->no_aliases--;
    } else {
        status = dbFreeRecord(pdbentry);
        if (status) return status;
    }
    free(precnode);
    pdbentry->precnode = NULL;
    return 0;
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

long dbFindRecordPart(DBENTRY *pdbentry, const char **ppname)
{
    dbBase      *pdbbase = pdbentry->pdbbase;
    const char  *pname = *ppname;
    const char  *pfn;
    size_t      lenName;
    PVDENTRY    *ppvdNode;

    zeroDbentry(pdbentry);
    pfn = strchr(pname, '.');
    if (pfn) {
        lenName = (size_t) (pfn - pname);
    } else {
        lenName = strlen(pname);
    }

    ppvdNode = dbPvdFind(pdbbase, pname, lenName);
    if (!ppvdNode)
        return S_dbLib_recNotFound;

    pdbentry->precnode = ppvdNode->precnode;
    pdbentry->precordType = ppvdNode->precordType;
    *ppname = pname + lenName;
    return 0;
}

long dbFindRecord(DBENTRY *pdbentry, const char *pname)
{
    long status = dbFindRecordPart(pdbentry, &pname);

    if (status) return status;
    if (*pname == '.')
        return dbFindField(pdbentry, ++pname);
    return 0;
}

long dbFirstRecord(DBENTRY *pdbentry)
{
    dbRecordType	*precordType = pdbentry->precordType;
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
    dbRecordType	*precordType = pdbentry->precordType;

    if(!precordType) return(0);
    return(ellCount(&precordType->recList));
}

int dbGetNAliases(DBENTRY *pdbentry)
{
    dbRecordType	*precordType = pdbentry->precordType;

    if(!precordType) return(0);
    return(precordType->no_aliases);
}

char * dbGetRecordName(DBENTRY *pdbentry)
{
    dbRecordType *pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;

    if(!pdbRecordType) return NULL;
    if(!precnode) return NULL;
    return precnode->recordname;
}

long dbVisibleRecord(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;

    if(!precnode) return(S_dbLib_recNotFound);
    precnode->flags |= DBRN_FLAGS_VISIBLE;
    return 0;
}

long dbInvisibleRecord(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;

    if(!precnode) return(S_dbLib_recNotFound);
    precnode->flags &= ~DBRN_FLAGS_VISIBLE;
    return 0;
}

int dbIsVisibleRecord(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;

    if(!precnode) return 0;
    return precnode->flags & DBRN_FLAGS_VISIBLE ? 1 : 0;
}

long dbCreateAlias(DBENTRY *pdbentry, const char *alias)
{
    dbRecordType	*precordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbRecordNode	*pnewnode;
    PVDENTRY    	*ppvd;
    ELLLIST     	*preclist = NULL;

    if (!precordType) return S_dbLib_recordTypeNotFound;
    if (!precnode) return S_dbLib_recNotFound;
    zeroDbentry(pdbentry);
    if (!dbFindRecord(pdbentry, alias)) return S_dbLib_recExists;
    zeroDbentry(pdbentry);
    pdbentry->precordType = precordType;
    preclist = &precordType->recList;
    pnewnode = dbCalloc(1, sizeof(dbRecordNode));
    pnewnode->recordname = epicsStrDup(alias);
    pnewnode->precord = precnode->precord;
    pnewnode->flags = DBRN_FLAGS_ISALIAS;
    if (!(precnode->flags & DBRN_FLAGS_ISALIAS))
        precnode->flags |= DBRN_FLAGS_HASALIAS;
    ellInit(&pnewnode->infoList);
    ellAdd(preclist, &pnewnode->node);
    precordType->no_aliases++;
    pdbentry->precnode = pnewnode;
    ppvd = dbPvdAdd(pdbentry->pdbbase, precordType, pnewnode);
    if (!ppvd) {errMessage(-1,"Logic Err: Could not add to PVD");return(-1);}
    return 0;
}

int dbIsAlias(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;

    if(!precnode) return 0;
    return precnode->flags & DBRN_FLAGS_ISALIAS ? 1 : 0;
}

long dbCopyRecord(DBENTRY *pdbentry,const char *newRecordName,int overWriteOK)
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
    if (!precnode || dbIsAlias(pdbentry)) return S_dbLib_recNotFound;
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
    if((status = dbFindRecordType(&dbentry,precordType->name))) return(status);
    if((status = dbCreateRecord(&dbentry,newRecordName))) return(status);
    if((status = dbFirstField(pdbentry,TRUE))) return(status);
    if((status = dbFirstField(&dbentry,TRUE))) return(status);
    while(!status) {
	if(!dbIsDefaultValue(pdbentry)) {
	    pvalue = dbGetString(pdbentry);
	    if((status = dbPutString(&dbentry,pvalue))) return(status);
	}
	status = dbNextField(pdbentry,TRUE);
	if(!status) status = dbNextField(&dbentry,TRUE);
	if(!status && (pdbentry->pflddes!=dbentry.pflddes)) {
	    epicsPrintf("dbCopyRecord: Logic Error\n");
	    return(-1);
	}
    }
    /*Copy the info strings too*/
    status = dbFirstInfo(pdbentry);
    while (!status) {
	status = dbPutInfo(&dbentry, dbGetInfoName(pdbentry), dbGetInfoString(pdbentry));
	if (status) return (status);
	status = dbNextInfo(pdbentry);
    }
    /*Leave pdbentry pointing to newRecordName*/
    return(dbFindRecord(pdbentry,newRecordName));
}

long dbFindFieldPart(DBENTRY *pdbentry,const char **ppname)
{
    dbRecordType *precordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    const char   *pname = *ppname;
    short        top, bottom, test;
    char         **papsortFldName;
    short        *sortFldInd;
    int          ch;
    size_t       nameLen;

    if (!precordType) return S_dbLib_recordTypeNotFound;
    if (!precnode) return S_dbLib_recNotFound;
    papsortFldName = precordType->papsortFldName;
    sortFldInd = precordType->sortFldInd;

    /* Measure field name length; name is a valid C identifier */
    nameLen = 0;
    if ((ch = *pname) &&
        (ch == '_' || isalpha(ch))) {
        while ((ch = pname[++nameLen]))
            if (!(ch == '_' || isalnum(ch))) break;
    }

    /* Handle absent field name */
    if (nameLen == 0) {
        dbFldDes *pflddes = precordType->pvalFldDes;

        if (!pflddes)
            return S_dbLib_recordTypeNotFound;
        pdbentry->pflddes = pflddes;
        pdbentry->indfield = precordType->indvalFlddes;
        *ppname = &pname[nameLen];
        return dbGetFieldAddress(pdbentry);
    }

    /* binary search through ordered field names */
    top = precordType->no_fields - 1;
    bottom = 0;
    test = (top + bottom) / 2;
    while (1) {
        int compare = strncmp(papsortFldName[test], pname, nameLen);
        if (compare == 0)
            compare = (int) (strlen(papsortFldName[test]) - nameLen);
        if (compare == 0) {
            dbFldDes *pflddes = precordType->papFldDes[sortFldInd[test]];

            if (!pflddes)
                return S_dbLib_recordTypeNotFound;
            pdbentry->pflddes = pflddes;
            pdbentry->indfield = sortFldInd[test];
            *ppname = &pname[nameLen];
            return dbGetFieldAddress(pdbentry);
        } else if (compare > 0) {
            top = test - 1;
            if (top < bottom) break;
            test = (top + bottom) / 2;
        } else {
            bottom = test + 1;
            if (top < bottom) break;
            test = (top + bottom) / 2;
        }
    }
    return S_dbLib_fieldNotFound;
}

long dbFindField(DBENTRY *pdbentry,const char *pname)
{
    long status = dbFindFieldPart(pdbentry, &pname);
    int ch;

    if (status == S_dbLib_fieldNotFound)
        return dbGetRecordAttribute(pdbentry, pname);
    if (status) return status;

    ch = *pname;
    if (ch == 0 || isspace(ch)) return 0;
    return S_dbLib_recNotFound;
}

int dbFoundField(DBENTRY *pdbentry)
{ return((pdbentry->pfield) ? TRUE : FALSE); }

char * dbGetString(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    char	*message;
    DBLINK 	*plink;

    message = getpMessage(pdbentry);
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
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
    case DBF_MENU:
    case DBF_DEVICE:
	return(dbGetStringNum(pdbentry));
    case DBF_INLINK:
    case DBF_OUTLINK:
	if(!pfield) {strcpy(message,"Field not found"); return(message);}
	plink = (DBLINK *)pfield;
	switch(plink->type) {
	    case CONSTANT:
		if(plink->value.constantStr) {
		    strcpy(message,plink->value.constantStr);
		} else {
		    strcpy(message,"");
		}
		break;
	    case MACRO_LINK:
		if(plink->value.macro_link.macroStr) {
		    strcpy(message,plink->value.macro_link.macroStr);
		} else {
		    strcpy(message,"");
		}
		break;
            case PN_LINK:
		if(plink->value.pv_link.pvname)
		    strcpy(message,plink->value.pv_link.pvname);
		else
		    strcpy(message,"");
		strcat(message," ");
		strcat(message,msstring[plink->value.pv_link.pvlMask&pvlOptMsMode]);
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
		if (plink->value.pv_link.pvname) {
		    strcpy(message, plink->value.pv_link.pvname);
		    if (pvlMask & pvlOptTSELisTime)
			strcat(message, ".TIME");
		} else
		    strcpy(message,"");
		strcat(message," ");
		strcat(message,ppstring[ppind]);
		strcat(message," ");
		strcat(message,msstring[pvlMask&pvlOptMsMode]);
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
	break;
    case DBF_FWDLINK: {
	    DBLINK *plink=(DBLINK *)pfield;

	    if(!pfield) {strcpy(message,"Field not found"); return(message);}
	    switch(plink->type) {
	    case CONSTANT:
		strcpy(message,"0");
		break;
	    case MACRO_LINK:
		if(plink->value.macro_link.macroStr) {
		    strcpy(message,plink->value.macro_link.macroStr);
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

static void cvtDecimalOrHexToShort(char *from,short *value)
{
    if(strspn(from,"0x")==2 || strspn(from,"0X")==2) {
        sscanf(from,"%hi",value);
    } else {
        sscanf(from,"%hd",value);
    }
}

long dbPutString(DBENTRY *pdbentry,const char *pstring)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    long	status=0;
    int		macroIsOk;
    int		stringHasMacro=FALSE;

    if(!pflddes) return(S_dbLib_flddesNotFound);
    macroIsOk = dbIsMacroOk(pdbentry);
    stringHasMacro = strstr(pstring,"$(") || strstr(pstring,"${");
    if(!macroIsOk && stringHasMacro) {
	epicsPrintf("%s.%s Has unexpanded macro\n",
		dbGetRecordName(pdbentry),dbGetFieldName(pdbentry));
	return(S_dbLib_badField);
    }
    switch (pflddes->field_type) {
    case DBF_STRING:
	if(!pfield) return(S_dbLib_fieldNotFound);
	if(strlen(pstring) >= (size_t)pflddes->size) return S_dbLib_strLen;
	strncpy((char *)pfield, pstring, pflddes->size-1);
        ((char *)pfield)[pflddes->size-1] = 0;

	if((pflddes->special == SPC_CALC) && !stringHasMacro) {
	    char  rpcl[RPCL_LEN];
	    short err;

	    if (postfix(pstring,rpcl,&err)) {
		status = S_dbLib_badField;
		errlogPrintf("%s in CALC expression '%s'\n",
			      calcErrorStr(err), pstring);
	    }
	}
	break;

    case DBF_CHAR:
    case DBF_SHORT:
    case DBF_LONG:
    case DBF_UCHAR:
    case DBF_USHORT:
    case DBF_ULONG:
    case DBF_ENUM:
    case DBF_FLOAT:
    case DBF_DOUBLE:
    case DBF_MENU:
        status = dbPutStringNum(pdbentry,pstring);
        break;

    case DBF_DEVICE:  {
	    DBENTRY	dbEntry;
	    char	*name;

	    status = dbPutStringNum(pdbentry, pstring);
	    if (status) return status;

	    name = dbGetRelatedField(pdbentry);
	    if (!name) return 0;

	    dbCopyEntryContents(pdbentry, &dbEntry);
	    status = dbFindField(&dbEntry, name);
	    if (!status)
		status = setLinkType(&dbEntry);
	    dbFinishEntry(&dbEntry);
	    return status;
	}

    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK: {
	    DBLINK	*plink;
        char	string[256];
	    char	*pstr = string;

	    if (!pfield)
	        return S_dbLib_fieldNotFound;

	    plink = (DBLINK *)pfield;
	    dbFreeLinkContents(plink);
	    if (stringHasMacro) {
		plink->type = MACRO_LINK;
		plink->value.macro_link.macroStr = epicsStrDup(pstring);
		goto done;
	    }

	    if (strcmp(pflddes->name, "INP") == 0 ||
		strcmp(pflddes->name, "OUT") == 0) {
		status = setLinkType(pdbentry); /* This uses DTYP to look up and set plink->type, necessary for default DTYP */
		if (status) {
		    errMessage(status,"in dbPutString from setLinkType");
		    return status;
		}
                /* store link text in case DTYP changes later */
                plink->text = malloc(strlen(pstring) + 1);
                if (plink->text)
                    strcpy(plink->text, pstring);
	    }
	    if (strlen(pstring) >= sizeof(string)) {
	        status = S_dbLib_badField;
	        errMessage(status,
			"dbPutString received a string that is too long");
	        return status;
            }
	    strcpy(pstr, pstring);
	    /* Strip leading blanks and tabs */
	    while (*pstr && (*pstr == ' ' || *pstr == '\t')) pstr++;
	    /* Strip trailing blanks and tabs */
	    if (pstr) {
		int ind;

	        for (ind = (int) strlen(pstr) - 1; ind >= 0; ind--) {
		    if (pstr[ind] != ' ' && pstr[ind] != '\t') break;
		    pstr[ind] = '\0';
		}
	    }
	    if (!pstr || !*pstr) {
		if (plink->type == PV_LINK) dbCvtLinkToConstant(pdbentry);
		if (plink->type != CONSTANT) return S_dbLib_badField;
		free((void *)plink->value.constantStr);
		plink->value.constantStr = NULL;
		goto done;
	    }
	    switch (plink->type) {
	    case CONSTANT:
	    case PV_LINK: {
	    	    short	ppOpt = 0;
		    short	msOpt = 0;
	    	    char	*end;
	    	    char	*enddouble;
	    	    char	*endlong;

		    /* Check first to see if string is a constant*/
		    /*It is a string if epicsStrtod or strtol eats entire string*/
		    /*leading and trailing blanks have already been stripped*/
		    (void) epicsStrtod(pstr, &enddouble);
		    (void) strtol(pstr, &endlong, 0);

		    if (*enddouble == 0 || *endlong == 0) {
			if (plink->type == PV_LINK) dbCvtLinkToConstant(pdbentry);
			if (!plink->value.constantStr ||
			    strlen(plink->value.constantStr) < strlen(pstr)) {
			    free(plink->value.constantStr);
			    plink->value.constantStr =
				dbCalloc(strlen(pstr) + 1, sizeof(char));
			}
			strcpy(plink->value.constantStr, pstr);
			goto done;
		    }

		    if (plink->type==CONSTANT) dbCvtLinkToPvlink(pdbentry);
	    	    end = strchr(pstr,' ');
		    if (end) {
			switch (pflddes->field_type) {
			case DBF_INLINK: {
			    if (strstr(end, "NPP")) ppOpt = 0;
			    else if (strstr(end, "CPP")) ppOpt = pvlOptCPP;
			    else if (strstr(end, "PP")) ppOpt = pvlOptPP;
			    else if (strstr(end, "CA")) ppOpt = pvlOptCA;
			    else if (strstr(end, "CP")) ppOpt = pvlOptCP;
			    else ppOpt = 0;
		            if (strstr(end, "NMS")) msOpt = pvlOptNMS;
		            else if (strstr(end, "MSI")) msOpt = pvlOptMSI;
		            else if (strstr(end, "MSS")) msOpt = pvlOptMSS;
/*must be the last one:*/   else if (strstr(end, "MS")) msOpt = pvlOptMS;
			    else msOpt = 0;
		            *end = 0;
			}
			break;

			case DBF_OUTLINK: {
			    if (strstr(end, "NPP")) ppOpt = 0;
			    else if(strstr(end, "PP")) ppOpt = pvlOptPP;
			    else if(strstr(end, "CA")) ppOpt = pvlOptCA;
			    else ppOpt = 0;
		            if (strstr(end, "NMS")) msOpt = pvlOptNMS;
		            else if(strstr(end, "MSI")) msOpt = pvlOptMSI;
		            else if(strstr(end, "MSS")) msOpt = pvlOptMSS;
/*must be the last one:*/   else if(strstr(end, "MS")) msOpt = pvlOptMS;
			    else msOpt = 0;
		            *end = 0;
			}
			break;

			case DBF_FWDLINK: {
			    if (strstr(end, "NPP")) ppOpt = 0;
			    else if (strstr(end, "CA")) ppOpt = pvlOptCA;
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
		    goto done;
		}
		/*break; is unnecessary*/
	    case VME_IO: {
	    	    char	*end;

		    if (!(end = strchr(pstr,'#'))) return S_dbLib_badField;
		    pstr = end + 1;
		    if (!(end = strchr(pstr,'C'))) return S_dbLib_badField;
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.vmeio.card);
		    if (!(end = strchr(pstr,'S'))) return S_dbLib_badField;
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr, &plink->value.vmeio.signal);
		    status = putParmString(&plink->value.vmeio.parm, pstr);
		}
		break;

	    case CAMAC_IO: {
	    	    char	*end;

		    if (!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if (!(end = strchr(pstr,'B'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.camacio.b);
		    if (!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.camacio.c);
		    if (!(end = strchr(pstr,'N'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.camacio.n);
 		    if (!(end = strchr(pstr,'A')))  {
 			plink->value.camacio.a = 0;
 		    } else {
 		        pstr = end + 1;
                        cvtDecimalOrHexToShort(pstr,&plink->value.camacio.a);
 		    }
 		    if (!(end = strchr(pstr,'F'))) {
 			plink->value.camacio.f = 0;
 		    } else {
 		        pstr = end + 1;
                        cvtDecimalOrHexToShort(pstr,&plink->value.camacio.f);
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
                    cvtDecimalOrHexToShort(pstr,&plink->value.rfio.cryo);
		    if(!(end = strchr(pstr,'M'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.rfio.micro);
		    if(!(end = strchr(pstr,'D'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.rfio.dataset);
		    if(!(end = strchr(pstr,'E'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.rfio.element);
		}
		break;
	    case AB_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.abio.link);
		    if(!(end = strchr(pstr,'A'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.abio.adapter);
		    if(!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.abio.card);
                    if(!(end = strchr(pstr,'S'))) return (S_dbLib_badField);
                    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.abio.signal);
                    status = putParmString(&plink->value.abio.parm,pstr);
		}
                break;
            case GPIB_IO: {
                    char *end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    if(!(end = strchr(pstr,'L'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.gpibio.link);
		    if(!(end = strchr(pstr,'A'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&plink->value.gpibio.addr);
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
                    cvtDecimalOrHexToShort(pstr,&tmp_val);
		    plink->value.bitbusio.link=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'N'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&tmp_val);
		    plink->value.bitbusio.node=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'P'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&tmp_val);
		    plink->value.bitbusio.port=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'S'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&tmp_val);
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
                    cvtDecimalOrHexToShort(pstr,&tmp_val);
		    plink->value.bbgpibio.link=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'B'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&tmp_val);
		    plink->value.bbgpibio.bbaddr=(unsigned char)tmp_val;
		    if(!(end = strchr(pstr,'G'))) return (S_dbLib_badField);
		    pstr = end + 1;
                    cvtDecimalOrHexToShort(pstr,&tmp_val);
            plink->value.bbgpibio.gpibaddr=(unsigned char)tmp_val;
		    status = putParmString(&plink->value.bbgpibio.parm,pstr);
		}
		break;
	    case VXI_IO: {
	    	    char	*end;

		    if(!(end = strchr(pstr,'#'))) return (S_dbLib_badField);
		    pstr = end + 1;
		    memset((char *)&plink->value.vxiio,0,sizeof(struct  vxiio));
		    plink->value.vxiio.parm = pNullString;
		    if(!((end = strchr(pstr,'C'))&&(end < strchr(pstr,'@')) )) {
			plink->value.vxiio.flag = VXISTATIC;
		        if(!(end = strchr(pstr,'V'))) return (S_dbLib_badField);
		        pstr = end + 1;
                        cvtDecimalOrHexToShort(pstr,&plink->value.vxiio.la);
		    } else {
			plink->value.vxiio.flag = VXIDYNAMIC;
		        if(!(end = strchr(pstr,'V'))) return (S_dbLib_badField);
		        pstr = end + 1;
                        cvtDecimalOrHexToShort(pstr,&plink->value.vxiio.frame);
		        if(!(end = strchr(pstr,'C'))) return (S_dbLib_badField);
		        pstr = end + 1;
                        cvtDecimalOrHexToShort(pstr,&plink->value.vxiio.slot);
		    }
		    if((end = strchr(pstr,'S'))) {
			pstr = end + 1;
                        cvtDecimalOrHexToShort(pstr,&plink->value.vxiio.signal);
		    } else {
			plink->value.vxiio.signal = 0;
		    }
		    status = putParmString(&plink->value.vxiio.parm,pstr);
		}
		break;
	    case INST_IO: {
		    status = putParmString(&plink->value.instio.string, pstr);
		}
		break;
	    }
	}
	break;
    default:
	return S_dbLib_badField;
    }
done:
    if (!status && strcmp(pflddes->name, "VAL") == 0) {
	DBENTRY	dbentry;

	dbCopyEntryContents(pdbentry, &dbentry);
	if (!dbFindField(&dbentry, "UDF")) {
	    dbPutString(&dbentry, "0");
	}
	dbFinishEntry(&dbentry);
    }
    return(status);
}

char * dbVerify(DBENTRY *pdbentry,const char *pstring)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    char	*message;
    int		stringHasMacro=FALSE;

    stringHasMacro = strstr(pstring,"$(") || strstr(pstring,"${");
    message = getpMessage(pdbentry);
    if(!pflddes) {strcpy(message,"fldDes not found"); return(message);}
    if(strstr(pstring,"$(") || strstr(pstring,"${")) return(NULL);
    switch (pflddes->field_type) {
    case DBF_STRING: {
	    size_t length;

	    length=strlen(pstring);
	    if(length>=pflddes->size) {
		sprintf(message,"string to big. max=%hd",pflddes->size);
		return(message);
	    }
	    if((pflddes->special == SPC_CALC) && !stringHasMacro) {
		char  rpcl[RPCL_LEN];
		short err;
		long  status;

		status = postfix(pstring,rpcl,&err);
		if(status)  {
		    sprintf(message,"%s in CALC expression '%s'",
			    calcErrorStr(err), pstring);
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
	    char  *endp;

	    (void) epicsStrtod(pstring,&endp);
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

long dbFirstInfo(DBENTRY *pdbentry)
{
    dbRecordNode *precnode = pdbentry->precnode;

    pdbentry->pinfonode = NULL;
    if (!precnode) return (S_dbLib_recNotFound);

    pdbentry->pinfonode = (dbInfoNode *)ellFirst(&precnode->infoList);
    return (pdbentry->pinfonode ? 0 : S_dbLib_infoNotFound);
}

long dbNextInfo(DBENTRY *pdbentry)
{
    dbRecordNode *precnode = pdbentry->precnode;
    dbInfoNode *pinfo;

    if (!precnode) return (S_dbLib_recNotFound);
    pinfo = pdbentry->pinfonode;
    if (!pinfo) return (S_dbLib_infoNotFound);

    pinfo = (dbInfoNode *)ellNext(&pinfo->node);
    pdbentry->pinfonode = pinfo;
    return (pinfo ? 0 : S_dbLib_infoNotFound);
}

long dbFindInfo(DBENTRY *pdbentry,const char *name)
{
    dbRecordNode *precnode = pdbentry->precnode;
    dbInfoNode *pinfo;

    pdbentry->pinfonode = NULL;
    if (!precnode) return(S_dbLib_recNotFound);

    pinfo = (dbInfoNode *)ellFirst(&precnode->infoList);
    while (pinfo) {
	if (!strcmp(pinfo->name, name)) {
	    pdbentry->pinfonode = pinfo;
	    return (0);
	}
	pinfo = (dbInfoNode *)ellNext(&pinfo->node);
    }
    return (S_dbLib_infoNotFound);
}

long dbDeleteInfo(DBENTRY *pdbentry)
{
    dbRecordNode	*precnode = pdbentry->precnode;
    dbInfoNode		*pinfo = pdbentry->pinfonode;

    if (!precnode) return (S_dbLib_recNotFound);
    if (!pinfo) return (S_dbLib_infoNotFound);
    ellDelete(&precnode->infoList,&pinfo->node);
    free(pinfo->name);
    free(pinfo->string);
    free(pinfo);
    pdbentry->pinfonode = NULL;
    return (0);
}

const char * dbGetInfoName(DBENTRY *pdbentry)
{
    dbInfoNode *pinfo = pdbentry->pinfonode;
    if (!pinfo) return (NULL);
    return (pinfo->name);
}

const char * dbGetInfoString(DBENTRY *pdbentry)
{
    dbInfoNode *pinfo = pdbentry->pinfonode;
    if (!pinfo) return (NULL);
    return (pinfo->string);
}

long dbPutInfoString(DBENTRY *pdbentry,const char *string)
{
    dbInfoNode *pinfo = pdbentry->pinfonode;
    char *newstring;
    if (!pinfo) return (S_dbLib_infoNotFound);
    newstring = realloc(pinfo->string,1+strlen(string));
    if (!newstring) return (S_dbLib_outMem);
    strcpy(newstring, string);
    pinfo->string = newstring;
    return (0);
}

long dbPutInfoPointer(DBENTRY *pdbentry, void *pointer)
{
    dbInfoNode *pinfo = pdbentry->pinfonode;
    if (!pinfo) return (S_dbLib_infoNotFound);
    pinfo->pointer = pointer;
    return (0);
}

void * dbGetInfoPointer(DBENTRY *pdbentry)
{
    dbInfoNode *pinfo = pdbentry->pinfonode;
    if (!pinfo) return (NULL);
    return (pinfo->pointer);
}

const char * dbGetInfo(DBENTRY *pdbentry,const char *name)
{
    if (dbFindInfo(pdbentry, name)) return NULL;
    return dbGetInfoString(pdbentry);
}

long dbPutInfo(DBENTRY *pdbentry,const char *name,const char *string)
{
    dbInfoNode *pinfo;
    dbRecordNode *precnode = pdbentry->precnode;
    if (!precnode) return (S_dbLib_recNotFound);

    dbFindInfo(pdbentry, name);
    pinfo = pdbentry->pinfonode;
    if (pinfo) return (dbPutInfoString(pdbentry, string));

    /*Create new info node*/
    pinfo = calloc(1,sizeof(dbInfoNode));
    if (!pinfo) return (S_dbLib_outMem);
    pinfo->name = calloc(1,1+strlen(name));
    if (!pinfo->name) {
	free(pinfo);
	return (S_dbLib_outMem);
    }
    strcpy(pinfo->name, name);
    pinfo->string = calloc(1,1+strlen(string));
    if (!pinfo->string) {
	free(pinfo->name);
	free(pinfo);
	return (S_dbLib_outMem);
    }
    strcpy(pinfo->string, string);
    ellAdd(&precnode->infoList,&pinfo->node);
    pdbentry->pinfonode = pinfo;
    return (0);
}

brkTable * dbFindBrkTable(dbBase *pdbbase,const char *name)
{
    GPHENTRY *pgph;

    pgph = gphFind(pdbbase->pgpHash,name,(void *)&pdbbase->bptList);
    if(!pgph) return(NULL);
    return((brkTable *)pgph->userPvt);
}

const char * dbGetFieldTypeString(int dbfType)
{
    int i;

    for (i=0; i < DBF_NTYPES; i++) {
        if (pamapdbfType[i].value == dbfType) {
            return pamapdbfType[i].strvalue;
        }
    }
    return "BAD_DBF_TYPE";
}

int dbFindFieldType(const char *type)
{
    int i;

    for (i = 0; i < DBF_NTYPES; i++) {
        if (strcmp(type, pamapdbfType[i].strvalue) == 0) {
            return pamapdbfType[i].value;
        }
    }
    return -1;
}

dbMenu * dbFindMenu(dbBase *pdbbase,const char *name)
{
    GPHENTRY *pgph;

    pgph = gphFind(pdbbase->pgpHash,name,(void *)&pdbbase->menuList);
    if(!pgph) return(NULL);
    return((dbMenu *)pgph->userPvt);
}

char   ** dbGetMenuChoices(DBENTRY *pdbentry)
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

char * dbGetMenuStringFromIndex(DBENTRY *pdbentry, int index)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;

    if(!pflddes) return(NULL);
    switch (pflddes->field_type) {
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;

	    if(!pdbMenu) return(NULL);
	    if(index<0 || index>=pdbMenu->nChoice) return(NULL);
	    return(pdbMenu->papChoiceValue[index]);
	}
    case DBF_DEVICE: {
	    dbDeviceMenu *pdbDeviceMenu;

	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(NULL);
	    if(index<0 || index>=pdbDeviceMenu->nChoice) return(NULL);
	    return(pdbDeviceMenu->papChoice[index]);
	}
    default:
	break;
    }
    return (NULL);
}

int dbGetMenuIndexFromString(DBENTRY *pdbentry, const char *choice)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    int		ind;
    int		nChoice = 0;
    char	**papChoice = NULL;

    if(!pflddes) return(-1);
    switch (pflddes->field_type) {
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;

	    if(!pdbMenu) return(-1);
	    papChoice = pdbMenu->papChoiceValue;
	    nChoice = pdbMenu->nChoice;
	    break;
	}
    case DBF_DEVICE: {
	    dbDeviceMenu *pdbDeviceMenu;

	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(-1);
	    papChoice = pdbDeviceMenu->papChoice;
	    nChoice = pdbDeviceMenu->nChoice;
	    break;
	}
    default:
	return(-1);
    }
    if(nChoice<=0 || !papChoice) return(-1);
    for(ind=0; ind<nChoice; ind++) {
	if(strcmp(choice,papChoice[ind])==0) return(ind);
    }
    return (-1);
}

drvSup * dbFindDriver(dbBase *pdbbase, const char *name) {
    GPHENTRY *pgph = gphFind(pdbbase->pgpHash,name,&pdbbase->drvList);
    if (!pgph) return NULL;
    return (drvSup *) pgph->userPvt;
}

char * dbGetRelatedField(DBENTRY *psave)
{
    DBENTRY	dbEntry;
    DBENTRY	*pdbentry= &dbEntry;
    dbFldDes	*pflddes;
    char	*rtnval = NULL;
    long	status;

    pflddes = psave->pflddes;
    if(pflddes->field_type !=DBF_DEVICE) return(NULL);
    dbCopyEntryContents(psave,pdbentry);
    pflddes = pdbentry->pflddes;
    status = dbFindField(pdbentry,"INP");
    if(status) status = dbFindField(pdbentry,"OUT");
    if(!status) rtnval = pdbentry->pflddes->name;
    dbFinishEntry(pdbentry);
    return(rtnval);
}

int  dbGetNLinks(DBENTRY *pdbentry)
{
    dbRecordType	*precordType = pdbentry->precordType;

    if(!precordType) return(S_dbLib_recordTypeNotFound);
    return((int)precordType->no_links);
}

long  dbGetLinkField(DBENTRY *pdbentry,int index)
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

int  dbGetLinkType(DBENTRY *pdbentry)
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
        case PN_LINK:
	case DB_LINK:
	case CA_LINK:
	    return(DCT_LINK_PV);
	default:
	    return(DCT_LINK_FORM);
	}
    }
    return(-1);
}

long  dbCvtLinkToConstant(DBENTRY *pdbentry)
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

long  dbCvtLinkToPvlink(DBENTRY *pdbentry)
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

void  dbDumpPath(DBBASE *pdbbase)
{
    ELLLIST	*ppathList;
    dbPathNode	*pdbPathNode;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    ppathList = (ELLLIST *)pdbbase->pathPvt;
    if(!ppathList || !(pdbPathNode = (dbPathNode *)ellFirst(ppathList))) {
	printf("no path defined\n");
	return;
    }
    while(pdbPathNode) {
	printf("%s",pdbPathNode->directory);
	pdbPathNode = (dbPathNode *)ellNext(&pdbPathNode->node);
	if(pdbPathNode) printf("%s", OSI_PATH_LIST_SEPARATOR);
    }
    printf("\n");
    return;
}

void  dbDumpRecord(
    dbBase *pdbbase,const char *precordTypename,int level)
{
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    dbWriteRecordFP(pdbbase,stdout,precordTypename,level);
}

void  dbDumpMenu(dbBase *pdbbase,const char *menuName)
{
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    dbWriteMenuFP(pdbbase,stdout,menuName);
}

void dbDumpRecordType(DBBASE *pdbbase,const char *recordTypeName)
{
    dbRecordType *pdbRecordType;
    dbFldDes	*pdbFldDes;
    int		gotMatch;
    int		i;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
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
	    (void *)pdbRecordType->prset,pdbRecordType->rec_size);
	if(recordTypeName) break;
    }
}

void  dbDumpField(
    DBBASE *pdbbase,const char *recordTypeName,const char *fname)
{
    dbRecordType *pdbRecordType;
    dbFldDes	*pdbFldDes;
    int		gotMatch;
    int		i;
    dbRecordAttribute *pAttribute;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
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
            printf("\t         prompt: %s\n",
                (pdbFldDes->prompt ? pdbFldDes->prompt : ""));
            printf("\t          extra: %s\n",
                (pdbFldDes->extra ? pdbFldDes->extra: ""));
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
            printf("\t     field_type: %s\n",
                dbGetFieldTypeString(pdbFldDes->field_type));
	    printf("\tprocess_passive: %u\n",pdbFldDes->process_passive);
	    printf("\t       property: %u\n",pdbFldDes->prop);
	    printf("\t           base: %d\n",pdbFldDes->base);
	    if(!pdbFldDes->promptgroup) {
		printf("\t    promptgroup: %d\n",pdbFldDes->promptgroup);
	    } else {
            printf("\t    promptgroup: %s\n",
                    dbGetPromptGroupNameFromKey(pdbbase, pdbFldDes->promptgroup));
        }
	    printf("\t       interest: %hd\n", pdbFldDes->interest);
	    printf("\t       as_level: %d\n",pdbFldDes->as_level);
            printf("\t        initial: %s\n",
                (pdbFldDes->initial ? pdbFldDes->initial : ""));
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
	pAttribute =
	    (dbRecordAttribute *)ellFirst(&pdbRecordType->attributeList);
	while(pAttribute) {
	    printf("Attribute: name %s value %s\n",
		pAttribute->name,pAttribute->value);
	    pAttribute = (dbRecordAttribute *)ellNext(&pAttribute->node);
	}
	if(recordTypeName) break;
    }
}

void  dbDumpDevice(DBBASE *pdbbase,const char *recordTypeName)
{
    dbRecordType *pdbRecordType;
    devSup	*pdevSup;
    int		gotMatch;

    if (recordTypeName) {
        if (*recordTypeName == 0 || *recordTypeName == '*')
            recordTypeName = 0;
    }
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
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
	printf("recordtype(%s)\n",pdbRecordType->name);
	for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
	pdevSup; pdevSup = (devSup *)ellNext(&pdevSup->node)) {
	    printf("    device name:   %s\n",pdevSup->name);
	    printf("\tchoice:    %s\n",pdevSup->choice);
	    printf("\tlink_type: %d\n",pdevSup->link_type);
	    printf("\tpdset:     %p\n",(void *)pdevSup->pdset);
            if (pdevSup->pdset) {
                static const char *names[] = {
                    " - report()",
                    " - init()",
                    " - init_record()",
                    " - get_ioint_info()"
                };
                int i, n = pdevSup->pdset->number;
                DEVSUPFUN *pfunc = &pdevSup->pdset->report;

                printf("\t    number: %d\n", n);
                for (i = 0; i < n; ++i, ++pfunc) {
                    const char *name = (i < NELEMENTS(names)) ? names[i] : "";

                    printf("\t    func %d: %p%s\n", i, (void *)*pfunc, name);
                }
            }
	    printf("\tpdsxt:     %p\n",(void *)pdevSup->pdsxt);
            if (pdevSup->pdsxt) {
                printf("\t    add_record: %p\n",
                    (void *)pdevSup->pdsxt->add_record);
                printf("\t    del_record: %p\n",
                    (void *)pdevSup->pdsxt->del_record);
            }
	}
	if(recordTypeName) break;
    }
}

void  dbDumpDriver(DBBASE *pdbbase)
{
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    dbWriteDriverFP(pdbbase,stdout);
}

void  dbDumpRegistrar(DBBASE *pdbbase)
{
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    dbWriteRegistrarFP(pdbbase,stdout);
}

void  dbDumpFunction(DBBASE *pdbbase)
{
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    dbWriteFunctionFP(pdbbase,stdout);
}

void  dbDumpVariable(DBBASE *pdbbase)
{
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    dbWriteVariableFP(pdbbase,stdout);
}

void  dbDumpBreaktable(DBBASE *pdbbase,const char *name)
{
    brkTable *pbrkTable;
    brkInt   *pbrkInt;
    int	     ind;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    for(pbrkTable = (brkTable *)ellFirst(&pdbbase->bptList);
    pbrkTable; pbrkTable = (brkTable *)ellNext(&pbrkTable->node)) {
	if (name && strcmp(name,pbrkTable->name)!=0) continue;
	printf("breaktable(%s) {\n",pbrkTable->name);
	pbrkInt = pbrkTable->paBrkInt;
	for(ind=0; ind < pbrkTable->number; ind++) {
	    printf("\traw=%f slope=%e eng=%f\n",
		   pbrkInt->raw, pbrkInt->slope, pbrkInt->eng);
	    pbrkInt++;
	}
	printf("}\n");
    }
    return;
}

static char *bus[VXI_IO+1] = {"","","VME","CAMAC","AB",
	"GPIB","BITBUS","","","","","","INST","BBGPIB","VXI"};
void  dbReportDeviceConfig(dbBase *pdbbase,FILE *report)
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    char	linkValue[messagesize];
    char	dtypValue[50];
    char	cvtValue[40];
    int		ilink,nlinks;
    struct link	*plink;
    int         linkType;
    FILE        *stream = (report==0) ? stdout : report;

    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
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
		linkType = plink->type;
		if(bus[linkType][0]==0) continue;
		strcpy(linkValue,dbGetString(pdbentry));
		status = dbFindField(pdbentry,"DTYP");
		if(status) break;
		strcpy(dtypValue,dbGetString(pdbentry));
		status = dbFindField(pdbentry,"LINR");
		if(status) {
		    cvtValue[0] = 0;
		} else {
		    if(strcmp(dbGetString(pdbentry),"LINEAR")!=0) {
			cvtValue[0] = 0;
		    } else {
			strcpy(cvtValue,"cvt(");
			status = dbFindField(pdbentry,"EGUL");
			if(!status) strcat(cvtValue,dbGetString(pdbentry));
			status = dbFindField(pdbentry,"EGUF");
			if(!status) {
			    strcat(cvtValue,",");
			    strcat(cvtValue,dbGetString(pdbentry));
			}
			strcat(cvtValue,")");
		    }
		}
		fprintf(stream,"%-8s %-20s %-20s %-20s %-s\n",
			bus[linkType],linkValue,dtypValue,
			dbGetRecordName(pdbentry),cvtValue);
		break;
	    }
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    finishOutstream(stream);
    return;
}
