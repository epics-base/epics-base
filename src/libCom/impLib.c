/*impLib.c*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/
 
/* Modification Log:
 * -----------------
 * 01  17sep96  mrk     Original
 */
#include <stdio.h>
#ifdef vxWorks
#include <vxWorks.h>
#include <taskLib.h>
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ellLib.h>
#include <epicsPrint.h>
#include <impLib.h>

/*private declarations*/
typedef struct pathNode {
	ELLNODE	node;
	char	*directory;
}pathNode;

typedef struct inputFile{
	ELLNODE		node;
	char		*path;
	char		*filename;
	FILE		*fp;
	int		line_num;
}inputFile;

typedef struct impPvt{
	char		*includeToken;
	char		*pathToken;
	char		*addPathToken;
	char		*envName;
	MAC_HANDLE	*macHandle;
	char		*buffer; /*For use if macros are being expanded*/
	int		bufferSize;
	ELLLIST		pathList;
	ELLLIST		inputFileList;
}impPvt;

/*private routines */
static void *impCalloc(size_t nobj,size_t size);
static void freePath(impPvt *handle);
static void freeInputFileList(impPvt *handle);
static char *openFile(impPvt *handle,const char *filename,FILE **fp);
static void includeNew(impPvt *handle,char *filename);
static char *getParameter(char *pfirst,char *token);

/*Beginning of Private routines*/
static void *impCalloc(size_t nobj,size_t size)
{
    void *p;
 
    p=calloc(nobj,size);
    if(p) return(p);
    printf("impCalloc: Can't allocate memory\n");
#ifdef vxWorks
    taskSuspend(0);
#else
    abort();
#endif
    return(NULL);
}

static void freePath(impPvt *handle)
{
    pathNode	*ppathNode;

    if(!handle) return;
    while(ppathNode = (pathNode *)ellFirst(&handle->pathList)) {
	ellDelete(&handle->pathList,&ppathNode->node);
	free((void *)ppathNode->directory);
	free((void *)ppathNode);
    }
    return;
}

static void freeInputFileList(impPvt *handle)
{
    inputFile *pinputFileNow;

    while(pinputFileNow=(inputFile *)ellFirst(&handle->inputFileList)) {
	if(pinputFileNow->fp && fclose(pinputFileNow->fp)) 
	    epicsPrintf("Error Closing file %s",pinputFileNow->filename);
	free((void *)pinputFileNow->filename);
	ellDelete(&handle->inputFileList,(ELLNODE *)pinputFileNow);
	free((void *)pinputFileNow);
    }
}

static char *openFile(impPvt *handle,const char *filename,FILE **fp)
{
    ELLLIST	*ppathList = &handle->pathList;
    pathNode	*ppathNode;
    char	*fullfilename;

    if(!filename) return(0);
    if(!ppathList || (ellCount(ppathList)==0) || strchr(filename,'/')){
	*fp = fopen(filename,"r");
	return(0);
    }
    ppathNode = (pathNode *)ellFirst(ppathList);
    while(ppathNode) {
	fullfilename = impCalloc(strlen(filename)+strlen(ppathNode->directory)
	    +2,sizeof(char));
	strcpy(fullfilename,ppathNode->directory);
	strcat(fullfilename,"/");
	strcat(fullfilename,filename);
	*fp = fopen(fullfilename,"r");
	free((void *)fullfilename);
	if(*fp) return(ppathNode->directory);
	ppathNode = (pathNode *)ellNext(&ppathNode->node);
    }
    epicsPrintf("Error opening file %s\n",filename);
    return(0);
}

static void includeNew(impPvt *handle,char *filename)
{
    inputFile	*pinputFile;
    FILE	*fp = NULL;

    pinputFile = impCalloc(1,sizeof(inputFile));
    pinputFile->path = openFile(handle,filename,&fp);
    if(!fp) {
	errPrintf(0,__FILE__, __LINE__,
		"includeNew opening file %s",filename);
	free((void *)pinputFile);
	return;
    }
    pinputFile->filename = impCalloc(strlen(filename)+1,sizeof(char));
    strcpy(pinputFile->filename,filename);
    pinputFile->fp = fp;
    ellAdd(&handle->inputFileList,&pinputFile->node);
}

static char *getParameter(char *pfirst,char *token)
{
    char	*plast;
    size_t	nskip;
    char	*prtn;
    size_t	size;

    /*Skip blanks and tabs*/
    pfirst += strlen(token);
    nskip = strspn(pfirst," \t");
    pfirst += nskip;
    if(!*pfirst) return(NULL);
    if(*pfirst=='"') {
	++pfirst;
	plast = strchr(pfirst,'"');
	if(!plast) return(NULL);
	size = plast-pfirst+1;
    } else {
	size = strcspn(pfirst," \t") + 1;
    }
    if(size<=1) return(NULL);
    prtn = impCalloc(size,sizeof(char));
    strncpy(prtn,pfirst,size-1);
    return(prtn);
}

int impInit(IMP_HANDLE *phandle,int bufferSize)
{
    impPvt *handle;

    handle = impCalloc(1,sizeof(impPvt));
    handle->buffer = impCalloc(bufferSize,sizeof(char));
    handle->bufferSize = bufferSize;
    ellInit(&handle->pathList);
    ellInit(&handle->inputFileList);
    impSetIncludeToken((IMP_HANDLE *)handle,"include");
    *phandle = (IMP_HANDLE)handle;
    return(impSuccess);
}

void impFree(IMP_HANDLE imp)
{
    impPvt *handle = (impPvt *)imp;
    if(!handle) return;

    freePath(handle);
    freeInputFileList(handle);
    free((void *)handle->includeToken);
    free((void *)handle->pathToken);
    free((void *)handle->addPathToken);
    free((void *)handle->envName);
    free((void *)handle->buffer);
    if(handle->macHandle) macDeleteHandle(handle->macHandle);
    free((void *)handle);
}

int impSetIncludeToken(IMP_HANDLE imp,char *includeToken)
{
    impPvt *handle = (impPvt *)imp;

    if(!handle) return(impFailure);
    free((void *)handle->includeToken);
    handle->includeToken = NULL;
    if(!includeToken) return(impSuccess);
    handle->includeToken = impCalloc(strlen(includeToken)+1,sizeof(char));
    strcpy(handle->includeToken,includeToken);
    return(impSuccess);
}

int impSetEnvName(IMP_HANDLE imp,char *envName)
{
    impPvt *handle = (impPvt *)imp;

    if(!handle) return(impFailure);
    free((void *)handle->envName);
    handle->envName = NULL;
    if(!envName) return(impSuccess);
    handle->envName = impCalloc(strlen(envName)+1,sizeof(char));
    strcpy(handle->envName,envName);
    return(impSuccess);
}

MAC_HANDLE *impGetMacHandle(IMP_HANDLE imp)
{
    impPvt *handle = (impPvt *)imp;

    if(!handle) return(NULL);
    return(handle->macHandle);
}

int impSetMacHandle(IMP_HANDLE imp,MAC_HANDLE *mac)
{
    impPvt *handle = (impPvt *)imp;

    if(!handle) return(impFailure);
    if(handle->macHandle) return(impFailure);
    handle->macHandle = mac;
    return(impSuccess);
}

void impMacFree(IMP_HANDLE imp)
{
    impPvt *handle = (impPvt *)imp;

    if(!handle) return;
    if(handle->macHandle) {
	macDeleteHandle(handle->macHandle);
	handle->macHandle = NULL;
    }
    return;
}

int impMacAddSubstitutions(IMP_HANDLE imp,const char *substitutions)
{
    impPvt	*handle = (impPvt *)imp;
    char	**pairs;

    if(!handle) return(impFailure);
    if(!substitutions || strlen(substitutions)==0) return(impSuccess);
    if(!handle->macHandle && macCreateHandle(&handle->macHandle,NULL)) {
	epicsPrintf("macCreateHandle error\n");
	return(impFailure);
    }
    macParseDefns(handle->macHandle,(char *)substitutions,&pairs);
    if(pairs) {
	macInstallMacros(handle->macHandle,pairs);
	free((void *)pairs);
    }
    return(impSuccess);
}

int impMacAddNameValuePairs(IMP_HANDLE imp,const char *pairs[])
{
    impPvt	*handle = (impPvt *)imp;

    if(!handle) return(impFailure);
    if(!handle->macHandle && macCreateHandle(&handle->macHandle,NULL)) {
	epicsPrintf("macCreateHandle error\n");
	return(impFailure);
    }
    macInstallMacros(handle->macHandle,(char **)pairs);
    return(impSuccess);
}

int impSetPath(IMP_HANDLE imp,const char *path)
{
    impPvt *handle = (impPvt *)imp;

    if(!handle) return(impFailure);
    freePath(handle);
    return(impAddPath(handle,path));
}

int impAddPath(IMP_HANDLE imp,const char *path)
{
    impPvt *handle = (impPvt *)imp;

    ELLLIST	*ppathList;
    pathNode	*ppathNode;
    const char	*pcolon;
    const char	*pdir;
    int		len;

    if(!handle) return(impFailure);
    ppathList = &handle->pathList;
    pdir = path;
    while(pdir) {
	if(*pdir == ':') {
	    pdir++;
	    continue;
	}
	ppathNode = (pathNode *)impCalloc(1,sizeof(pathNode));
	ellAdd(ppathList,&ppathNode->node);
	pcolon = strchr(pdir,':');
	len = (pcolon ? (pcolon - pdir) : strlen(pdir));
	ppathNode->directory = (char *)impCalloc(1,len + 1);
	strncpy(ppathNode->directory,pdir,len);
	pdir = (pcolon ? (pcolon+1) : 0);
    }
    return(impSuccess);
}

int impPrintInclude(IMP_HANDLE imp,FILE *fp)
{
    impPvt      *handle = (impPvt *)imp;
    inputFile	*pinputFile;

    if(!handle) return(impFailure);
    pinputFile = (inputFile *)ellLast(&handle->inputFileList);
    if(!pinputFile || !handle->buffer) return(impFailure);
    fprintf(fp,"input line: %s",handle->buffer);
    while(pinputFile) {
	if(pinputFile->filename) {
	    fprintf(fp,"   in:");
	    if(pinputFile->path)
	        fprintf(fp," path \"%s\" ",pinputFile->path);
	    fprintf(fp," file %s",pinputFile->filename);
	} else {
	    fprintf(fp,"stdin:");
	    if(pinputFile->path)
	        fprintf(fp," path \"%s\" ",pinputFile->path);
	}
	fprintf(fp," line %d\n",pinputFile->line_num);
	pinputFile = (inputFile *)ellPrevious(&pinputFile->node);
    }
    fprintf(fp,"\n");
    return(impSuccess);
}

int impDumpPath(IMP_HANDLE imp)
{
    impPvt	*handle = (impPvt *)imp;
    ELLLIST	*ppathList;
    pathNode	*ppathNode;

    if(!handle) return(impFailure);
    ppathList = &handle->pathList;
    if(!ppathList || !(ppathNode = (pathNode *)ellFirst(ppathList))) {
	printf("no path defined\n");
	return(impSuccess);
    }
    while(ppathNode) {
	printf("%s",ppathNode->directory);
	ppathNode = (pathNode *)ellNext(&ppathNode->node);
	if(ppathNode) printf(":");
    }
    printf("\n");
    return(impSuccess);
}

int impOpenFile(IMP_HANDLE imp,char *filename)
{
    impPvt	*handle = (impPvt *)imp;
    inputFile	*pinputFile;
    FILE	*fp;
    char	*penv;

    if(!handle) return(impFailure);
    freeInputFileList(handle);
    if((ellCount(&handle->pathList)==0) && handle->envName) {
	penv = getenv(handle->envName);
	if(penv)impSetPath(imp,penv);
    }
    pinputFile = impCalloc(1,sizeof(inputFile));
    if(filename) {
	pinputFile->filename = impCalloc(strlen(filename)+1,sizeof(char));
	strcpy(pinputFile->filename,filename);
	pinputFile->path = openFile(handle,filename,&fp);
	if(!fp) {
	    epicsPrintf("Error opening file %s\n",filename);
	    return(impFailure);
	}
    } else {
	fp = stdin;
    }
    pinputFile->fp = fp;
    pinputFile->line_num = 0;
    handle->buffer[0] = '\0';
    ellAdd(&handle->inputFileList,&pinputFile->node);
    return(impSuccess);
}

int impCloseFile(IMP_HANDLE imp)
{
    impPvt	*handle = (impPvt *)imp;

    if(!handle) return(impFailure);
    if(ellCount(&handle->inputFileList) !=0) freeInputFileList(handle);
    return(impSuccess);
}

int impGetLine(IMP_HANDLE imp,char *buffer,int bufferSize)
{
    impPvt	*handle = (impPvt *)imp;
    char	*fgetsRtn;
    int		n;
    char	*pfirst;
    size_t	nskip;
    char	*parameter;
    inputFile	*pinputFileNow;
    
    if(!handle) return(impFailure);
    while(TRUE) { /*until we get some input*/
	MAC_HANDLE *macHandle = handle->macHandle;

	pinputFileNow = (inputFile *)ellLast(&handle->inputFileList);
	if(!pinputFileNow) return(0);
	if(macHandle)
	    fgetsRtn=fgets(handle->buffer,handle->bufferSize,pinputFileNow->fp);
	else
	    fgetsRtn=fgets(buffer,bufferSize,pinputFileNow->fp);
	if(!fgetsRtn) {
	    if(fclose(pinputFileNow->fp)) 
		errPrintf(0,__FILE__, __LINE__,
			"Closing file %s",pinputFileNow->filename);
	    free((void *)pinputFileNow->filename);
	    ellDelete(&handle->inputFileList,(ELLNODE *)pinputFileNow);
	    free((void *)pinputFileNow);
	    continue;
	}
	pinputFileNow->line_num++;
	if(macHandle) {
	    n = macExpandString(macHandle,handle->buffer,buffer,bufferSize-1);
	    if(n<0) {
		epicsPrintf("macExpandString failed for %s",handle->buffer);
		impPrintInclude(imp,stderr);
		continue;
	    }
	}
	nskip = strspn(fgetsRtn," \t");
	pfirst = fgetsRtn + nskip;
	if(handle->includeToken
	&& pfirst[0]==handle->includeToken[0]
	&& strstr(pfirst,handle->includeToken)){
	    parameter = getParameter(pfirst,handle->includeToken);
	    if(!parameter) continue;
	    includeNew(imp,parameter);
	    free((void *)parameter);
	    continue;
	}
        return(strlen(buffer));
    }
}
