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
#include "epicsAssert.h"
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

#include "dbCommon.h"
#include "dbJLink.h"

int dbStaticDebug = 0;
static char *pNullString = "";
#define messagesize	276
#define RPCL_LEN INFIX_TO_POSTFIX_SIZE(80)

/* must be long enough to hold 32-bit signed integer in base 10 */
STATIC_ASSERT(messagesize>=11);

static char *ppstring[5]={" NPP"," PP"," CA"," CP"," CPP"};
static char *msstring[4]={" NMS"," MS"," MSI"," MSS"};

epicsShareDef maplinkType pamaplinkType[LINK_NTYPES] = {
	{"CONSTANT",CONSTANT},
	{"PV_LINK",PV_LINK},
	{"VME_IO",VME_IO},
	{"CAMAC_IO",CAMAC_IO},
	{"AB_IO",AB_IO},
	{"GPIB_IO",GPIB_IO},
	{"BITBUS_IO",BITBUS_IO},
	{"MACRO_LINK",MACRO_LINK},
	{"JSON_LINK",JSON_LINK},
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
static void dbMsgPrint(DBENTRY *pdbentry, const char *fmt, ...)
    EPICS_PRINTF_STYLE(2,3);
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

void dbFreeLinkContents(struct link *plink)
{
    char *parm = NULL;

    switch(plink->type) {
	case CONSTANT: free((void *)plink->value.constantStr); break;
	case MACRO_LINK: free((void *)plink->value.macro_link.macroStr); break;
	case PV_LINK: free((void *)plink->value.pv_link.pvname); break;
	case JSON_LINK:
	    dbJLinkFree(plink->value.json.jlink);
	    parm = plink->value.json.string;
	    break;
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
    plink->lset = NULL;
    plink->text = NULL;
    memset(&plink->value, 0, sizeof(union value));
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
    *msg = '\0';
    return msg;
}

static
void dbMsgCpy(DBENTRY *pdbentry, const char *msg)
{
    getpMessage(pdbentry);
    strncpy(pdbentry->message, msg, messagesize-1);
    pdbentry->message[messagesize-1] = '\0';
}

static
void dbMsgPrint(DBENTRY *pdbentry, const char *fmt, ...)
{
    va_list args;
    getpMessage(pdbentry);
    va_start(args, fmt);
    epicsVsnprintf(pdbentry->message, messagesize, fmt, args);
    va_end(args);
}

static void ulongToHexString(epicsUInt32 source, char *pdest)
{
    static const char hex_digit_to_ascii[16] = "0123456789abcdef";
    epicsUInt32 val,temp;
    char digit[10];
    int i,j;

    if (source==0) {
        strcpy(pdest,"0x0");
        return;
    }
    *pdest++ = '0'; *pdest++ = 'x';
    val = source;
    for (i=0; val!=0; i++) {
        temp = val/16;
        digit[i] = hex_digit_to_ascii[val - temp*16];
        val = temp;
    }
    for (j=i-1; j>=0; j--) {
        *pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static void realToString(double value, char *preturn, int isdouble)
{
    static const double delta[2] = {1e-6, 1e-15};
    static const int precision[2] = {6, 14};
    double	absvalue;
    int		logval,prec;
    size_t  end;
    char	tstr[30];
    char	*ptstr = &tstr[0];
    int		round;
    int		ise = FALSE;
    char	*loce = NULL;

    if (value == 0) {
        strcpy(preturn, "0");
        return;
    }

    absvalue = value < 0 ? -value : value;
    if (absvalue < (double)INT_MAX) {
        epicsInt32 intval = (epicsInt32) value;
        double diff = value - intval;

        if (diff < 0) diff = -diff;
        if (diff < absvalue * delta[isdouble]) {
            cvtLongToString(intval, preturn);
            return;
        }
    }

    /*Now starts the hard cases*/
    if (value < 0) {
        *preturn++ = '-';
        value = -value;
    }

    logval = (int)log10(value);
    if (logval > 6 || logval < -2) {
        int nout;

        ise = TRUE;
        prec = precision[isdouble];
        nout = sprintf(ptstr, "%.*e", prec, value);
        loce = strchr(ptstr, 'e');

        if (!loce) {
            ptstr[nout] = 0;
            strcpy(preturn, ptstr);
            return;
        }

        *loce++ = 0;
    } else {
        prec = precision[isdouble] - logval;
        if ( prec < 0) prec = 0;
        sprintf(ptstr, "%.*f", prec, value);
    }

    if (prec > 0) {
        end = strlen(ptstr) - 1;
        round = FALSE;
        while (end > 0) {
            if (tstr[end] == '.') {end--; break;}
            if (tstr[end] == '0') {end--; continue;}
            if (!round && end < precision[isdouble]) break;
            if (!round && tstr[end] < '8') break;
            if (tstr[end-1] == '.') {
                if (round) end = end-2;
                break;
            }
            if (tstr[end-1] != '9') break;
            round = TRUE;
            end--;
        }
        tstr[end+1] = 0;
        while (round) {
            if (tstr[end] < '9') {tstr[end]++; break;}
            if (end == 0) { *preturn++ = '1'; tstr[end] = '0'; break;}
            tstr[end--] = '0';
        }
    }
    strcpy(preturn, &tstr[0]);
    if (ise) {
        if (!(strchr(preturn, '.'))) strcat(preturn, ".0");
        strcat(preturn, "e");
        strcat(preturn, loce);
    }
}

static void floatToString(float value, char *preturn)
{
    realToString((double)value, preturn, 0);
}

static void doubleToString(double value, char *preturn)
{
    realToString(value, preturn, 1);
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
    linkSup		*plinkSup;
    brkTable		*pbrkTable;
    brkTable		*pbrkTableNext;
    chFilterPlugin  *pfilt;
    chFilterPlugin  *pfiltNext;
    dbGuiGroup      *pguiGroup;
    dbGuiGroup      *pguiGroupNext;
    int			i;
    DBENTRY		dbentry;
    long status;

    dbInitEntry(pdbbase,&dbentry);
    status = dbFirstRecordType(&dbentry);
    while(!status) {
        /* dbDeleteRecord() will remove alias or real record node.
         * For real record nodes, also removes the nodes of all aliases.
         * This complicates safe traversal, so we re-start iteration
         * from the first record after each call.
         */
        while((status = dbFirstRecord(&dbentry))==0) {
            dbDeleteRecord(&dbentry);
        }
        assert(status==S_dbLib_recNotFound);
        status = dbNextRecordType(&dbentry);
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
    while ((plinkSup = (linkSup *) ellGet(&pdbbase->linkList))) {
        free(plinkSup->jlif_name);
        free(plinkSup->name);
        free(plinkSup);
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

long dbWriteLinkFP(DBBASE *pdbbase, FILE *fp)
{
    linkSup *plinkSup;

    if (!pdbbase) {
	fprintf(stderr, "pdbbase not specified\n");
	return -1;
    }
    for (plinkSup = (linkSup *) ellFirst(&pdbbase->linkList);
        plinkSup; plinkSup = (linkSup *) ellNext(&plinkSup->node)) {
	fprintf(fp, "link(%s,%s)\n", plinkSup->name, plinkSup->jlif_name);
    }
    return 0;
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
    /* alias of alias still references actual record */
    while(precnode && (precnode->flags&DBRN_FLAGS_ISALIAS))
        precnode = precnode->aliasedRecnode;
    if (!precnode) return S_dbLib_recNotFound;
    zeroDbentry(pdbentry);
    if (!dbFindRecord(pdbentry, alias)) return S_dbLib_recExists;
    zeroDbentry(pdbentry);
    pdbentry->precordType = precordType;
    preclist = &precordType->recList;
    pnewnode = dbCalloc(1, sizeof(dbRecordNode));
    pnewnode->recordname = epicsStrDup(alias);
    pnewnode->precord = precnode->precord;
    pnewnode->aliasedRecnode = precnode;
    pnewnode->flags = DBRN_FLAGS_ISALIAS;
    precnode->flags |= DBRN_FLAGS_HASALIAS;
    ellInit(&pnewnode->infoList);
    ellAdd(preclist, &pnewnode->node);
    precordType->no_aliases++;
    pdbentry->precnode = pnewnode;
    ppvd = dbPvdAdd(pdbentry->pdbbase, precordType, pnewnode);
    if (!ppvd) {errMessage(-1,"Logic Err: Could not add to PVD");return(-1);}
    return 0;
}

int dbFollowAlias(DBENTRY *pdbentry)
{
    if(!pdbentry->precnode)
        return S_dbLib_recNotFound;
    if(pdbentry->precnode->aliasedRecnode)
        pdbentry->precnode = pdbentry->precnode->aliasedRecnode;
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
    DBLINK 	*plink;

    if (!pflddes) {
        dbMsgCpy(pdbentry, "fldDes not found");
        return pdbentry->message;
    }
    switch (pflddes->field_type) {
    case DBF_STRING:
    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK:
        if (!pfield) {
            dbMsgCpy(pdbentry, "Field not allocated (NULL)");
            return pdbentry->message;
        }
        break;
    default:
        break;
    }

    switch (pflddes->field_type) {
    case DBF_STRING:
        dbMsgCpy(pdbentry, (char *)pfield);
        break;
    case DBF_CHAR:
    case DBF_UCHAR:
    case DBF_SHORT:
    case DBF_USHORT:
    case DBF_ENUM:
    case DBF_LONG:
    case DBF_ULONG:
    case DBF_INT64:
    case DBF_UINT64:
    case DBF_FLOAT:
    case DBF_DOUBLE:
    case DBF_MENU:
    case DBF_DEVICE:
	return(dbGetStringNum(pdbentry));
    case DBF_INLINK:
    case DBF_OUTLINK:
	plink = (DBLINK *)pfield;
	switch(plink->type) {
	case CONSTANT:
	    if (plink->value.constantStr) {
		dbMsgCpy(pdbentry, plink->value.constantStr);
	    } else {
		dbMsgCpy(pdbentry, "");
	    }
	    break;
	case MACRO_LINK:
	    if (plink->value.macro_link.macroStr) {
		dbMsgCpy(pdbentry, plink->value.macro_link.macroStr);
	    } else {
		dbMsgCpy(pdbentry, "");
	    }
	    break;
	case JSON_LINK:
	    dbMsgCpy(pdbentry, plink->value.json.string);
	    break;
        case PN_LINK:
            dbMsgPrint(pdbentry, "%s%s",
                   plink->value.pv_link.pvname ? plink->value.pv_link.pvname : "",
                   msstring[plink->value.pv_link.pvlMask&pvlOptMsMode]);
	    break;
	case PV_LINK:
	case CA_LINK:
	case DB_LINK: {
	    int 	ppind;
	    short	pvlMask;

	    pvlMask = plink->value.pv_link.pvlMask;
	    if (pvlMask&pvlOptPP) ppind=1;
	    else if(pvlMask&pvlOptCA) ppind=2;
	    else if(pvlMask&pvlOptCP) ppind=3;
	    else if(pvlMask&pvlOptCPP) ppind=4;
	    else ppind=0;
            dbMsgPrint(pdbentry, "%s%s%s%s",
                   plink->value.pv_link.pvname ? plink->value.pv_link.pvname : "",
                   (pvlMask & pvlOptTSELisTime) ? ".TIME" : "",
                   ppstring[ppind],
                   msstring[plink->value.pv_link.pvlMask&pvlOptMsMode]);
	    break;
	}
	case VME_IO:
	    dbMsgPrint(pdbentry, "#C%d S%d @%s",
		plink->value.vmeio.card,plink->value.vmeio.signal,
		plink->value.vmeio.parm);
	    break;
	case CAMAC_IO:
	    dbMsgPrint(pdbentry, "#B%d C%d N%d A%d F%d @%s",
		plink->value.camacio.b,plink->value.camacio.c,
		plink->value.camacio.n,plink->value.camacio.a,
		plink->value.camacio.f,plink->value.camacio.parm);
	    break;
	case RF_IO:
	    dbMsgPrint(pdbentry, "#R%d M%d D%d E%d",
		plink->value.rfio.cryo,
		plink->value.rfio.micro,
		plink->value.rfio.dataset,
		plink->value.rfio.element);
	    break;
	case AB_IO:
	    dbMsgPrint(pdbentry, "#L%d A%d C%d S%d @%s",
		plink->value.abio.link,plink->value.abio.adapter,
		plink->value.abio.card,plink->value.abio.signal,
		plink->value.abio.parm);
	    break;
	case GPIB_IO:
	    dbMsgPrint(pdbentry, "#L%d A%d @%s",
		plink->value.gpibio.link,plink->value.gpibio.addr,
		plink->value.gpibio.parm);
	    break;
	case BITBUS_IO:
	    dbMsgPrint(pdbentry, "#L%u N%u P%u S%u @%s",
		plink->value.bitbusio.link,plink->value.bitbusio.node,
		plink->value.bitbusio.port,plink->value.bitbusio.signal,
		plink->value.bitbusio.parm);
	    break;
	case BBGPIB_IO:
	    dbMsgPrint(pdbentry, "#L%u B%u G%u @%s",
		plink->value.bbgpibio.link,plink->value.bbgpibio.bbaddr,
		plink->value.bbgpibio.gpibaddr,plink->value.bbgpibio.parm);
	    break;
	case INST_IO:
	    dbMsgPrint(pdbentry, "@%s", plink->value.instio.string);
	    break;
	case VXI_IO :
	    if (plink->value.vxiio.flag == VXIDYNAMIC)
		dbMsgPrint(pdbentry, "#V%d C%d S%d @%s",
		    plink->value.vxiio.frame,plink->value.vxiio.slot,
		    plink->value.vxiio.signal,plink->value.vxiio.parm);
	    else
		dbMsgPrint(pdbentry, "#V%d S%d @%s",
		    plink->value.vxiio.la,plink->value.vxiio.signal,
		    plink->value.vxiio.parm);
	    break;
	default :
	    return(NULL);
	}
	break;
    case DBF_FWDLINK: {
	    DBLINK *plink=(DBLINK *)pfield;

	    switch(plink->type) {
	    case CONSTANT:
		dbMsgCpy(pdbentry, "0");
		break;
	    case MACRO_LINK:
		if (plink->value.macro_link.macroStr) {
		    dbMsgCpy(pdbentry, plink->value.macro_link.macroStr);
		} else {
		    dbMsgCpy(pdbentry, "");
		}
		break;
	    case JSON_LINK:
		dbMsgCpy(pdbentry, plink->value.json.string);
		break;
	    case PV_LINK:
	    case CA_LINK:
	    case DB_LINK: {
		int	ppind;
		short	pvlMask;

		pvlMask = plink->value.pv_link.pvlMask;
		if (pvlMask&pvlOptCA) ppind=2;
		else ppind=0;
                dbMsgPrint(pdbentry, "%s%s",
                    plink->value.pv_link.pvname ? plink->value.pv_link.pvname : "",
                    ppind ? ppstring[ppind] : "");
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
    return pdbentry->message;
}

char *dbGetStringNum(DBENTRY *pdbentry)
{
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    char	*message;
    unsigned char cvttype;

    /* the following assumes that messagesize is large enough
     * to hold the base 10 encoded value of a 32-bit integer.
     */
    message = getpMessage(pdbentry);
    cvttype = pflddes->base;
    switch (pflddes->field_type) {
    case DBF_CHAR:
        if (cvttype == CT_DECIMAL)
            cvtCharToString(*(char *) pfield, message);
        else
            ulongToHexString(*(char *) pfield, message);
        break;
    case DBF_UCHAR:
        if (cvttype==CT_DECIMAL)
            cvtUcharToString(*(epicsUInt8 *) pfield, message);
        else
            ulongToHexString(*(epicsUInt8 *) pfield, message);
        break;
    case DBF_SHORT:
        if (cvttype==CT_DECIMAL)
            cvtShortToString(*(epicsInt16 *) pfield, message);
        else
            ulongToHexString(*(epicsInt16 *) pfield, message);
        break;
    case DBF_USHORT:
    case DBF_ENUM:
        if (cvttype==CT_DECIMAL)
            cvtUshortToString(*(epicsUInt16 *) pfield, message);
        else
            ulongToHexString(*(epicsUInt16 *) pfield, message);
        break;
    case DBF_LONG:
        if (cvttype==CT_DECIMAL)
            cvtLongToString(*(epicsInt32 *) pfield, message);
        else
            ulongToHexString(*(epicsInt32 *) pfield, message);
        break;
    case DBF_ULONG:
        if (cvttype==CT_DECIMAL)
            cvtUlongToString(*(epicsUInt32 *) pfield, message);
        else
            ulongToHexString(*(epicsUInt32 *) pfield, message);
        break;
    case DBF_INT64:
        if (cvttype==CT_DECIMAL)
            cvtInt64ToString(*(epicsInt64 *) pfield, message);
        else
            cvtInt64ToHexString(*(epicsInt64 *) pfield, message);
        break;
    case DBF_UINT64:
        if (cvttype==CT_DECIMAL)
            cvtUInt64ToString(*(epicsUInt32 *) pfield, message);
        else
            cvtUInt64ToHexString(*(epicsUInt32 *) pfield, message);
        break;
    case DBF_FLOAT:
        floatToString(*(epicsFloat32 *) pfield, message);
        break;
    case DBF_DOUBLE:
        doubleToString(*(epicsFloat64 *) pfield, message);
        break;
    case DBF_MENU:
        {
            dbMenu *pdbMenu = (dbMenu *)pflddes->ftPvt;
            epicsEnum16 choice_ind;
            char *pchoice;

            if (!pfield) {
                dbMsgCpy(pdbentry, "Field not found");
                return message;
            }
            choice_ind = *((epicsEnum16 *) pdbentry->pfield);
            if (!pdbMenu || choice_ind < 0 || choice_ind >= pdbMenu->nChoice)
                return NULL;
            pchoice = pdbMenu->papChoiceValue[choice_ind];
            dbMsgCpy(pdbentry, pchoice);
        }
        break;
    case DBF_DEVICE:
        {
            dbDeviceMenu *pdbDeviceMenu;
            epicsEnum16 choice_ind;
            char *pchoice;

            if (!pfield) {
                dbMsgCpy(pdbentry, "Field not found");
                return message;
            }
            pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
            if (!pdbDeviceMenu)
                return NULL;
            choice_ind = *((epicsEnum16 *) pdbentry->pfield);
            if (choice_ind<0 || choice_ind>=pdbDeviceMenu->nChoice)
                return NULL;
            pchoice = pdbDeviceMenu->papChoice[choice_ind];
            dbMsgCpy(pdbentry, pchoice);
        }
        break;
    default:
        return NULL;
    }
    return message;
}

long dbInitRecordLinks(dbRecordType *rtyp, struct dbCommon *prec)
{
    short i;

    for (i=0; i<rtyp->no_links; i++) {
        dbLinkInfo link_info;
        dbFldDes *pflddes = rtyp->papFldDes[rtyp->link_ind[i]];
        DBLINK *plink = (DBLINK *)(((char *)prec) + pflddes->offset);
        devSup *devsup = NULL;

        plink->precord = prec;

        /* link fields are zero'd on allocation.
         * so are effectively CONSTANT, but with constantStr==NULL.
         * Here we initialize them to have the correct link type,
         * with zero values and empty (but non-NULL) strings.
         */

        if(pflddes->isDevLink) {
            devsup = (devSup *)ellNth(&rtyp->devList, prec->dtyp+1);
        }
        if(devsup)
            plink->type = devsup->link_type;
        else
            plink->type = CONSTANT;

        switch (plink->type) {
        /* constantStr is allowed to remain NULL if plink->text==NULL
         * constantStr==NULL has special meaning in recGblInitConstantLink()
         */
        case CONSTANT: plink->value.constantStr = NULL; break;
        case PV_LINK:  plink->value.pv_link.pvname = callocMustSucceed(1, 1, "init PV_LINK"); break;
        case JSON_LINK: plink->value.json.string = pNullString; break;
        case VME_IO: plink->value.vmeio.parm = pNullString; break;
        case CAMAC_IO: plink->value.camacio.parm = pNullString; break;
        case AB_IO: plink->value.abio.parm = pNullString; break;
        case GPIB_IO: plink->value.gpibio.parm = pNullString; break;
        case BITBUS_IO: plink->value.bitbusio.parm = pNullString; break;
        case INST_IO: plink->value.instio.string = pNullString; break;
        case BBGPIB_IO: plink->value.bbgpibio.parm = pNullString; break;
        case VXI_IO: plink->value.vxiio.parm = pNullString; break;
        }

        if(!plink->text)
            continue;

        if(dbParseLink(plink->text, pflddes->field_type, &link_info, 0)!=0) {
            /* This was already parsed once when ->text was set.
             * Any syntax error messages were printed at that time.
             */

        } else if(dbCanSetLink(plink, &link_info, devsup)!=0) {
            errlogPrintf("Error: %s.%s: can't initialize link type %d with \"%s\" (type %d)\n",
                         prec->name, pflddes->name, plink->type, plink->text, link_info.ltype);

        } else if(dbSetLink(plink, &link_info, devsup)) {
            errlogPrintf("Error: %s.%s: failed to initialize link type %d with \"%s\" (type %d)\n",
                         prec->name, pflddes->name, plink->type, plink->text, link_info.ltype);
        }
        free(plink->text);
        plink->text = NULL;
    }
    return 0;
}

void dbFreeLinkInfo(dbLinkInfo *pinfo)
{
    if (pinfo->ltype == JSON_LINK) {
        dbJLinkFree(pinfo->jlink);
        pinfo->jlink = NULL;
    }
    free(pinfo->target);
    pinfo->target = NULL;
}

long dbParseLink(const char *str, short ftype, dbLinkInfo *pinfo, unsigned opts)
{
    char *pstr;
    size_t len;
    double value;

    memset(pinfo, 0, sizeof(*pinfo));

    /* Strip leading white space */
    while (*str && isspace((int)*str)) str++;

    len = strlen(str);
    /* Strip trailing white space */
    while (len > 0 && isspace((int)str[len-1])) len--;

    pstr = malloc(len + 1);
    if (!pstr)
        return S_dbLib_outMem;
    pinfo->target = pstr;

    /* Check for Instrument I/O links */
    if (*str == '@') {
        pinfo->ltype = INST_IO;

        /* Store everything after the '@' */
        memcpy(pstr, str+1, --len);
        pstr[len] = '\0';
        return 0;
    }

    /* Store the stripped string */
    memcpy(pstr, str, len);
    pstr[len] = '\0';

    /* Check for braces => JSON */
    if (*str == '{' && str[len-1] == '}') {
        if (dbJLinkParse(str, len, ftype, &pinfo->jlink, opts))
            goto fail;

        pinfo->ltype = JSON_LINK;
        return 0;
    }

    /* Check for other HW link types */
    if (*pstr == '#') {
        int ret;
        char junk = 0;
        char *parm = strchr(pstr, '@'); /* find start of parm string */

        if (parm) {
            *parm++ = '\0'; /* isolate the parm string for later */
            len -= (parm - pstr);
        }

        /* generalized extraction of ID charactor and integer pairs (eg. "#C15 S14") */
        ret = sscanf(pinfo->target, "# %c%d %c%d %c%d %c%d %c%d %c",
                     &pinfo->hwid[0], &pinfo->hwnums[0],
                     &pinfo->hwid[1], &pinfo->hwnums[1],
                     &pinfo->hwid[2], &pinfo->hwnums[2],
                     &pinfo->hwid[3], &pinfo->hwnums[3],
                     &pinfo->hwid[4], &pinfo->hwnums[4],
                     &junk);

        /* ret<0 when pattern not matched
         * ret==11 when extra non-space before '@'.
         * ret is odd when a number is missing
         */
        if (ret<0 || ret>10 || ret%2==1) goto fail;

        if      (strcmp(pinfo->hwid, "CS")==0)    pinfo->ltype = VME_IO;
        else if (strcmp(pinfo->hwid, "BCN")==0)   pinfo->ltype = CAMAC_IO;
        else if (strcmp(pinfo->hwid, "BCNA")==0)  pinfo->ltype = CAMAC_IO;
        else if (strcmp(pinfo->hwid, "BCNF")==0)  pinfo->ltype = CAMAC_IO;
        else if (strcmp(pinfo->hwid, "BCNAF")==0) pinfo->ltype = CAMAC_IO;
        else if (strcmp(pinfo->hwid, "RMDE")==0)  pinfo->ltype = RF_IO;
        else if (strcmp(pinfo->hwid, "LACS")==0)  pinfo->ltype = AB_IO;
        else if (strcmp(pinfo->hwid, "LA")==0)    pinfo->ltype = GPIB_IO;
        else if (strcmp(pinfo->hwid, "LNPS")==0)  pinfo->ltype = BITBUS_IO;
        else if (strcmp(pinfo->hwid, "LBG")==0)   pinfo->ltype = BBGPIB_IO;
        else if (strcmp(pinfo->hwid, "VCS")==0)   pinfo->ltype = VXI_IO;
        else if (strcmp(pinfo->hwid, "VS")==0)    pinfo->ltype = VXI_IO;
        else goto fail;

        if (parm && pinfo->ltype != RF_IO) {
            /* move parm string to beginning of buffer */
            memmove(pinfo->target, parm, len + 1);
        } else if (!parm && pinfo->ltype == RF_IO) {
            /* RF_IO, the string isn't needed at all */
            free(pinfo->target);
            pinfo->target = NULL;
        }
        else goto fail;

        return 0;
    }

    /* Link is a constant if empty or it holds just a number */
    if (len == 0 || epicsParseDouble(pstr, &value, NULL) == 0) {
        pinfo->ltype = CONSTANT;
        return 0;
    }

    /* Link may be an array constant */
    if (pstr[0] == '[' && pstr[len-1] == ']' &&
        (strchr(pstr, ',') || strchr(pstr, '"'))) {
        pinfo->ltype = CONSTANT;
        return 0;
    }

    pinfo->ltype = PV_LINK;
    pstr = strchr(pstr, ' '); /* find start of link modifiers (can't be seperated by tabs) */
    if (pstr) {
        *pstr++ = '\0'; /* isolate modifiers. pinfo->target is PV name only for re-use in struct pv_link */

        /* Space seperation of modifiers isn't required, and other chars are ignored.
         * Order of comparisons resolves ambiguity by checking for
         * longer matches first.
         * eg. "QQCPPXMSITT" is pvlOptCPP|pvlOptMSI
         */

        if (strstr(pstr, "NPP")) pinfo->modifiers = 0;
        else if (strstr(pstr, "CPP")) pinfo->modifiers = pvlOptCPP;
        else if (strstr(pstr, "PP")) pinfo->modifiers = pvlOptPP;
        else if (strstr(pstr, "CA")) pinfo->modifiers = pvlOptCA;
        else if (strstr(pstr, "CP")) pinfo->modifiers = pvlOptCP;

        if (strstr(pstr, "NMS")) pinfo->modifiers |= pvlOptNMS;
        else if (strstr(pstr, "MSI")) pinfo->modifiers |= pvlOptMSI;
        else if (strstr(pstr, "MSS")) pinfo->modifiers |= pvlOptMSS;
        else if (strstr(pstr, "MS")) pinfo->modifiers |= pvlOptMS;

        /* filter modifiers based on link type */
        switch(ftype) {
        case DBF_INLINK: /* accept all */ break;
        case DBF_OUTLINK: pinfo->modifiers &= ~pvlOptCPP; break;
        case DBF_FWDLINK: pinfo->modifiers &= pvlOptCA; break;
        }
    }

    return 0;
fail:
    dbFreeLinkInfo(pinfo);
    return S_dbLib_badField;
}

long dbCanSetLink(DBLINK *plink, dbLinkInfo *pinfo, devSup *devsup)
{
    /* Release pinfo resources on failure */
    int expected_type = devsup ? devsup->link_type : CONSTANT;

    if (pinfo->ltype == expected_type)
        return 0;

    switch (pinfo->ltype) {
    case CONSTANT:
    case JSON_LINK:
    case PV_LINK:
        if (expected_type == CONSTANT ||
            expected_type == JSON_LINK ||
            expected_type == PV_LINK)
            return 0;
    default:
        dbFreeLinkInfo(pinfo);
        return 1;
    }
}

static
void dbSetLinkConst(DBLINK *plink, dbLinkInfo *pinfo)
{
    plink->type = CONSTANT;
    plink->value.constantStr = pinfo->target;

    pinfo->target = NULL;
}

static
void dbSetLinkPV(DBLINK *plink, dbLinkInfo *pinfo)
{
    plink->type = PV_LINK;
    plink->value.pv_link.pvname = pinfo->target;
    plink->value.pv_link.pvlMask = pinfo->modifiers;

    pinfo->target = NULL;
}

static
void dbSetLinkJSON(DBLINK *plink, dbLinkInfo *pinfo)
{
    plink->type = JSON_LINK;
    plink->value.json.string = pinfo->target;
    plink->value.json.jlink = pinfo->jlink;

    pinfo->target = NULL;
    pinfo->jlink = NULL;
}

static
void dbSetLinkHW(DBLINK *plink, dbLinkInfo *pinfo)
{
    switch(pinfo->ltype) {
    case JSON_LINK:
        plink->value.json.string = pinfo->target;
        break;
    case INST_IO:
        plink->value.instio.string = pinfo->target;
        break;
    case VME_IO:
        plink->value.vmeio.card = pinfo->hwnums[0];
        plink->value.vmeio.signal = pinfo->hwnums[1];
        plink->value.vmeio.parm = pinfo->target;
        break;
    case CAMAC_IO:
        plink->value.camacio.b = pinfo->hwnums[0];
        plink->value.camacio.c = pinfo->hwnums[1];
        plink->value.camacio.n = pinfo->hwnums[2];
        plink->value.camacio.a = pinfo->hwnums[3];
        plink->value.camacio.f = pinfo->hwnums[4];
        plink->value.camacio.parm = pinfo->target;
        break;
    case RF_IO:
        plink->value.rfio.cryo = pinfo->hwnums[0];
        plink->value.rfio.micro = pinfo->hwnums[1];
        plink->value.rfio.dataset = pinfo->hwnums[2];
        plink->value.rfio.element = pinfo->hwnums[3];
        break;
    case AB_IO:
        plink->value.abio.link = pinfo->hwnums[0];
        plink->value.abio.adapter = pinfo->hwnums[1];
        plink->value.abio.card = pinfo->hwnums[2];
        plink->value.abio.signal = pinfo->hwnums[3];
        plink->value.abio.parm = pinfo->target;
        break;
    case GPIB_IO:
        plink->value.gpibio.link = pinfo->hwnums[0];
        plink->value.gpibio.addr = pinfo->hwnums[1];
        plink->value.gpibio.parm = pinfo->target;
        break;
    case BITBUS_IO:
        plink->value.bitbusio.link = pinfo->hwnums[0];
        plink->value.bitbusio.node = pinfo->hwnums[1];
        plink->value.bitbusio.port = pinfo->hwnums[2];
        plink->value.bitbusio.signal = pinfo->hwnums[3];
        plink->value.bitbusio.parm = pinfo->target;
        break;
    case BBGPIB_IO:
        plink->value.bbgpibio.link = pinfo->hwnums[0];
        plink->value.bbgpibio.bbaddr = pinfo->hwnums[1];
        plink->value.bbgpibio.gpibaddr = pinfo->hwnums[2];
        plink->value.bbgpibio.parm = pinfo->target;
        break;
    case VXI_IO:
        if(strcmp(pinfo->hwid, "VCS")==0) {
            plink->value.vxiio.flag=VXIDYNAMIC;
            plink->value.vxiio.frame = pinfo->hwnums[0];
            plink->value.vxiio.slot = pinfo->hwnums[1];
            plink->value.vxiio.signal = pinfo->hwnums[2];
        } else if(strcmp(pinfo->hwid, "VS")==0) {
            plink->value.vxiio.flag=VXISTATIC;
            plink->value.vxiio.la = pinfo->hwnums[0];
            plink->value.vxiio.signal = pinfo->hwnums[1];
        } else {
            cantProceed("dbSetLinkHW: logic error, unknown VXI_IO variant");
        }
        plink->value.vxiio.parm = pinfo->target;
        break;

    default:
        cantProceed("dbSetLinkHW: logic error, unhandled link type");
        return;
    }

    plink->type = pinfo->ltype;

    pinfo->target = NULL; /* now owned by link field */
}

long dbSetLink(DBLINK *plink, dbLinkInfo *pinfo, devSup *devsup)
{
    int expected_type = devsup ? devsup->link_type : CONSTANT;

    if (expected_type == CONSTANT ||
        expected_type == JSON_LINK ||
        expected_type == PV_LINK) {
        switch (pinfo->ltype) {
        case CONSTANT:
            dbFreeLinkContents(plink);
            dbSetLinkConst(plink, pinfo);
            break;
        case PV_LINK:
            dbFreeLinkContents(plink);
            dbSetLinkPV(plink, pinfo);
            break;
        case JSON_LINK:
            dbFreeLinkContents(plink);
            dbSetLinkJSON(plink, pinfo);
            break;
        default:
            errlogMessage("Warning: dbSetLink: forgot to test with dbCanSetLink() or logic error");
            goto fail; /* can't assign HW link */
        }
    }
    else if (expected_type == pinfo->ltype) {
        dbFreeLinkContents(plink);
        dbSetLinkHW(plink, pinfo);
    }
    else
        goto fail;

    return 0;
fail:
    dbFreeLinkInfo(pinfo);
    return S_dbLib_badField;
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
    case DBF_INT64:
    case DBF_UCHAR:
    case DBF_USHORT:
    case DBF_ULONG:
    case DBF_UINT64:
    case DBF_ENUM:
    case DBF_FLOAT:
    case DBF_DOUBLE:
    case DBF_MENU:
    case DBF_DEVICE:
        status = dbPutStringNum(pdbentry,pstring);
        break;

    case DBF_INLINK:
    case DBF_OUTLINK:
    case DBF_FWDLINK: {
            dbLinkInfo link_info;
            DBLINK *plink = (DBLINK *)pfield;
            DBENTRY infoentry;
            unsigned opts = 0;

            if(pdbentry->precnode && ellCount(&pdbentry->precnode->infoList)) {
                dbCopyEntryContents(pdbentry, &infoentry);

                if(dbFindInfo(&infoentry, "base:lsetDebug")==0 && epicsStrCaseCmp(dbGetInfoString(&infoentry), "YES")==0)
                    opts |= LINK_DEBUG_LSET;
                if(dbFindInfo(&infoentry, "base:jlinkDebug")==0 && epicsStrCaseCmp(dbGetInfoString(&infoentry), "YES")==0)
                    opts |= LINK_DEBUG_JPARSE;

                dbFinishEntry(&infoentry);
            }

            status = dbParseLink(pstring, pflddes->field_type, &link_info, opts);
            if (status) break;

            if (plink->type==CONSTANT && plink->value.constantStr==NULL) {
                /* links not yet initialized by dbInitRecordLinks() */
                free(plink->text);
                plink->text = epicsStrDup(pstring);
                dbFreeLinkInfo(&link_info);
            } else {
                /* assignment after init (eg. autosave restore) */
                struct dbCommon *prec = pdbentry->precnode->precord;
                devSup *devsup = (devSup *)ellNth(&pdbentry->precordType->devList, prec->dtyp+1);

                status = dbCanSetLink(plink, &link_info, devsup);
                if (status == 0)
                    status = dbSetLink(plink, &link_info, devsup);
            }
        }
        break;

    default:
	return S_dbLib_badField;
    }

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

linkSup* dbFindLinkSup(dbBase *pdbbase, const char *name) {
    GPHENTRY *pgph = gphFind(pdbbase->pgpHash,name,&pdbbase->linkList);
    if (!pgph) return NULL;
    return (linkSup *) pgph->userPvt;
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
    printf("rset * %p rec_size %d\n",
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

void  dbDumpLink(DBBASE *pdbbase)
{
    if(!pdbbase) {
	fprintf(stderr,"pdbbase not specified\n");
	return;
    }
    dbWriteLinkFP(pdbbase,stdout);
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
		strncpy(linkValue, dbGetString(pdbentry), NELEMENTS(linkValue)-1);
		linkValue[NELEMENTS(linkValue)-1] = '\0';
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
