/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* EPICS Base is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* msi - macro substitutions and include */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <dbDefs.h>
#include <macLib.h>
#include <ellLib.h>
#include <errlog.h>
#include <epicsString.h>
#include <osiFileName.h>

#define MAX_BUFFER_SIZE 4096
#define MAX_DEPS 1024

/* Module to read the template files */
typedef struct inputData inputData;

static void inputConstruct(inputData **ppvt);
static void inputDestruct(inputData *pvt);
static void inputAddPath(inputData *pvt, char *pval);
static void inputBegin(inputData *pvt, char *fileName);
static char *inputNextLine(inputData *pvt);
static void inputNewIncludeFile(inputData *pvt, char *name);
static void inputErrPrint(inputData *pvt);

/* Module to read the substitution file */
typedef struct subInfo subInfo;

static void substituteOpen(subInfo **ppvt, char *substitutionName);
static void substituteDestruct(subInfo *pvt);
static int substituteGetNextSet(subInfo *pvt, char **filename);
static int substituteGetGlobalSet(subInfo *pvt);
static char *substituteGetReplacements(subInfo *pvt);
static char *substituteGetGlobalReplacements(subInfo *pvt);

/* Forward references to local routines */
static void usageExit(int status);
static void addMacroReplacements(MAC_HANDLE *macPvt, char *pval);
static void makeSubstitutions(inputData *inputPvt, MAC_HANDLE *macPvt, char *templateName);

/*Global variables */
static int opt_V = 0;
static int opt_D = 0;

static char *outFile = 0;
static int numDeps = 0, depHashes[MAX_DEPS];


int main(int argc,char **argv)
{
    inputData *inputPvt;
    MAC_HANDLE *macPvt;
    char *pval;
    int  narg;
    char *substitutionName=0;
    char *templateName=0;
    int  i;
    int  localScope = 1;

    inputConstruct(&inputPvt);
    macCreateHandle(&macPvt,0);
    while((argc>1) && (argv[1][0] == '-')) {
        narg = (strlen(argv[1])==2) ? 2 : 1;
        pval = (narg==1) ? (argv[1]+2) : argv[2];
        if(strncmp(argv[1],"-I",2)==0) {
            inputAddPath(inputPvt,pval);
        } else if (strcmp(argv[1], "-D") == 0) {
            opt_D = 1;
            narg = 1; /* no argument for this option */
        } else if(strncmp(argv[1],"-o",2)==0) {
            outFile = epicsStrDup(pval);
        } else if(strncmp(argv[1],"-M",2)==0) {
            addMacroReplacements(macPvt,pval);
        } else if(strncmp(argv[1],"-S",2)==0) {
            substitutionName = epicsStrDup(pval);
        } else if (strcmp(argv[1], "-V") == 0) {
            opt_V = 1;
            narg = 1; /* no argument for this option */
        } else if (strcmp(argv[1], "-g") == 0) {
            localScope = 0;
            narg = 1; /* no argument for this option */
        } else if (strcmp(argv[1], "-h") == 0) {
            usageExit(0);
        } else {
            fprintf(stderr, "msi: Bad argument \"%s\"\n", argv[1]);
            usageExit(1);
        }
        argc -= narg;
        for(i=1; i<argc; i++) argv[i] = argv[i + narg];
    }
    if (!opt_V)
        macSuppressWarning(macPvt,1);
    if(argc>2) {
        fprintf(stderr,"msi: Too many arguments\n");
        usageExit(1);
    }
    if (opt_D) {
        if (!outFile) {
            fprintf(stderr, "msi: Option -D requires -o for Makefile target\n");
            exit(1);
        }
        printf("%s:", outFile);
    }
    else if (outFile && freopen(outFile, "w", stdout) == NULL) {
        fprintf(stderr, "msi: Can't open %s for writing: %s\n",
            outFile, strerror(errno));
        exit(1);
    }
    if(argc==2) {
        templateName = epicsStrDup(argv[1]);
    }
    if(!substitutionName) {
        makeSubstitutions(inputPvt,macPvt,templateName);
    } else {
        subInfo *substitutePvt;
        char *filename = 0;
        int isGlobal, isFile;

        substituteOpen(&substitutePvt,substitutionName);
        do {
            if ((isGlobal = substituteGetGlobalSet(substitutePvt))) {
                pval = substituteGetGlobalReplacements(substitutePvt);
                if(pval) {
                    addMacroReplacements(macPvt,pval);
                }
            } else if ((isFile = substituteGetNextSet(substitutePvt,&filename))) {
                if(templateName) filename = templateName;
                if(!filename) {
                    fprintf(stderr,"msi: No template file\n");
                    usageExit(1);
                }
                while((pval = substituteGetReplacements(substitutePvt))){
                    if (localScope) macPushScope(macPvt);
                    addMacroReplacements(macPvt,pval);
                    makeSubstitutions(inputPvt,macPvt,filename);
                    if (localScope) macPopScope(macPvt);
                }
            }
        } while (isGlobal || isFile);
        substituteDestruct(substitutePvt);
    }
    errlogFlush();
    macDeleteHandle(macPvt);
    inputDestruct(inputPvt);
    if (opt_D) {
        printf("\n");
    }
    free(templateName);
    free(substitutionName);
    return opt_V & 2;
}

void usageExit(int status)
{
    fprintf(stderr,
        "Usage: msi [options] [template]\n"
        "  stdin is used if neither template nor substitution file is given\n"
        "  options:\n"
        "    -h        Print this help message\n"
        "    -D        Output file dependencies, not substitutions\n"
        "    -V        Undefined macros generate an error\n"
        "    -g        All macros have global scope\n"
        "    -o<FILE>  Send output to <FILE>\n"
        "    -I<DIR>   Add <DIR> to include file search path\n"
        "    -M<SUBST> Add <SUBST> to (global) macro definitions\n"
        "              (<SUBST> takes the form VAR=VALUE,...)\n"
        "    -S<FILE>  Expand the substitutions in FILE\n");
    exit(status);
}

static void addMacroReplacements(MAC_HANDLE *macPvt,char *pval)
{
    char **pairs;
    long status;

    status = macParseDefns(macPvt,pval,&pairs);
    if(status==-1) {
        fprintf(stderr,"msi: Error from macParseDefns\n");
        usageExit(1);
    }
    if(status) {
        status = macInstallMacros(macPvt,pairs);
        if(!status) {
            fprintf(stderr,"Error from macInstallMacros\n");
            usageExit(1);
        }
        free(pairs);
    }
}

typedef enum {cmdInclude,cmdSubstitute} cmdType;
static const char *cmdNames[] = {"include","substitute"};

static void makeSubstitutions(inputData *inputPvt, MAC_HANDLE *macPvt, char *templateName)
{
    char *input;
    static char buffer[MAX_BUFFER_SIZE];
    int  n;

    inputBegin(inputPvt,templateName);
    while((input = inputNextLine(inputPvt))) {
        int     expand=1;
        char    *p;
        char    *command = 0;

        p = input; 
        /*skip whitespace at beginning of line*/
        while(*p && (isspace((int) *p))) ++p;
        /*Look for i or s */
        if(*p && (*p=='i' || *p=='s')) command = p;
        if(command) {
            char *pstart;
            char *pend;
            char *copy;
            int  cmdind=-1;
            int  i;
            
            for(i=0; i< NELEMENTS(cmdNames); i++) {
                if(strstr(command,cmdNames[i])) {
                    cmdind = i;
                }
            }
            if(cmdind<0) goto endif;
            p = command + strlen(cmdNames[cmdind]);
            /*skip whitespace after command*/
            while(*p && (isspace((int) *p))) ++p;
            /*Next character must be quote*/
            if((*p==0) || (*p!='"')) goto endif;
            pstart = ++p;
            /*Look for end quote*/
            while(*p && (*p!='"')) {
                /*allow escape for imbeded quote*/
                if((*p=='\\') && *(p+1)=='"') {
                    p += 2; continue;
                } else {
                    if(*p=='"') break;
                }
                ++p;
            }
            pend = p;
            if(*p==0) goto endif;
            /*skip quote and any trailing blanks*/
            while(*++p==' ') ;
            if(*p != '\n' && *p !=0) goto endif;
            copy = calloc(pend-pstart+1,sizeof(char));
            strncpy(copy,pstart,pend-pstart);
            switch(cmdind) {
            case cmdInclude:
                inputNewIncludeFile(inputPvt,copy);
                break;
            case cmdSubstitute:
                addMacroReplacements(macPvt,copy);
                break;
            default:
                fprintf(stderr,"msi: Logic error in makeSubstitutions\n");
                inputErrPrint(inputPvt);
                exit(1);
            }
            free(copy);
            expand = 0;
        }
endif:
        if (expand && !opt_D) {
            n = macExpandString(macPvt,input,buffer,MAX_BUFFER_SIZE-1);
            fputs(buffer,stdout);
            if (opt_V == 1 && n < 0) {
                fprintf(stderr,"msi: Error - undefined macros present\n");
                opt_V++;
            }
        }
    }
}

typedef struct inputFile{
    ELLNODE     node;
    char        *filename;
    FILE        *fp;
    int         lineNum;
}inputFile;

typedef struct pathNode {
    ELLNODE     node;
    char        *directory;
} pathNode;

struct inputData {
    ELLLIST     inputFileList;
    ELLLIST     pathList;
    char        inputBuffer[MAX_BUFFER_SIZE];
};

static void inputOpenFile(inputData *pinputData,char *filename);
static void inputCloseFile(inputData *pinputData);
static void inputCloseAllFiles(inputData *pinputData);

static void inputConstruct(inputData **ppvt)
{
    inputData   *pinputData;

    pinputData = calloc(1,sizeof(inputData));
    ellInit(&pinputData->inputFileList);
    ellInit(&pinputData->pathList);
    *ppvt = pinputData;
}

static void inputDestruct(inputData *pinputData)
{
    pathNode    *ppathNode;

    inputCloseAllFiles(pinputData);
    while((ppathNode = (pathNode *)ellFirst(&pinputData->pathList))) {
        ellDelete(&pinputData->pathList,&ppathNode->node);
        free(ppathNode->directory);
        free(ppathNode);
    }
    free(pinputData);
}

static void inputAddPath(inputData *pinputData, char *path)
{
    ELLLIST     *ppathList = &pinputData->pathList;
    pathNode    *ppathNode;
    const char  *pcolon;
    const char  *pdir;
    size_t      len;
    int         emptyName;
    const char  sep = *OSI_PATH_LIST_SEPARATOR;

    pdir = path;
    /*an empty name at beginning, middle, or end means current directory*/
    while(pdir && *pdir) {
        emptyName = ((*pdir == sep) ? 1 : 0);
        if(emptyName) ++pdir;
        ppathNode = (pathNode *)calloc(1,sizeof(pathNode));
        ellAdd(ppathList,&ppathNode->node);
        if(!emptyName) {
            pcolon = strchr(pdir,sep);
            len = (pcolon ? (pcolon - pdir) : strlen(pdir));
            if(len>0)  {
                ppathNode->directory = (char *)calloc(len+1,sizeof(char));
                strncpy(ppathNode->directory,pdir,len);
                pdir = pcolon;
                /*unless at end skip past first colon*/
                if(pdir && *(pdir+1)!=0) ++pdir;
            } else { /*must have been trailing : */
                emptyName=1;
            }
        }
        if(emptyName) {
            ppathNode->directory = (char *)calloc(2,sizeof(char));
            strcpy(ppathNode->directory,".");
        }
    }
    return;
}

static void inputBegin(inputData *pinputData, char *fileName)
{
    inputCloseAllFiles(pinputData);
    inputOpenFile(pinputData,fileName);
}

static char *inputNextLine(inputData *pinputData)
{
    inputFile   *pinputFile;
    char        *pline;

    while((pinputFile = (inputFile *)ellFirst(&pinputData->inputFileList))) {
        pline = fgets(pinputData->inputBuffer,MAX_BUFFER_SIZE,pinputFile->fp);
        if(pline) {
            ++pinputFile->lineNum;
            return(pline);
        }
        inputCloseFile(pinputData);
    }
    return(0);
}

static void inputNewIncludeFile(inputData *pinputData, char *name)
{
    inputOpenFile(pinputData,name);
}

static void inputErrPrint(inputData *pinputData)
{
    inputFile   *pinputFile;

    fprintf(stderr,"input: '%s' at ",pinputData->inputBuffer);
    pinputFile = (inputFile *)ellFirst(&pinputData->inputFileList);
    while(pinputFile) {
        fprintf(stderr,"line %d of ",pinputFile->lineNum);
        if(pinputFile->filename) {
            fprintf(stderr," file %s\n",pinputFile->filename);
        } else {
            fprintf(stderr,"stdin:\n");
        }
        pinputFile = (inputFile *)ellNext(&pinputFile->node);
        if(pinputFile) {
            fprintf(stderr,"  included from ");
        } else {
            fprintf(stderr,"\n");
        }
    }
    fprintf(stderr,"\n");
}

static void inputOpenFile(inputData *pinputData,char *filename)
{
    ELLLIST     *ppathList = &pinputData->pathList;
    pathNode    *ppathNode = 0;
    inputFile   *pinputFile;
    char        *fullname = 0;
    FILE        *fp = 0;

    if(!filename) {
        fp = stdin;
    } else if((ellCount(ppathList)==0) || strchr(filename,'/')){
        fp = fopen(filename,"r");
    } else {
        ppathNode = (pathNode *)ellFirst(ppathList);
        while(ppathNode) {
            fullname = calloc(strlen(filename)+strlen(ppathNode->directory) +2,
                sizeof(char));
            strcpy(fullname,ppathNode->directory);
            strcat(fullname,"/");
            strcat(fullname,filename);
            fp = fopen(fullname,"r");
            if(fp) break;
            free(fullname);
            ppathNode = (pathNode *)ellNext(&ppathNode->node);
        }
    }
    if(!fp) {
        fprintf(stderr,"msi: Can't open file '%s'\n",filename);
        inputErrPrint(pinputData);
        exit(1);
    }
    pinputFile = calloc(1,sizeof(inputFile));
    if(ppathNode) {
        pinputFile->filename = fullname;
    } else if(filename) {
        pinputFile->filename = epicsStrDup(filename);
    } else {
        pinputFile->filename = epicsStrDup("stdin");
    }

    if (opt_D) {
        int hash = epicsStrHash(pinputFile->filename, 12345);
        int i = 0;
        int match = 0;

        while (i < numDeps) {
            if (hash == depHashes[i++]) {
                match = 1;
                break;
            }
        }
        if (!match) {
            const char *wrap = numDeps ? " \\\n" : "";

            printf("%s %s", wrap, pinputFile->filename);
            if (numDeps < MAX_DEPS) {
                depHashes[numDeps++] = hash;
            }
            else {
                fprintf(stderr, "msi: More than %d dependencies!\n", MAX_DEPS);
                depHashes[0] = hash;
            }
        }
    }

    pinputFile->fp = fp;
    ellInsert(&pinputData->inputFileList,0,&pinputFile->node);
}

static void inputCloseFile(inputData *pinputData)
{
    inputFile *pinputFile;

    pinputFile = (inputFile *)ellFirst(&pinputData->inputFileList);
    if(!pinputFile) return;
    ellDelete(&pinputData->inputFileList,&pinputFile->node);
    if(fclose(pinputFile->fp)) 
        fprintf(stderr,"msi: Can't close input file '%s'\n",pinputFile->filename);
    free(pinputFile->filename);
    free(pinputFile);
}

static void inputCloseAllFiles(inputData *pinputData)
{
    inputFile   *pinputFile;

    while((pinputFile=(inputFile *)ellFirst(&pinputData->inputFileList))){
        inputCloseFile(pinputData);
    }
}

/*start of code that handles substitution file*/
typedef enum {
    tokenLBrace,tokenRBrace,tokenSeparater,tokenString,tokenEOF
}tokenType;

typedef struct subFile {
    char        *substitutionName;
    FILE        *fp;
    int         lineNum;
    char        inputBuffer[MAX_BUFFER_SIZE];
    char        *pnextChar;
    tokenType   token;
    char        string[MAX_BUFFER_SIZE];
} subFile;

typedef struct patternNode {
    ELLNODE     node;
    char        *var;
} patternNode;

struct subInfo {
    subFile     *psubFile;
    int         isFile;
    char        *filename;
    int         isPattern;
    ELLLIST     patternList;
    size_t      size;
    size_t      curLength;
    char        *macroReplacements;
};

static char *subGetNextLine(subFile *psubFile);
static tokenType subGetNextToken(subFile *psubFile);
static void subFileErrPrint(subFile *psubFile,char * message);
static void freeSubFile(subInfo *psubInfo);
static void freePattern(subInfo *psubInfo);
static void catMacroReplacements(subInfo *psubInfo,const char *value);

void freeSubFile(subInfo *psubInfo)
{
    subFile     *psubFile = psubInfo->psubFile;
    if(psubFile->fp) {
        if(fclose(psubFile->fp))
            fprintf(stderr,"msi: Can't close substitution file\n");
    }
    free(psubFile);
    free(psubInfo->filename);
    psubInfo->psubFile = 0;
}

void freePattern(subInfo *psubInfo)
{
    patternNode *ppatternNode;
    while((ppatternNode = (patternNode *)ellFirst(&psubInfo->patternList))) {
        ellDelete(&psubInfo->patternList,&ppatternNode->node);
        free(ppatternNode->var);
        free(ppatternNode);
    }
    psubInfo->isPattern = 0;
}

static void substituteDestruct(subInfo *psubInfo)
{
    freeSubFile(psubInfo);
    freePattern(psubInfo);
    free(psubInfo);
    return;
}

static void substituteOpen(subInfo **ppvt,char *substitutionName)
{
    subInfo     *psubInfo;
    subFile     *psubFile;
    FILE        *fp;

    psubInfo = calloc(1,sizeof(subInfo));
    *ppvt = psubInfo;
    psubFile = calloc(1,sizeof(subFile));
    psubInfo->psubFile = psubFile;
    ellInit(&psubInfo->patternList);
    fp = fopen(substitutionName,"r");
    if(!fp) {
        fprintf(stderr,"msi: Can't open file '%s'\n",substitutionName);
        exit(1);
    }
    psubFile->substitutionName = substitutionName;
    psubFile->fp = fp;
    psubFile->lineNum = 1;
    psubFile->inputBuffer[0] = 0;
    psubFile->pnextChar = &psubFile->inputBuffer[0];
    subGetNextToken(psubFile);
    return;
}

static int substituteGetGlobalSet(subInfo *psubInfo)
{
    subFile     *psubFile = psubInfo->psubFile;

    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenString && strcmp(psubFile->string,"global")==0) {
        subGetNextToken(psubFile);
        return(1);
    }
    return(0);
}

static int substituteGetNextSet(subInfo *psubInfo,char **filename)
{
    subFile     *psubFile = psubInfo->psubFile;
    patternNode *ppatternNode;

    *filename = 0;
    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenEOF) return(0);
    if(psubFile->token==tokenString && strcmp(psubFile->string,"file")==0) {
        psubInfo->isFile = 1;
        if(subGetNextToken(psubFile)!=tokenString) {
            subFileErrPrint(psubFile,"Parse error, expecting filename");
            exit(1);
        }
        freePattern(psubInfo);
        free(psubInfo->filename);
        if(psubFile->string[0]=='"'&&psubFile->string[strlen(psubFile->string)-1]=='"') {
            psubFile->string[strlen(psubFile->string)-1]='\0';
            psubInfo->filename = macEnvExpand(psubFile->string+1);
        }
        else {
            psubInfo->filename = macEnvExpand(psubFile->string);
        }
        while(subGetNextToken(psubFile)==tokenSeparater);
        if(psubFile->token!=tokenLBrace) {
            subFileErrPrint(psubFile,"Parse error, expecting {");
            exit(1);
        }
        subGetNextToken(psubFile);
    }
    *filename = psubInfo->filename;
    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenLBrace) return(1);
    if(psubFile->token==tokenRBrace) return(1);
    if(psubFile->token!=tokenString
    || strcmp(psubFile->string,"pattern")!=0) {
        subFileErrPrint(psubFile,"Parse error, expecting pattern");
        exit(1);
    }
    freePattern(psubInfo);
    psubInfo->isPattern = 1;
    while(subGetNextToken(psubFile)==tokenSeparater);
    if(psubFile->token!=tokenLBrace) {
        subFileErrPrint(psubFile,"Parse error, expecting {");
        exit(1);
    }
    while(1) {
        while(subGetNextToken(psubFile)==tokenSeparater);
        if(psubFile->token!=tokenString) break;
        ppatternNode = calloc(1,sizeof(patternNode));
        ellAdd(&psubInfo->patternList,&ppatternNode->node);
        ppatternNode->var = epicsStrDup(psubFile->string);
    }
    if(psubFile->token!=tokenRBrace) {
        subFileErrPrint(psubFile,"Parse error, expecting }");
        exit(1);
    }
    subGetNextToken(psubFile);
    return(1);
}

static char *substituteGetGlobalReplacements(subInfo *psubInfo)
{
    subFile     *psubFile = psubInfo->psubFile;

    if(psubInfo->macroReplacements) psubInfo->macroReplacements[0] = 0;
    psubInfo->curLength = 0;
    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenRBrace && psubInfo->isFile) {
        psubInfo->isFile = 0;
        free(psubInfo->filename);
        psubInfo->filename = 0;
        freePattern(psubInfo);
        subGetNextToken(psubFile);
        return(0);
    }
    if(psubFile->token==tokenEOF) return(0);
    if(psubFile->token!=tokenLBrace) return(0);
    while(1) {
        switch(subGetNextToken(psubFile)) {
            case tokenRBrace:
                subGetNextToken(psubFile);
                if (!psubInfo->macroReplacements) {
                    catMacroReplacements(psubInfo,"");
                }
                return(psubInfo->macroReplacements);
            case tokenSeparater:
                catMacroReplacements(psubInfo,",");
                break;
            case tokenString:
                catMacroReplacements(psubInfo,psubFile->string);
                break;
            default:
                subFileErrPrint(psubFile,"Parse error, illegal token");
                exit(1);
        }
    }
}

static char *substituteGetReplacements(subInfo *psubInfo)
{
    subFile     *psubFile = psubInfo->psubFile;
    patternNode *ppatternNode;

    if(psubInfo->macroReplacements) psubInfo->macroReplacements[0] = 0;
    psubInfo->curLength = 0;
    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenRBrace && psubInfo->isFile) {
        psubInfo->isFile = 0;
        free(psubInfo->filename);
        psubInfo->filename = 0;
        freePattern(psubInfo);
        subGetNextToken(psubFile);
        return(0);
    }
    if(psubFile->token==tokenEOF) return(0);
    if(psubFile->token!=tokenLBrace) return(0);
    if(psubInfo->isPattern) {
        int gotFirstPattern = 0;

        while(subGetNextToken(psubFile)==tokenSeparater);
        ppatternNode = (patternNode *)ellFirst(&psubInfo->patternList);
        while(1) {
            if(psubFile->token==tokenRBrace) {
                subGetNextToken(psubFile);
                return(psubInfo->macroReplacements);
            }
            if(psubFile->token!=tokenString) {
                subFileErrPrint(psubFile,"Parse error, illegal token");
                exit(-1);
            }
            if(gotFirstPattern) catMacroReplacements(psubInfo,",");
            gotFirstPattern = 1;
            if(ppatternNode) {
                catMacroReplacements(psubInfo,ppatternNode->var);
                catMacroReplacements(psubInfo,"=");
                catMacroReplacements(psubInfo,psubFile->string);
                ppatternNode = (patternNode *)ellNext(&ppatternNode->node);
            } else {
                subFileErrPrint(psubFile,"Warning, too many values given");
            }
            while(subGetNextToken(psubFile)==tokenSeparater);
        }
    } else while(1) {
        switch(subGetNextToken(psubFile)) {
            case tokenRBrace:
                subGetNextToken(psubFile);
                if (!psubInfo->macroReplacements) {
                    catMacroReplacements(psubInfo,"");
                }
                return(psubInfo->macroReplacements);
            case tokenSeparater:
                catMacroReplacements(psubInfo,",");
                break;
            case tokenString:
                catMacroReplacements(psubInfo,psubFile->string);
                break;
            default:
                subFileErrPrint(psubFile,"Parse error, illegal token");
                exit(1);
        }
    }
}

static char *subGetNextLine(subFile *psubFile)
{
    char *pline;

    do {
        pline = fgets(psubFile->inputBuffer,MAX_BUFFER_SIZE,psubFile->fp);
        ++psubFile->lineNum;
    } while(pline && psubFile->inputBuffer[0]=='#');
    if(!pline) {
        psubFile->token = tokenEOF;
        psubFile->inputBuffer[0] = 0;
        psubFile->pnextChar = 0;
        return(0);
    }
    psubFile->pnextChar = &psubFile->inputBuffer[0];
    return(&psubFile->inputBuffer[0]);
}

static void subFileErrPrint(subFile *psubFile,char * message)
{
    fprintf(stderr,"msi: %s\n",message);
    fprintf(stderr,"  in substitution file '%s' at line %d:\n  %s",
        psubFile->substitutionName,
        psubFile->lineNum,psubFile->inputBuffer);
}


static tokenType subGetNextToken(subFile *psubFile)
{
    char        *p;
    char        *pto;

    p = psubFile->pnextChar;
    if(!p) { psubFile->token = tokenEOF; return(tokenEOF);}
    if(*p==0 || *p=='\n' || *p=='#') {
        p = subGetNextLine(psubFile);
        if(!p) { psubFile->token = tokenEOF; return(tokenEOF);}
        else { psubFile->token = tokenSeparater; return(tokenSeparater);}
    }
    while(isspace((int) *p)) p++;
    if(*p=='{') {
        psubFile->token = tokenLBrace;
        psubFile->pnextChar = ++p;
        return(tokenLBrace);
    }
    if(*p=='}') {
        psubFile->token = tokenRBrace;
        psubFile->pnextChar = ++p;
        return(tokenRBrace);
    }
    if(*p==0 || isspace((int) *p) || *p==',') {
        while (isspace((int) *p) || *p==',') p++;
        psubFile->token = tokenSeparater;
        psubFile->pnextChar = p;
        return(tokenSeparater);
    }
    /*now handle quoted strings*/
    if(*p=='"') {
        pto = &psubFile->string[0];
        *pto++ = *p++;
        while(*p!='"') {
            if(*p==0 || *p=='\n') {
                subFileErrPrint(psubFile,"Strings must be on single line\n");
                exit(1);
            }
            /*allow  escape for imbeded quote*/
            if((*p=='\\') && *(p+1)=='"') {
                *pto++ = *p++;
                *pto++ = *p++;
                continue;
            }
            *pto++ = *p++;
        }
        *pto++ = *p++;
        psubFile->pnextChar = p;
        *pto = 0;
        psubFile->token = tokenString;
        return(tokenString);
    }
    /*Now take anything up to next non String token and not space*/
    pto = &psubFile->string[0];
    while(!isspace((int) *p) && (strspn(p,"\",{}")==0)) *pto++ = *p++; 
    *pto = 0;
    psubFile->pnextChar = p;
    psubFile->token = tokenString;
    return(tokenString);
}

static void catMacroReplacements(subInfo *psubInfo,const char *value)
{
    size_t      len = strlen(value);

    if(psubInfo->size <= (psubInfo->curLength + len)) {
        size_t newsize = psubInfo->size + MAX_BUFFER_SIZE;
        char *newbuf;

        if(newsize <= psubInfo->curLength + len)
            newsize = psubInfo->curLength + len + 1;
        newbuf = calloc(1,newsize);
        if(!newbuf) {
            fprintf(stderr,"calloc failed for size %lu\n",
	        (unsigned long) newsize);
            exit(1);
        }
        if(psubInfo->macroReplacements) {
            memcpy(newbuf,psubInfo->macroReplacements,psubInfo->curLength);
            free(psubInfo->macroReplacements);
        }
        psubInfo->size = newsize;
        psubInfo->macroReplacements = newbuf;
    }
    strcat(psubInfo->macroReplacements,value);
    psubInfo->curLength += len;
}
