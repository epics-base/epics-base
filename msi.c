/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/*msi - macro sunstitutions and include */
 
/*
 * Modification Log:
 * -----------------
 * .01  08DEC97       mrk     Original version
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
 
#include <macLib.h>
#include <ellLib.h>

#define MAX_BUFFER_SIZE 4096

#if ((EPICS_VERSION <= 3) && (EPICS_REVISION <= 13))
#define macEnvExpand(x) strdup(x)
#endif

/*Forward references to local routines*/
static void usageExit(void);
static void addMacroReplacements(MAC_HANDLE *macPvt,char *pval);
static void makeSubstitutions(void *inputPvt,void *macPvt,char *templateName);

/*Routines that read the template files */
static void inputConstruct(void **inputPvt);
static void inputDestruct(void *inputPvt);
static void inputAddPath(void *inputPvt, char *pval);
static void inputBegin(void *inputPvt,char *fileName);
static char *inputNextLine(void *inputPvt);
static void inputNewIncludeFile(void *inputPvt,char *name);
static void inputErrPrint(void *inputPvt);

/*Routines that read the substitution file */
static void substituteDestruct(void *substitutePvt);
static void substituteOpen(void **substitutePvt,char *substitutionName);
static int substituteGetNextSet(void *substitutePvt,char **filename);
static char *substituteGetReplacements(void *substitutePvt);

/*Exit status*/
static int exitStatus = 0;


int main(int argc,char **argv)
{
    void *inputPvt;
    MAC_HANDLE *macPvt;
    char *pval;
    int  narg;
    char *substitutionName=0;
    char *templateName=0;
    int  i;

    inputConstruct(&inputPvt);
    macCreateHandle(&macPvt,0);
    macSuppressWarning(macPvt,TRUE);
    while((argc>1) && (argv[1][0] == '-')) {
	narg = (strlen(argv[1])==2) ? 2 : 1;
	pval = (narg==1) ? (argv[1]+2) : argv[2];
	if(strncmp(argv[1],"-I",2)==0) {
	    inputAddPath(inputPvt,pval);
	} else if(strncmp(argv[1],"-o",2)==0) {
	    if(freopen(pval,"w",stdout)==NULL) {
            fprintf(stderr,"Can't open %s for writing: %s\n", pval, strerror(errno));
            exit(1);
        }
	} else if(strncmp(argv[1],"-M",2)==0) {
	    addMacroReplacements(macPvt,pval);
	} else if(strncmp(argv[1],"-S",2)==0) {
	    substitutionName = calloc(strlen(pval)+1,sizeof(char));
	    strcpy(substitutionName,pval);
	} else if(strncmp(argv[1],"-V",2)==0) {
	    macSuppressWarning(macPvt,FALSE);
	    narg = 1; /* no argument for this option */
	} else {
	    usageExit();
	}
	argc -= narg;
	for(i=1; i<argc; i++) argv[i] = argv[i + narg];
    }
    if(argc>2) {
	fprintf(stderr,"too many files\n");
	usageExit();
    }
    if(argc==2) {
        templateName = calloc(strlen(argv[1])+1,sizeof(char));
        strcpy(templateName,argv[1]);
    }
    if(!substitutionName) {
	makeSubstitutions(inputPvt,macPvt,templateName);
    } else {
	void *substitutePvt;
        char *filename = 0;

	substituteOpen(&substitutePvt,substitutionName);
        while(substituteGetNextSet(substitutePvt,&filename)) {
            if(templateName) filename = templateName;
            if(!filename) {
	        fprintf(stderr,"no template file\n");
	        usageExit();
            }
            macPushScope(macPvt);
	    while((pval = substituteGetReplacements(substitutePvt))){
	        addMacroReplacements(macPvt,pval);
	        makeSubstitutions(inputPvt,macPvt,filename);
	    }
            macPopScope(macPvt);
        }
        substituteDestruct(substitutePvt);
    }
    inputDestruct(inputPvt);
    free((void *)templateName);
    free((void *)substitutionName);
    return(exitStatus);
}

void usageExit(void)
{
    fprintf(stderr,"usage: msi -V -opath -Ipath ... -Msub ... -Ssubfile  template\n");
    fprintf(stderr,"    Specifying path will replace the default '.'\n");
    fprintf(stderr,"    stdin is used if template is not given\n");
    exit(1);
}

static void addMacroReplacements(MAC_HANDLE *macPvt,char *pval)
{
    char **pairs;
    long status;

    status = macParseDefns(macPvt,pval,&pairs);
    if(!status) {
        fprintf(stderr,"Error macParseDefns error\n");
        usageExit();
    }
    status = macInstallMacros(macPvt,pairs);
    if(!status) {
        fprintf(stderr,"Error macInstallMacros error\n");
        usageExit();
    }
    free((void *)pairs);
}

#define cmdNumberOf 2
typedef enum {cmdInclude,cmdSubstitute} cmdType;
static char *cmdName[cmdNumberOf] = {"include","substitute"};
static void makeSubstitutions(void *inputPvt,void *macPvt,char *templateName)
{
    char *input;
    static char buffer[MAX_BUFFER_SIZE];
    int  n;

    inputBegin(inputPvt,templateName);
    while((input = inputNextLine(inputPvt))) {
	int	expand=TRUE;
	char	*p;
	char	*command = 0;

	p = input; 
	/*skip whitespace at beginning of line*/
	while(*p && (isspace(*p))) ++p;
	/*Look for i or s */
	if(*p && (*p=='i' || *p=='s')) command = p;
	if(command) {
	    char *pstart;
	    char *pend;
	    char *copy;
	    int  cmdind=-1;
	    int  i;
	    
	    for(i=0; i< cmdNumberOf; i++) {
		if(strstr(command,cmdName[i])) {
		    cmdind = i;
		}
	    }
	    if(cmdind<0) goto endif;
	    p = command + strlen(cmdName[cmdind]);
	    /*skip whitespace after command*/
	    while(*p && (isspace(*p))) ++p;
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
		fprintf(stderr,"Logic Error: makeSubstitutions\n");
	        inputErrPrint(inputPvt);
		exit(1);
	    }
	    free(copy);
	    expand = FALSE;
	}
endif:
	if(expand) {
	    n = macExpandString(macPvt,input,buffer,MAX_BUFFER_SIZE-1);
	    fputs(buffer,stdout);
	    if(n<0) {
	        inputErrPrint(inputPvt);
		exitStatus = 2;
	    }
	}
    }
}

typedef struct inputFile{
    ELLNODE	node;
    char	*filename;
    FILE	*fp;
    int		lineNum;
}inputFile;

typedef struct pathNode {
    ELLNODE	node;
    char	*directory;
} pathNode;

typedef struct inputData {
    ELLLIST 	inputFileList;
    ELLLIST 	pathList;
    char	inputBuffer[MAX_BUFFER_SIZE];
}inputData;

static void inputOpenFile(inputData *pinputData,char *filename);
static void inputCloseFile(inputData *pinputData);
static void inputCloseAllFiles(inputData *pinputData);

static void inputConstruct(void **ppvt)
{
    inputData	*pinputData;

    pinputData = calloc(1,sizeof(inputData));
    ellInit(&pinputData->inputFileList);
    ellInit(&pinputData->pathList);
    *ppvt = pinputData;
}

static void inputDestruct(void *pvt)
{
    inputData	*pinputData = (inputData *)pvt;
    pathNode	*ppathNode;

    inputCloseAllFiles(pinputData);
    while((ppathNode = (pathNode *)ellFirst(&pinputData->pathList))) {
	ellDelete(&pinputData->pathList,&ppathNode->node);
	free((void *)ppathNode->directory);
	free((void *)ppathNode);
    }
    free(pvt);
}

static void inputAddPath(void *pvt, char *path)
{
    inputData	*pinputData = (inputData *)pvt;
    ELLLIST	*ppathList = &pinputData->pathList;
    pathNode	*ppathNode;
    const char	*pcolon;
    const char	*pdir;
    int		len;
    int		emptyName;

    pdir = path;
    /*an empty name at beginning, middle, or end means current directory*/
    while(pdir && *pdir) {
	emptyName = ((*pdir == ':') ? TRUE : FALSE);
	if(emptyName) ++pdir;
	ppathNode = (pathNode *)calloc(1,sizeof(pathNode));
	ellAdd(ppathList,&ppathNode->node);
	if(!emptyName) {
	    pcolon = strchr(pdir,':');
	    len = (pcolon ? (pcolon - pdir) : strlen(pdir));
	    if(len>0)  {
	        ppathNode->directory = (char *)calloc(len+1,sizeof(char));
	        strncpy(ppathNode->directory,pdir,len);
		pdir = pcolon;
		/*unless at end skip past first colon*/
		if(pdir && *(pdir+1)!=0) ++pdir;
	    } else { /*must have been trailing : */
		emptyName=TRUE;
	    }
	}
	if(emptyName) {
	    ppathNode->directory = (char *)calloc(2,sizeof(char));
	    strcpy(ppathNode->directory,".");
	}
    }
    return;
}

static void inputBegin(void *pvt,char *fileName)
{
    inputData	*pinputData = (inputData *)pvt;

    inputCloseAllFiles(pinputData);
    inputOpenFile(pinputData,fileName);
}

static char *inputNextLine(void *pvt)
{
    inputData	*pinputData = (inputData *)pvt;
    inputFile	*pinputFile;
    char	*pline;

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

static void inputNewIncludeFile(void *pvt,char *name)
{
    inputData	*pinputData = (inputData *)pvt;

    inputOpenFile(pinputData,name);
}

static void inputErrPrint(void *pvt)
{
    inputData	*pinputData = (inputData *)pvt;
    inputFile	*pinputFile;

    fprintf(stderr,"input: %s  which is ",pinputData->inputBuffer);
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
	    fprintf(stderr,"  which is included from ");
	} else {
	    fprintf(stderr,"\n");
	}
    }
    fprintf(stderr,"\n");
}

static void inputOpenFile(inputData *pinputData,char *filename)
{
    ELLLIST	*ppathList = &pinputData->pathList;
    pathNode	*ppathNode = 0;
    inputFile	*pinputFile;
    char	*fullname = 0;
    FILE	*fp = 0;

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
	    free((void *)fullname);
	    ppathNode = (pathNode *)ellNext(&ppathNode->node);
	}
    }
    if(!fp) {
	fprintf(stderr,"Could not open %s\n",filename);
        inputErrPrint((void *)pinputData);
        exit(1);
    }
    pinputFile = calloc(1,sizeof(inputFile));
    if(ppathNode) {
	pinputFile->filename = calloc(1,strlen(fullname)+1);
	strcpy(pinputFile->filename,fullname);
	free((void *)fullname);
    } else if(filename) {
        pinputFile->filename = calloc(1,strlen(filename)+1);
        strcpy(pinputFile->filename,filename);
    } else {
	pinputFile->filename = calloc(1,strlen("stdin")+1);
	strcpy(pinputFile->filename,"stdin");
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
	fprintf(stderr,"fclose failed: file %s\n",pinputFile->filename);
    free(pinputFile->filename);
    free(pinputFile);
}

static void inputCloseAllFiles(inputData *pinputData)
{
    inputFile	*pinputFile;

    while((pinputFile=(inputFile *)ellFirst(&pinputData->inputFileList))){
	inputCloseFile(pinputData);
    }
}

/*start of code that handles substitution file*/
typedef enum {
    tokenLBrace,tokenRBrace,tokenSeparater,tokenString,tokenEOF
}tokenType;

typedef struct subFile {
    char	*substitutionName;
    FILE	*fp;
    int		lineNum;
    char	inputBuffer[MAX_BUFFER_SIZE];
    char	*pnextChar;
    tokenType	token;
    char	string[MAX_BUFFER_SIZE];
} subFile;

typedef struct patternNode {
    ELLNODE	node;
    char	*var;
}patternNode;

typedef struct subInfo {
    subFile	*psubFile;
    int		isFile;
    char	*filename;
    int		isPattern;
    ELLLIST	patternList;
    size_t	size;
    size_t	curLength;
    char	*macroReplacements;
}subInfo;

static char *subGetNextLine(subFile *psubFile);
static tokenType subGetNextToken(subFile *psubFile);
static void subFileErrPrint(subFile *psubFile,char * message);
static void freeSubFile(subInfo *psubInfo);
static void freePattern(subInfo *psubInfo);
static void catMacroReplacements(subInfo *psubInfo,const char *value);

void freeSubFile(subInfo *psubInfo)
{
    subFile	*psubFile = psubInfo->psubFile;
    if(psubFile->fp) {
        if(fclose(psubFile->fp))
            fprintf(stderr,"fclose failed on substitution file\n");
    }
    free((void *)psubFile);
    free((void *)psubInfo->filename);
    psubInfo->psubFile = 0;
}

void freePattern(subInfo *psubInfo)
{
    patternNode	*ppatternNode;
    while((ppatternNode = (patternNode *)ellFirst(&psubInfo->patternList))) {
	ellDelete(&psubInfo->patternList,&ppatternNode->node);
	free(ppatternNode->var);
	free(ppatternNode);
    }
    psubInfo->isPattern = FALSE;
}

static void substituteDestruct(void *pvt)
{
    subInfo	*psubInfo = (subInfo *)pvt;

    freePattern(psubInfo);
    free((void *)psubInfo);
    return;
}

static void substituteOpen(void **ppvt,char *substitutionName)
{
    subInfo	*psubInfo;
    subFile	*psubFile;
    FILE	*fp;

    psubInfo = calloc(1,sizeof(subInfo));
    *ppvt = (void *)psubInfo;
    psubFile = calloc(1,sizeof(subFile));
    psubInfo->psubFile = psubFile;
    ellInit(&psubInfo->patternList);
    fp = fopen(substitutionName,"r");
    if(!fp) {
	fprintf(stderr,"Could not open %s\n",substitutionName);
        exit(1);
    }
    psubFile->substitutionName = substitutionName;
    psubFile->fp = fp;
    psubFile->lineNum = 0;
    psubFile->inputBuffer[0] = 0;
    psubFile->pnextChar = &psubFile->inputBuffer[0];
    subGetNextToken(psubFile);
    return;
}

static int substituteGetNextSet(void *pvt,char **filename)
{
    subInfo	*psubInfo = (subInfo *)pvt;
    subFile	*psubFile = psubInfo->psubFile;
    patternNode	*ppatternNode;

    *filename = 0;
    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenEOF) return(FALSE);
    if(psubFile->token==tokenString && strcmp(psubFile->string,"file")==0) {
        psubInfo->isFile = TRUE;
        if(subGetNextToken(psubFile)!=tokenString) {
            subFileErrPrint(psubFile,"Expecting filename");
            exit(1);
        }
        freePattern(psubInfo);
        free((void *)psubInfo->filename);
        if(psubFile->string[0]=='"'&&psubFile->string[strlen(psubFile->string)-1]=='"') {
            psubFile->string[strlen(psubFile->string)-1]='\0';
            psubInfo->filename = macEnvExpand(psubFile->string+1);
        }
        else {
            psubInfo->filename = macEnvExpand(psubFile->string);
        }
        while(subGetNextToken(psubFile)==tokenSeparater);
        if(psubFile->token!=tokenLBrace) {
            subFileErrPrint(psubFile,"Expecting {");
            exit(1);
        }
        subGetNextToken(psubFile);
    }
    *filename = psubInfo->filename;
    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenLBrace) return(TRUE);
    if(psubFile->token==tokenRBrace) return(FALSE);
    if(psubFile->token!=tokenString
    || strcmp(psubFile->string,"pattern")!=0) {
        subFileErrPrint(psubFile,"Expecting pattern");
        exit(1);
    }
    freePattern(psubInfo);
    psubInfo->isPattern = TRUE;
    while(subGetNextToken(psubFile)==tokenSeparater);
    if(psubFile->token!=tokenLBrace) {
	subFileErrPrint(psubFile,"Expecting {");
	exit(1);
    }
    while(TRUE) {
        while(subGetNextToken(psubFile)==tokenSeparater);
	if(psubFile->token!=tokenString) break;
	ppatternNode = calloc(1,sizeof(patternNode));
	ellAdd(&psubInfo->patternList,&ppatternNode->node);
	ppatternNode->var = calloc(strlen(psubFile->string)+1,sizeof(char));
	strcpy(ppatternNode->var,psubFile->string);
    }
    if(psubFile->token!=tokenRBrace) {
	subFileErrPrint(psubFile,"Expecting }");
	exit(1);
    }
    subGetNextToken(psubFile);
    return(TRUE);
}

static char *substituteGetReplacements(void *pvt)
{
    subInfo	*psubInfo = (subInfo *)pvt;
    subFile	*psubFile = psubInfo->psubFile;
    patternNode *ppatternNode;

    if(psubInfo->macroReplacements) psubInfo->macroReplacements[0] = 0;
    psubInfo->curLength = 0;
    while(psubFile->token==tokenSeparater) subGetNextToken(psubFile);
    if(psubFile->token==tokenRBrace && psubInfo->isFile) {
        psubInfo->isFile = FALSE;
        free((void *)psubInfo->filename);
        psubInfo->filename = 0;
        freePattern(psubInfo);
        subGetNextToken(psubFile);
        return(0);
    }
    if(psubFile->token==tokenEOF) return(0);
    if(psubFile->token!=tokenLBrace) return(0);
    if(psubInfo->isPattern) {
	int gotFirstPattern = FALSE;

        while(subGetNextToken(psubFile)==tokenSeparater);
	ppatternNode = (patternNode *)ellFirst(&psubInfo->patternList);
	while(TRUE) {
            if(psubFile->token==tokenRBrace) {
                if(ppatternNode) 
                    subFileErrPrint(psubFile,"less values than patterns");
                subGetNextToken(psubFile);
                return(psubInfo->macroReplacements);
            }
            if(psubFile->token!=tokenString) {
                subFileErrPrint(psubFile,"Illegal token");
                exit(-1);
            }
	    if(gotFirstPattern) catMacroReplacements(psubInfo,",");
	    gotFirstPattern = TRUE;
            if(ppatternNode) {
	        catMacroReplacements(psubInfo,ppatternNode->var);
	        catMacroReplacements(psubInfo,"=");
	        catMacroReplacements(psubInfo,psubFile->string);
                ppatternNode = (patternNode *)ellNext(&ppatternNode->node);
            } else {
                subFileErrPrint(psubFile,"more values than patterns");
            }
            while(subGetNextToken(psubFile)==tokenSeparater);
	}
    } else while(TRUE) {
        switch(subGetNextToken(psubFile)) {
	    case tokenRBrace:
                subGetNextToken(psubFile);
                return(psubInfo->macroReplacements);
	    case tokenSeparater:
		catMacroReplacements(psubInfo,",");
		break;
	    case tokenString:
		catMacroReplacements(psubInfo,psubFile->string);
		break;
	    default:
		subFileErrPrint(psubFile,"Illegal token");
		exit(1);
	}
    }
}

static char *subGetNextLine(subFile *psubFile)
{
    char *pline;

    pline = fgets(psubFile->inputBuffer,MAX_BUFFER_SIZE,psubFile->fp);
    ++psubFile->lineNum;
    while(pline && psubFile->inputBuffer[0]=='#') {
	pline = fgets(psubFile->inputBuffer,MAX_BUFFER_SIZE,psubFile->fp);
        ++psubFile->lineNum;
    }
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
    fprintf(stderr,"substitution file %s line %d: %s",
	psubFile->substitutionName,
	psubFile->lineNum,psubFile->inputBuffer);
    fprintf(stderr,"%s\n",message);
}


static tokenType subGetNextToken(subFile *psubFile)
{
    char	*p;
    char	*pto;

    p = psubFile->pnextChar;
    if(!p) { psubFile->token = tokenEOF; return(tokenEOF);}
    if(*p==0 || *p=='\n' || *p=='#') {
	p = subGetNextLine(psubFile);
	if(!p) { psubFile->token = tokenEOF; return(tokenEOF);}
        else { psubFile->token = tokenSeparater; return(tokenSeparater);}
    }
    while(isspace(*p)) p++;
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
    if(*p==0 || isspace(*p) || *p==',') {
	while(isspace(*p) || *p==',') p++;
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
    while(!isspace(*p) && (strspn(p,"\",{}")==0)) *pto++ = *p++; 
    *pto = 0;
    psubFile->pnextChar = p;
    psubFile->token = tokenString;
    return(tokenString);
}

static void catMacroReplacements(subInfo *psubInfo,const char *value)
{
    size_t	len = strlen(value);

    if(psubInfo->size <= (psubInfo->curLength + len)) {
        size_t newsize = psubInfo->size + MAX_BUFFER_SIZE;
        char *newbuf;

        if(newsize <= psubInfo->curLength + len)
            newsize = psubInfo->curLength + len + 1;
        newbuf = calloc(1,newsize);
        if(!newbuf) {
            fprintf(stderr,"calloc failed for size %d\n",newsize);
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
