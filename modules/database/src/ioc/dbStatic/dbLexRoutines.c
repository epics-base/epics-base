/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author:  Marty Kraimer Date:    13JUL95*/

/*The routines in this module are serially reusable NOT reentrant*/

#include <ctype.h>
#include <epicsStdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "dbmf.h"
#include "ellLib.h"
#include "epicsPrint.h"
#include "epicsString.h"
#include "errMdef.h"
#include "freeList.h"
#include "gpHash.h"
#include "macLib.h"

#include "dbBase.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "epicsExport.h"
#include "link.h"
#include "special.h"
#include "iocInit.h"

/* This file is included from dbYacc.y
 * Duplicate some declarations to avoid warnings from analysis tools which don't know about this.
 */
static int yyerror(char *str);
static long pvt_yy_parse(void);

/*global declarations*/
char *makeDbdDepends=0;

int dbRecordsOnceOnly=0;
epicsExportAddress(int,dbRecordsOnceOnly);

int dbBptNotMonotonic=0;
epicsExportAddress(int,dbBptNotMonotonic);

int dbQuietMacroWarnings=0;
epicsExportAddress(int,dbQuietMacroWarnings);

int dbRecordsAbcSorted=0;
epicsExportAddress(int,dbRecordsAbcSorted);

/*private routines */
static void yyerrorAbort(char *str);
static void allocTemp(void *pvoid);
static void *popFirstTemp(void);
static void *getLastTemp(void);
static int db_yyinput(char *buf,int max_size);
static void dbIncludePrint(void);
static void dbPathCmd(char *path);
static void dbAddPathCmd(char *path);
static void dbIncludeNew(char *include_file);
static void dbMenuHead(char *name);
static void dbMenuChoice(char *name,char *value);
static void dbMenuBody(void);

static void dbRecordtypeHead(char *name);
static void dbRecordtypeEmpty(void);
static void dbRecordtypeBody(void);
static void dbRecordtypeFieldHead(char *name,char *type);
static void dbRecordtypeFieldItem(char *name,char *value);
static short findOrAddGuiGroup(const char *name);

static void dbDevice(char *recordtype,char *linktype,
        char *dsetname,char *choicestring);
static void dbDriver(char *name);
static void dbLinkType(char *name, char *jlif_name);
static void dbRegistrar(char *name);
static void dbFunction(char *name);
static void dbVariable(char *name, char *type);

static void dbBreakHead(char *name);
static void dbBreakItem(char *value);
static void dbBreakBody(void);

static void dbRecordHead(char *recordType,char*name,int visible);
static void dbRecordField(char *name,char *value);
static void dbRecordBody(void);

/*private declarations*/
#define MY_BUFFER_SIZE 1024
static char *my_buffer=NULL;
static char *mac_input_buffer=NULL;
static char *my_buffer_ptr=NULL;
static MAC_HANDLE *macHandle = NULL;
typedef struct inputFile{
    ELLNODE     node;
    const char  *path;
    const char  *filename;
    FILE        *fp;
    int         line_num;
}inputFile;
static ELLLIST inputFileList = ELLLIST_INIT;

static inputFile *pinputFileNow = NULL;
/* The DBBASE most recently allocated/used by dbReadCOM() */
static DBBASE *savedPdbbase = NULL;

typedef struct tempListNode {
    ELLNODE     node;
    void        *item;
}tempListNode;

static ELLLIST tempList = ELLLIST_INIT;
static void *freeListPvt = NULL;
static int duplicate = FALSE;

static void yyerrorAbort(char *str)
{
    yyerror(str);
    yyAbort = TRUE;
}

static void allocTemp(void *pvoid)
{
    tempListNode        *ptempListNode;

    ptempListNode = freeListCalloc(freeListPvt);
    ptempListNode->item = pvoid;
    ellAdd(&tempList,&ptempListNode->node);
}

static void *popFirstTemp(void)
{
    tempListNode        *ptempListNode;
    void                *ptemp = NULL;

    ptempListNode = (tempListNode *)ellFirst(&tempList);
    if(ptempListNode) {
        ptemp = ptempListNode->item;
        ellDelete(&tempList,(ELLNODE *)ptempListNode);
        freeListFree(freeListPvt,ptempListNode);
    }
    return(ptemp);
}

static void *getLastTemp(void)
{
    tempListNode        *ptempListNode;

    ptempListNode = (tempListNode *)ellLast(&tempList);
    return(ptempListNode->item);
}

const char *dbOpenFile(DBBASE *pdbbase,const char *filename,FILE **fp)
{
    ELLLIST     *ppathList = (ELLLIST *)pdbbase->pathPvt;
    dbPathNode  *pdbPathNode;
    char        *fullfilename;

    *fp = 0;
    if (!filename) return 0;
    if (!ppathList || ellCount(ppathList) == 0 ||
        strchr(filename, '/') || strchr(filename, '\\')) {
        *fp = fopen(filename, "r");
        if (*fp && makeDbdDepends)
            fprintf(stdout, "%s:%s \n", makeDbdDepends, filename);
        return 0;
    }
    pdbPathNode = (dbPathNode *)ellFirst(ppathList);
    while (pdbPathNode) {
        fullfilename = dbMalloc(strlen(pdbPathNode->directory) +
            strlen(filename) + 2);
        strcpy(fullfilename, pdbPathNode->directory);
        strcat(fullfilename, "/");
        strcat(fullfilename, filename);
        *fp = fopen(fullfilename, "r");
        if (*fp && makeDbdDepends)
            fprintf(stdout, "%s:%s \n", makeDbdDepends, fullfilename);
        free((void *)fullfilename);
        if (*fp) return pdbPathNode->directory;
        pdbPathNode = (dbPathNode *)ellNext(&pdbPathNode->node);
    }
    return 0;
}


static void freeInputFileList(void)
{
    inputFile *pinputFileNow;

    while((pinputFileNow=(inputFile *)ellFirst(&inputFileList))) {
        if(fclose(pinputFileNow->fp))
            errPrintf(0,__FILE__, __LINE__,
                        "Closing file %s",pinputFileNow->filename);
        free((void *)pinputFileNow->filename);
        ellDelete(&inputFileList,(ELLNODE *)pinputFileNow);
        free((void *)pinputFileNow);
    }
}

static
int cmp_dbRecordNode(const ELLNODE *lhs, const ELLNODE *rhs)
{
    dbRecordNode *LHS = (dbRecordNode*)lhs,
                 *RHS = (dbRecordNode*)rhs;

    return strcmp(LHS->recordname, RHS->recordname);
}

static long dbReadCOM(DBBASE **ppdbbase,const char *filename, FILE *fp,
        const char *path,const char *substitutions)
{
    long        status;
    inputFile   *pinputFile = NULL;
    char        *penv;
    char        **macPairs;

    if (ellCount(&tempList)) {
        epicsPrintf("dbReadCOM: Parser stack dirty %d\n", ellCount(&tempList));
    }

    if (getIocState() != iocVoid) {
        status = -2;
        goto cleanup;
    }

    if(*ppdbbase == 0) *ppdbbase = dbAllocBase();
    savedPdbbase = *ppdbbase;
    if(path && strlen(path)>0) {
        dbPath(savedPdbbase,path);
    } else {
        penv = getenv("EPICS_DB_INCLUDE_PATH");
        if(penv) {
            dbPath(savedPdbbase,penv);
        } else {
            dbPath(savedPdbbase,".");
        }
    }
    my_buffer = dbCalloc(MY_BUFFER_SIZE,sizeof(char));
    freeListInitPvt(&freeListPvt,sizeof(tempListNode),100);
    if(substitutions) {
        if(macCreateHandle(&macHandle,NULL)) {
            epicsPrintf("macCreateHandle error\n");
            status = -1;
            goto cleanup;
        }
        macParseDefns(macHandle,(char *)substitutions,&macPairs);
        if(macPairs ==NULL) {
            macDeleteHandle(macHandle);
            macHandle = NULL;
        } else {
            macInstallMacros(macHandle,macPairs);
            free((void *)macPairs);
            mac_input_buffer = dbCalloc(MY_BUFFER_SIZE,sizeof(char));
        }
        macSuppressWarning(macHandle,dbQuietMacroWarnings);
    }
    pinputFile = dbCalloc(1,sizeof(inputFile));
    if (filename) {
        pinputFile->filename = macEnvExpand(filename);
    }
    if (!fp) {
        FILE *fp1 = 0;

        if (pinputFile->filename)
            pinputFile->path = dbOpenFile(savedPdbbase, pinputFile->filename, &fp1);
        if (!pinputFile->filename || !fp1) {
            errPrintf(0, __FILE__, __LINE__,
                "dbRead opening file %s\n",pinputFile->filename);
            free((char*)pinputFile->filename);
            free(pinputFile);
            status = -1;
            goto cleanup;
        }
        pinputFile->fp = fp1;
    } else {
        pinputFile->fp = fp;
        fp = NULL;
    }
    pinputFile->line_num = 0;
    pinputFileNow = pinputFile;
    my_buffer[0] = '\0';
    my_buffer_ptr = my_buffer;
    ellAdd(&inputFileList,&pinputFile->node);
    status = pvt_yy_parse();

    if (ellCount(&tempList) && !yyAbort)
        epicsPrintf("dbReadCOM: Parser stack dirty w/o error. %d\n", ellCount(&tempList));
    while (ellCount(&tempList))
        popFirstTemp(); /* Memory leak on parser failure */

    dbFreePath(savedPdbbase);
    if(!status) { /*add RTYP and VERS as an attribute */
        DBENTRY dbEntry;
        DBENTRY *pdbEntry = &dbEntry;
        long    localStatus;

        dbInitEntry(savedPdbbase,pdbEntry);
        localStatus = dbFirstRecordType(pdbEntry);
        while(!localStatus) {
            localStatus = dbPutRecordAttribute(pdbEntry,"RTYP",
                dbGetRecordTypeName(pdbEntry));
            if(!localStatus)  {
                localStatus = dbPutRecordAttribute(pdbEntry,"VERS",
                    "none specified");
            }
            if(localStatus) {
                fprintf(stderr,"dbPutRecordAttribute status %ld\n",status);
            } else {
                localStatus = dbNextRecordType(pdbEntry);
            }
        }
        dbFinishEntry(pdbEntry);
    }
cleanup:
    if(dbRecordsAbcSorted) {
        ELLNODE *cur;
        for(cur = ellFirst(&savedPdbbase->recordTypeList); cur; cur=ellNext(cur))
        {
            dbRecordType *rtype = CONTAINER(cur, dbRecordType, node);

            ellSortStable(&rtype->recList, &cmp_dbRecordNode);
        }
    }
    if(macHandle) macDeleteHandle(macHandle);
    macHandle = NULL;
    if(mac_input_buffer) free((void *)mac_input_buffer);
    mac_input_buffer = NULL;
    if(freeListPvt) freeListCleanup(freeListPvt);
    freeListPvt = NULL;
    if(my_buffer) free((void *)my_buffer);
    my_buffer = NULL;
    freeInputFileList();
    if(fp)
        fclose(fp);
    return(status);
}

long dbReadDatabase(DBBASE **ppdbbase,const char *filename,
        const char *path,const char *substitutions)
{return (dbReadCOM(ppdbbase,filename,0,path,substitutions));}

long dbReadDatabaseFP(DBBASE **ppdbbase,FILE *fp,
        const char *path,const char *substitutions)
{return (dbReadCOM(ppdbbase,0,fp,path,substitutions));}

static int db_yyinput(char *buf, int max_size)
{
    size_t  l,n;
    char        *fgetsRtn;

    if(yyAbort) return(0);
    if(*my_buffer_ptr==0) {
        while(TRUE) { /*until we get some input*/
            if(macHandle) {
                fgetsRtn = fgets(mac_input_buffer,MY_BUFFER_SIZE,
                        pinputFileNow->fp);
                if(fgetsRtn) {
                    int exp = macExpandString(macHandle,mac_input_buffer,
                        my_buffer,MY_BUFFER_SIZE);
                    if (exp < 0) {
                        fprintf(stderr, "Warning: '%s' line %d has undefined macros\n",
                            pinputFileNow->filename, pinputFileNow->line_num+1);
                    }
                }
            } else {
                fgetsRtn = fgets(my_buffer,MY_BUFFER_SIZE,pinputFileNow->fp);
            }
            if(fgetsRtn) break;
            if(fclose(pinputFileNow->fp))
                errPrintf(0,__FILE__, __LINE__,
                        "Closing file %s",pinputFileNow->filename);
            free((void *)pinputFileNow->filename);
            ellDelete(&inputFileList,(ELLNODE *)pinputFileNow);
            free((void *)pinputFileNow);
            pinputFileNow = (inputFile *)ellLast(&inputFileList);
            if(!pinputFileNow) return(0);
        }
        if(dbStaticDebug) fprintf(stderr,"%s",my_buffer);
        pinputFileNow->line_num++;
        my_buffer_ptr = &my_buffer[0];
    }
    l = strlen(my_buffer_ptr);
    n = (l<=max_size ? l : max_size);
    memcpy(buf,my_buffer_ptr,n);
    my_buffer_ptr += n;
    return (int)n;
}

static void dbIncludePrint(void)
{
    inputFile *pinputFile = pinputFileNow;

    while (pinputFile) {
        epicsPrintf(" in");
        if (pinputFile->path)
            epicsPrintf(" path \"%s\" ",pinputFile->path);
        if (pinputFile->filename) {
            epicsPrintf(" file \"%s\"",pinputFile->filename);
        } else {
            epicsPrintf(" standard input");
        }
        epicsPrintf(" line %d\n",pinputFile->line_num);
        pinputFile = (inputFile *)ellPrevious(&pinputFile->node);
    }
    return;
}

static void dbPathCmd(char *path)
{
    dbPath(savedPdbbase,path);
}

static void dbAddPathCmd(char *path)
{
    dbAddPath(savedPdbbase,path);
}

static void dbIncludeNew(char *filename)
{
    inputFile   *pinputFile;
    FILE        *fp;

    pinputFile = dbCalloc(1,sizeof(inputFile));
    pinputFile->filename = macEnvExpand(filename);
    pinputFile->path = dbOpenFile(savedPdbbase, pinputFile->filename, &fp);
    if (!fp) {
        epicsPrintf("Can't open include file \"%s\"\n", filename);
        yyerror(NULL);
        free((void *)pinputFile->filename);
        free((void *)pinputFile);
        return;
    }
    pinputFile->fp = fp;
    ellAdd(&inputFileList,&pinputFile->node);
    pinputFileNow = pinputFile;
}

static void dbMenuHead(char *name)
{
    dbMenu              *pdbMenu;
    GPHENTRY            *pgphentry;

    if (!*name) {
        yyerrorAbort("dbMenuHead: Menu name can't be empty");
        return;
    }
    pgphentry = gphFind(savedPdbbase->pgpHash,name,&savedPdbbase->menuList);
    if(pgphentry) {
        duplicate = TRUE;
        return;
    }
    if(ellCount(&tempList)) yyerrorAbort("dbMenuHead: tempList not empty");
    pdbMenu = dbCalloc(1,sizeof(dbMenu));
    pdbMenu->name = epicsStrDup(name);
    allocTemp(pdbMenu);
}

static void dbMenuChoice(char *name,char *value)
{
    if (!*name) {
        yyerror("dbMenuChoice: Menu choice name can't be empty");
        return;
    }
    if(duplicate) return;
    allocTemp(epicsStrDup(name));
    allocTemp(epicsStrDup(value));
}

static void dbMenuBody(void)
{
    dbMenu              *pnewMenu;
    dbMenu              *pMenu;
    int                 nChoice;
    int                 i;
    GPHENTRY            *pgphentry;

    if(duplicate) {
        duplicate = FALSE;
        return;
    }
    pnewMenu = (dbMenu *)popFirstTemp();
    if(!pnewMenu)
        return;
    pnewMenu->nChoice = nChoice = ellCount(&tempList)/2;
    pnewMenu->papChoiceName = dbCalloc(pnewMenu->nChoice,sizeof(char *));
    pnewMenu->papChoiceValue = dbCalloc(pnewMenu->nChoice,sizeof(char *));
    for(i=0; i<nChoice; i++) {
        pnewMenu->papChoiceName[i] = (char *)popFirstTemp();
        pnewMenu->papChoiceValue[i] = (char *)popFirstTemp();
        if(!pnewMenu->papChoiceName[i] || !pnewMenu->papChoiceValue[i])
            return;
    }
    if(ellCount(&tempList)) yyerrorAbort("dbMenuBody: tempList not empty");
    /* Add menu in sorted order */
    pMenu = (dbMenu *)ellFirst(&savedPdbbase->menuList);
    while(pMenu && strcmp(pMenu->name,pnewMenu->name) >0 )
        pMenu = (dbMenu *)ellNext(&pMenu->node);
    if(pMenu)
        ellInsert(&savedPdbbase->menuList,ellPrevious(&pMenu->node),&pnewMenu->node);
    else
        ellAdd(&savedPdbbase->menuList,&pnewMenu->node);
    pgphentry = gphAdd(savedPdbbase->pgpHash,pnewMenu->name,&savedPdbbase->menuList);
    if(!pgphentry) {
        yyerrorAbort("gphAdd failed");
    } else {
        pgphentry->userPvt = pnewMenu;
    }
}

static void dbRecordtypeHead(char *name)
{
    dbRecordType        *pdbRecordType;
    GPHENTRY            *pgphentry;

    if (!*name) {
        yyerrorAbort("dbRecordtypeHead: Recordtype name can't be empty");
        return;
    }
    pgphentry = gphFind(savedPdbbase->pgpHash,name,&savedPdbbase->recordTypeList);
    if(pgphentry) {
        duplicate = TRUE;
        return;
    }
    pdbRecordType = dbCalloc(1,sizeof(dbRecordType));
    pdbRecordType->name = epicsStrDup(name);
    if (savedPdbbase->loadCdefs) ellInit(&pdbRecordType->cdefList);
    if(ellCount(&tempList))
        yyerrorAbort("dbRecordtypeHead tempList not empty");
    allocTemp(pdbRecordType);
}

static void dbRecordtypeFieldHead(char *name,char *type)
{
    dbFldDes            *pdbFldDes;
    int                 i;

    if (!*name) {
        yyerrorAbort("dbRecordtypeFieldHead: Field name can't be empty");
        return;
    }
    if(duplicate) return;
    pdbFldDes = dbCalloc(1,sizeof(dbFldDes));
    allocTemp(pdbFldDes);
    pdbFldDes->name = epicsStrDup(name);
    pdbFldDes->as_level = ASL1;
    pdbFldDes->isDevLink = strcmp(pdbFldDes->name, "INP")==0 ||
            strcmp(pdbFldDes->name, "OUT")==0;
    i = dbFindFieldType(type);
    if (i < 0)
        yyerrorAbort("Illegal Field Type");
    pdbFldDes->field_type = i;
}

static short findOrAddGuiGroup(const char *name)
{
    dbGuiGroup *pdbGuiGroup;
    GPHENTRY   *pgphentry;
    pgphentry = gphFind(savedPdbbase->pgpHash, name, &savedPdbbase->guiGroupList);
    if (!pgphentry) {
        pdbGuiGroup = dbCalloc(1,sizeof(dbGuiGroup));
        pdbGuiGroup->name = epicsStrDup(name);
        ellAdd(&savedPdbbase->guiGroupList, &pdbGuiGroup->node);
        pdbGuiGroup->key = ellCount(&savedPdbbase->guiGroupList);
        pgphentry = gphAdd(savedPdbbase->pgpHash, pdbGuiGroup->name, &savedPdbbase->guiGroupList);
        pgphentry->userPvt = pdbGuiGroup;
    }
    return ((dbGuiGroup *)pgphentry->userPvt)->key;
}

static void dbRecordtypeFieldItem(char *name,char *value)
{
    dbFldDes            *pdbFldDes;

    if(duplicate) return;
    pdbFldDes = (dbFldDes *)getLastTemp();
    if(strcmp(name,"asl")==0) {
        if(strcmp(value,"ASL0")==0) {
            pdbFldDes->as_level = ASL0;
        } else if(strcmp(value,"ASL1")==0) {
            pdbFldDes->as_level = ASL1;
        } else {
            yyerror("Illegal Access Security value: Must be ASL0 or ASL1");
        }
        return;
    }
    if(strcmp(name,"initial")==0) {
        pdbFldDes->initial = epicsStrDup(value);
        return;
    }
    if(strcmp(name,"promptgroup")==0) {
        pdbFldDes->promptgroup = findOrAddGuiGroup(value);
        return;
    }
    if(strcmp(name,"prompt")==0) {
        pdbFldDes->prompt = epicsStrDup(value);
        return;
    }
    if(strcmp(name,"special")==0) {
        int     i;
        for(i=0; i<SPC_NTYPES; i++) {
            if(strcmp(value,pamapspcType[i].strvalue)==0) {
                pdbFldDes->special = pamapspcType[i].value;
                return;
            }
        }
        if(sscanf(value,"%hd",&pdbFldDes->special)==1) {
            return;
        }
        yyerror("Illegal 'special' value.");
        return;
    }
    if(strcmp(name,"pp")==0) {
        if((strcmp(value,"YES")==0) || (strcmp(value,"TRUE")==0)) {
            pdbFldDes->process_passive = TRUE;
        } else if((strcmp(value,"NO")==0) || (strcmp(value,"FALSE")==0)) {
            pdbFldDes->process_passive = FALSE;
        } else {
            yyerror("Illegal 'pp' value, must be YES/NO/TRUE/FALSE");
        }
        return;
    }
    if(strcmp(name,"interest")==0) {
        if(sscanf(value,"%hd",&pdbFldDes->interest)!=1)
            yyerror("Illegal 'interest' value, must be integer");
        return;
    }
    if(strcmp(name,"base")==0) {
        if(strcmp(value,"DECIMAL")==0) {
            pdbFldDes->base = CT_DECIMAL;
        } else if(strcmp(value,"HEX")==0) {
            pdbFldDes->base = CT_HEX;
        } else {
            yyerror("Illegal 'base' value, must be DECIMAL/HEX");
        }
        return;
    }
    if(strcmp(name,"size")==0) {
        if(sscanf(value,"%hd",&pdbFldDes->size)!=1)
            yyerror("Illegal 'size' value, must be integer");
        return;
    }
    if(strcmp(name,"extra")==0) {
        pdbFldDes->extra = epicsStrDup(value);
        return;
    }
    if(strcmp(name,"menu")==0) {
        pdbFldDes->ftPvt = (dbMenu *)dbFindMenu(savedPdbbase,value);
        if(!savedPdbbase->ignoreMissingMenus && !pdbFldDes->ftPvt)
            yyerrorAbort("menu not found");
        return;
    }
    if(strcmp(name,"prop")==0) {
        if(strcmp(value, "YES")==0)
            pdbFldDes->prop = 1;
        else
            pdbFldDes->prop = 0;
        return;
    }
}

static void dbRecordtypeCdef(char *text) {
    dbText              *pdbCdef;
    tempListNode        *ptempListNode;
    dbRecordType        *pdbRecordType;

    if (!savedPdbbase->loadCdefs || duplicate) return;
    ptempListNode = (tempListNode *)ellFirst(&tempList);
    pdbRecordType = ptempListNode->item;

    pdbCdef = dbCalloc(1,sizeof(dbText));
    if (text[0] == ' ') text++; /* strip leading space if present */
    pdbCdef->text = epicsStrDup(text);
    ellAdd(&pdbRecordType->cdefList, &pdbCdef->node);
    return;
}

static void dbRecordtypeEmpty(void)
{
    tempListNode *ptempListNode;
    dbRecordType *pdbRecordType;

    if (duplicate) {
        duplicate = FALSE;
        return;
    }

    ptempListNode = (tempListNode *)ellFirst(&tempList);
    pdbRecordType = ptempListNode->item;
    epicsPrintf("Declaration of recordtype(%s) preceeded full definition.\n",
        pdbRecordType->name);
    yyerrorAbort(NULL);
}

static void dbRecordtypeBody(void)
{
    dbRecordType        *pdbRecordType;
    dbFldDes            *pdbFldDes;
    int                 i,j,ilink;
    GPHENTRY            *pgphentry;
    int                 no_fields,no_prompt,no_links;
    dbfType             field_type;
    char                *psortFldNameTemp;
    short               psortFldIndTemp;
    char                **papsortFldName;
    short               *sortFldInd;

    if(duplicate) {
        duplicate = FALSE;
        return;
    }
    pdbRecordType= (dbRecordType *)popFirstTemp();
    if(!pdbRecordType)
        return;
    pdbRecordType->no_fields = no_fields = ellCount(&tempList);
    pdbRecordType->papFldDes = dbCalloc(no_fields,sizeof(dbFldDes *));
    pdbRecordType->papsortFldName = dbCalloc(no_fields,sizeof(char *));
    pdbRecordType->sortFldInd = dbCalloc(no_fields,sizeof(short));
    no_prompt = no_links = 0;
    for(i=0; i<no_fields; i++) {
        pdbFldDes = (dbFldDes *)popFirstTemp();
        if(!pdbFldDes)
            return;
        pdbFldDes->pdbRecordType = pdbRecordType;
        pdbFldDes->indRecordType = i;
        pdbRecordType->papFldDes[i] = pdbFldDes;
        if(pdbFldDes->promptgroup) no_prompt++;
        field_type = pdbFldDes->field_type;
        if((field_type>=DBF_INLINK) && (field_type<=DBF_FWDLINK))no_links++;
        if((field_type==DBF_STRING) && (pdbFldDes->size==0))
            fprintf(stderr,"recordtype(%s).%s size not specified\n",
                pdbRecordType->name,pdbFldDes->name);
        if((field_type==DBF_NOACCESS) && (pdbFldDes->extra==0))
            fprintf(stderr,"recordtype(%s).%s extra not specified\n",
                pdbRecordType->name,pdbFldDes->name);
    }
    if (ellCount(&tempList))
        yyerrorAbort("dbRecordtypeBody: tempList not empty");
    pdbRecordType->no_prompt = no_prompt;
    pdbRecordType->no_links = no_links;
    pdbRecordType->link_ind = dbCalloc(no_links,sizeof(short));
    ilink = 0;
    for(i=0; i<no_fields; i++) {
        pdbFldDes = pdbRecordType->papFldDes[i];
        /* if prompt is null make it a null string */
        if(!pdbFldDes->prompt) pdbFldDes->prompt = dbCalloc(1,sizeof(char));
        field_type = pdbFldDes->field_type;
        if((field_type>=DBF_INLINK) && (field_type<=DBF_FWDLINK))
            pdbRecordType->link_ind[ilink++] = i;
        if(strcmp(pdbFldDes->name,"VAL")==0) {
            pdbRecordType->pvalFldDes = pdbRecordType->papFldDes[i];
            pdbRecordType->indvalFlddes = i;
        }
        pdbRecordType->papsortFldName[i] = pdbFldDes->name;
        pdbRecordType->sortFldInd[i] = i;
    }
   /*Now sort fields. Sorry dumb sort algorithm */
   papsortFldName = pdbRecordType->papsortFldName;
   sortFldInd = pdbRecordType->sortFldInd;
   for(i=0; i<no_fields; i++) {
        for(j=i+1; j<no_fields; j++) {
            if(strcmp(papsortFldName[j],papsortFldName[i])<0 ) {
                psortFldNameTemp = papsortFldName[j];
                psortFldIndTemp = sortFldInd[j];
                papsortFldName[j] = papsortFldName[i];
                sortFldInd[j] = sortFldInd[i];
                papsortFldName[i] = psortFldNameTemp;
                sortFldInd[i] = psortFldIndTemp;
            }
        }
    }
    /*Initialize lists*/
    ellInit(&pdbRecordType->attributeList);
    ellInit(&pdbRecordType->recList);
    ellInit(&pdbRecordType->devList);
    pgphentry = gphAdd(savedPdbbase->pgpHash,pdbRecordType->name,
        &savedPdbbase->recordTypeList);
    if(!pgphentry) {
        yyerrorAbort("gphAdd failed");
    } else {
        pgphentry->userPvt = pdbRecordType;
    }
    ellAdd(&savedPdbbase->recordTypeList,&pdbRecordType->node);
}

static void dbDevice(char *recordtype,char *linktype,
        char *dsetname,char *choicestring)
{
    devSup              *pdevSup;
    dbRecordType        *pdbRecordType;
    GPHENTRY            *pgphentry;
    int                 i,link_type;
    pgphentry = gphFind(savedPdbbase->pgpHash,recordtype,&savedPdbbase->recordTypeList);
    if(!pgphentry) {
        epicsPrintf("Record type \"%s\" not found for device \"%s\"\n",
                    recordtype, choicestring);
        yyerror(NULL);
        return;
    }
    link_type=-1;
    for(i=0; i<LINK_NTYPES; i++) {
        if(strcmp(pamaplinkType[i].strvalue,linktype)==0) {
            link_type = pamaplinkType[i].value;
            break;
        }
    }
    if(link_type==-1) {
        epicsPrintf("Bad link type \"%s\" for device \"%s\"\n",
                    linktype, choicestring);
        yyerror(NULL);
        return;
    }
    pdbRecordType = (dbRecordType *)pgphentry->userPvt;
    pgphentry = gphFind(savedPdbbase->pgpHash,choicestring,&pdbRecordType->devList);
    if(pgphentry) {
        return;
    }
    pdevSup = dbCalloc(1,sizeof(devSup));
    pdevSup->name = epicsStrDup(dsetname);
    pdevSup->choice = epicsStrDup(choicestring);
    pdevSup->link_type = link_type;
    pgphentry = gphAdd(savedPdbbase->pgpHash,pdevSup->choice,&pdbRecordType->devList);
    if(!pgphentry) {
        yyerrorAbort("gphAdd failed");
    } else {
        pgphentry->userPvt = pdevSup;
    }
    ellAdd(&pdbRecordType->devList,&pdevSup->node);
}

static void dbDriver(char *name)
{
    drvSup      *pdrvSup;
    GPHENTRY    *pgphentry;

    if (!*name) {
        yyerrorAbort("dbDriver: Driver name can't be empty");
        return;
    }
    pgphentry = gphFind(savedPdbbase->pgpHash,name,&savedPdbbase->drvList);
    if(pgphentry) {
        return;
    }
    pdrvSup = dbCalloc(1,sizeof(drvSup));
    pdrvSup->name = epicsStrDup(name);
    pgphentry = gphAdd(savedPdbbase->pgpHash,pdrvSup->name,&savedPdbbase->drvList);
    if(!pgphentry) {
        yyerrorAbort("gphAdd failed");
    }
    pgphentry->userPvt = pdrvSup;
    ellAdd(&savedPdbbase->drvList,&pdrvSup->node);
}

static void dbLinkType(char *name, char *jlif_name)
{
    linkSup *pLinkSup;
    GPHENTRY *pgphentry;

    pgphentry = gphFind(savedPdbbase->pgpHash, name, &savedPdbbase->linkList);
    if (pgphentry) {
        return;
    }
    pLinkSup = dbCalloc(1,sizeof(linkSup));
    pLinkSup->name = epicsStrDup(name);
    pLinkSup->jlif_name = epicsStrDup(jlif_name);
    pgphentry = gphAdd(savedPdbbase->pgpHash, pLinkSup->name, &savedPdbbase->linkList);
    if (!pgphentry) {
        yyerrorAbort("gphAdd failed");
    }
    pgphentry->userPvt = pLinkSup;
    ellAdd(&savedPdbbase->linkList, &pLinkSup->node);
}

static void dbRegistrar(char *name)
{
    dbText      *ptext;
    GPHENTRY    *pgphentry;

    if (!*name) {
        yyerrorAbort("dbRegistrar: Registrar name can't be empty");
        return;
    }
    pgphentry = gphFind(savedPdbbase->pgpHash,name,&savedPdbbase->registrarList);
    if(pgphentry) {
        return;
    }
    ptext = dbCalloc(1,sizeof(dbText));
    ptext->text = epicsStrDup(name);
    pgphentry = gphAdd(savedPdbbase->pgpHash,ptext->text,&savedPdbbase->registrarList);
    if(!pgphentry) {
        yyerrorAbort("gphAdd failed");
    }
    pgphentry->userPvt = ptext;
    ellAdd(&savedPdbbase->registrarList,&ptext->node);
}

static void dbFunction(char *name)
{
    dbText     *ptext;
    GPHENTRY   *pgphentry;

    if (!*name) {
        yyerrorAbort("dbFunction: Function name can't be empty");
        return;
    }
    pgphentry = gphFind(savedPdbbase->pgpHash,name,&savedPdbbase->functionList);
    if(pgphentry) {
       return;
    }
    ptext = dbCalloc(1,sizeof(dbText));
    ptext->text = epicsStrDup(name);
    pgphentry = gphAdd(savedPdbbase->pgpHash,ptext->text,&savedPdbbase->functionList);
    if(!pgphentry) {
       yyerrorAbort("gphAdd failed");
    }
    pgphentry->userPvt = ptext;
    ellAdd(&savedPdbbase->functionList,&ptext->node);
}

static void dbVariable(char *name, char *type)
{
    dbVariableDef       *pvar;
    GPHENTRY            *pgphentry;

    if (!*name) {
        yyerrorAbort("dbVariable: Variable name can't be empty");
        return;
    }
    pgphentry = gphFind(savedPdbbase->pgpHash,name,&savedPdbbase->variableList);
    if(pgphentry) {
        return;
    }
    pvar = dbCalloc(1,sizeof(dbVariableDef));
    pvar->name = epicsStrDup(name);
    pvar->type = epicsStrDup(type);
    pgphentry = gphAdd(savedPdbbase->pgpHash,pvar->name,&savedPdbbase->variableList);
    if(!pgphentry) {
        yyerrorAbort("gphAdd failed");
    }
    pgphentry->userPvt = pvar;
    ellAdd(&savedPdbbase->variableList,&pvar->node);
}

static void dbBreakHead(char *name)
{
    brkTable    *pbrkTable;
    GPHENTRY    *pgphentry;

    if (!*name) {
        yyerrorAbort("dbBreakHead: Breaktable name can't be empty");
        return;
    }
    pgphentry = gphFind(savedPdbbase->pgpHash,name,&savedPdbbase->bptList);
    if(pgphentry) {
        duplicate = TRUE;
        return;
    }
    pbrkTable = dbCalloc(1,sizeof(brkTable));
    pbrkTable->name = epicsStrDup(name);
    if(ellCount(&tempList)) yyerrorAbort("dbBreakHead:tempList not empty");
    allocTemp(pbrkTable);
}

static void dbBreakItem(char *value)
{
    double dummy;
    if (duplicate) return;
    if (epicsScanDouble(value, &dummy) != 1) {
        yyerrorAbort("Non-numeric value in breaktable");
    }
    allocTemp(epicsStrDup(value));
}

static void dbBreakBody(void)
{
    brkTable            *pnewbrkTable;
    brkInt              *paBrkInt;
    brkTable            *pbrkTable;
    int                 number, down=0;
    int                 i;
    GPHENTRY            *pgphentry;

    if (duplicate) {
        duplicate = FALSE;
        return;
    }
    pnewbrkTable = (brkTable *)popFirstTemp();
    if(!pnewbrkTable)
        return;
    number = ellCount(&tempList);
    if (number % 2) {
        yyerrorAbort("breaktable: Raw value missing");
        return;
    }
    number /= 2;
    if (number < 2) {
        yyerrorAbort("breaktable: Must have at least two points!");
        return;
    }
    pnewbrkTable->number = number;
    pnewbrkTable->paBrkInt = paBrkInt = dbCalloc(number, sizeof(brkInt));
    for (i=0; i<number; i++) {
        char    *str;

        str = (char *)popFirstTemp();
        if(!str)
            return;
        (void) epicsScanDouble(str, &paBrkInt[i].raw);
        free(str);

        str = (char *)popFirstTemp();
        if(!str)
            return;
        (void) epicsScanDouble(str, &paBrkInt[i].eng);
        free(str);
    }
    /* Compute slopes */
    for (i=0; i<number-1; i++) {
        double slope =
          (paBrkInt[i+1].eng - paBrkInt[i].eng)/
          (paBrkInt[i+1].raw - paBrkInt[i].raw);
        if (!dbBptNotMonotonic && slope == 0) {
            yyerrorAbort("breaktable slope is zero");
            return;
        }
        if (i == 0) {
            down = (slope < 0);
        } else if (!dbBptNotMonotonic && down != (slope < 0)) {
            yyerrorAbort("breaktable slope changes sign");
            return;
        }
        paBrkInt[i].slope = slope;
    }
    /* Continue with last slope beyond the final point */
    paBrkInt[number-1].slope = paBrkInt[number-2].slope;
    /* Add brkTable in sorted order */
    pbrkTable = (brkTable *)ellFirst(&savedPdbbase->bptList);
    while (pbrkTable) {
        if (strcmp(pbrkTable->name, pnewbrkTable->name) > 0) {
            ellInsert(&savedPdbbase->bptList, ellPrevious((ELLNODE *)pbrkTable),
                (ELLNODE *)pnewbrkTable);
            break;
        }
        pbrkTable = (brkTable *)ellNext(&pbrkTable->node);
    }
    if (!pbrkTable) ellAdd(&savedPdbbase->bptList, &pnewbrkTable->node);
    pgphentry = gphAdd(savedPdbbase->pgpHash,pnewbrkTable->name,&savedPdbbase->bptList);
    if (!pgphentry) {
        yyerrorAbort("dbBreakBody: gphAdd failed");
        return;
    }
    pgphentry->userPvt = pnewbrkTable;
}

static
int dbRecordNameValidate(const char *name)
{
    size_t i=0u;
    const char *pos = name;

    if (!*name) {
        yyerrorAbort("Error: Record/Alias name can't be empty");
        return 1;
    }

    for(; *pos; i++, pos++) {
        unsigned char c = *pos;
        if(i==0) {
            /* first character restrictions */
            if(c=='-' || c=='+' || c=='[' || c=='{') {
                errlogPrintf("Warning: Record/Alias name '%s' should not begin with '%c'\n", name, c);
            }
        }
        /* any character restrictions */
        if(c < ' ') {
            errlogPrintf("Warning: Record/Alias name '%s' should not contain non-printable 0x%02x\n",
                         name, c);

        } else if(c==' ' || c=='\t' || c=='"' || c=='\'' || c=='.' || c=='$') {
            epicsPrintf("Error: Bad character '%c' in Record/Alias name \"%s\"\n",
                c, name);
            yyerrorAbort(NULL);
            return 1;
        }
    }

    return 0;
}

static void dbRecordHead(char *recordType, char *name, int visible)
{
    DBENTRY *pdbentry;
    long status;

    if(dbRecordNameValidate(name))
        return;

    pdbentry = dbAllocEntry(savedPdbbase);
    if (ellCount(&tempList))
        yyerrorAbort("dbRecordHead: tempList not empty");
    allocTemp(pdbentry);

    if (recordType[0] == '*' && recordType[1] == 0) {
        if (dbRecordsOnceOnly)
            epicsPrintf("Record-type \"*\" not valid with dbRecordsOnceOnly\n");
        else {
            status = dbFindRecord(pdbentry, name);
            if (status == 0)
                return; /* done */
            epicsPrintf("Record \"%s\" not found\n", name);
        }
        yyerror(NULL);
        duplicate = TRUE;
        return;
    }

    status = dbFindRecordType(pdbentry, recordType);
    if (status) {
        epicsPrintf("Record \"%s\" is of unknown type \"%s\"\n",
                    name, recordType);
        yyerrorAbort(NULL);
        return;
    }

    /*Duplicate records are ok if the same type */

    status = dbCreateRecord(pdbentry,name);
    if (status == S_dbLib_recExists) {
        if (strcmp(recordType, dbGetRecordTypeName(pdbentry)) != 0) {
            epicsPrintf("Record \"%s\" of type \"%s\" redefined with new type "
                "\"%s\"\n", name, dbGetRecordTypeName(pdbentry), recordType);
            yyerror(NULL);
            duplicate = TRUE;
            return;
        }
        else if (dbRecordsOnceOnly) {
            epicsPrintf("Record \"%s\" already defined (dbRecordsOnceOnly is "
                "set)\n", name);
            yyerror(NULL);
            duplicate = TRUE;
        }
    }
    else if (status) {
        epicsPrintf("Can't create record \"%s\" of type \"%s\"\n",
                     name, recordType);
        yyerrorAbort(NULL);
    }

    if (visible)
        dbVisibleRecord(pdbentry);
}

static void dbRecordField(char *name,char *value)
{
    DBENTRY *pdbentry;
    tempListNode *ptempListNode;
    long status;

    if (duplicate) return;
    ptempListNode = (tempListNode *)ellFirst(&tempList);
    pdbentry = ptempListNode->item;
    status = dbFindField(pdbentry,name);
    if (status) {
        epicsPrintf("Record \"%s\" does not have a field \"%s\"\n",
            dbGetRecordName(pdbentry), name);
        yyerror(NULL);
        return;
    }
    if (pdbentry->indfield == 0) {
        epicsPrintf("Can't set \"NAME\" field of record \"%s\"\n",
            dbGetRecordName(pdbentry));
        yyerror(NULL);
        return;
    }

    if (*value == '"' || *value == '\'') {
        /* jsonSTRING values still have their quotes */
        value++;
        value[strlen(value) - 1] = 0;
        dbTranslateEscape(value, value);    /* in-place; safe & legal */
    }

    status = dbPutString(pdbentry,value);
    if (status) {
        char msg[128];

        errSymLookup(status, msg, sizeof(msg));
        epicsPrintf("Can't set \"%s.%s\" to \"%s\" %s : %s\n",
            dbGetRecordName(pdbentry), name, value, pdbentry->message ? pdbentry->message : "", msg);
        dbPutStringSuggest(pdbentry, value);
        yyerror(NULL);
        return;
    }
}

static void dbRecordInfo(char *name, char *value)
{
    DBENTRY *pdbentry;
    tempListNode *ptempListNode;
    long status;

    if (!*name) {
        yyerrorAbort("dbRecordInfo: Info item name can't be empty");
        return;
    }
    if (duplicate) return;
    ptempListNode = (tempListNode *)ellFirst(&tempList);
    pdbentry = ptempListNode->item;

    if (*value == '"' || *value == '\'') {
        /* jsonSTRING values still have their quotes */
        value++;
        value[strlen(value) - 1] = 0;
        dbTranslateEscape(value, value);    /* in-place; safe & legal */
    }

    status = dbPutInfo(pdbentry,name,value);
    if (status) {
        epicsPrintf("Can't set \"%s\" info \"%s\" to \"%s\"\n",
                    dbGetRecordName(pdbentry), name, value);
        yyerror(NULL);
        return;
    }
}

static void dbRecordAlias(char *name)
{
    DBENTRY *pdbentry;
    tempListNode *ptempListNode;
    long status;

    if(dbRecordNameValidate(name))
        return;

    if (duplicate) return;
    ptempListNode = (tempListNode *)ellFirst(&tempList);
    pdbentry = ptempListNode->item;
    status = dbCreateAlias(pdbentry, name);
    if (status) {
        epicsPrintf("Can't create alias \"%s\" for \"%s\"\n",
                    name, dbGetRecordName(pdbentry));
        yyerror(NULL);
        return;
    }
}

static void dbAlias(char *name, char *alias)
{
    DBENTRY dbEntry;
    DBENTRY *pdbEntry = &dbEntry;

    if(dbRecordNameValidate(alias) || dbRecordNameValidate(name))
        return;

    dbInitEntry(savedPdbbase, pdbEntry);
    if (dbFindRecord(pdbEntry, name)) {
        epicsPrintf("Alias \"%s\" refers to unknown record \"%s\"\n",
                    alias, name);
        yyerror(NULL);
    }
    else if (dbCreateAlias(pdbEntry, alias)) {
        epicsPrintf("Can't create alias \"%s\" referring to \"%s\"\n",
                    alias, name);
        yyerror(NULL);
    }
    dbFinishEntry(pdbEntry);
}

static void dbRecordBody(void)
{
    DBENTRY *pdbentry;

    if (duplicate) {
        duplicate = FALSE;
        return;
    }
    pdbentry = (DBENTRY *)popFirstTemp();
    if (ellCount(&tempList))
        yyerrorAbort("dbRecordBody: tempList not empty");
    dbFreeEntry(pdbentry);
}
