/* share/src/libCom $Id$
 * errSymLib.c
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
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
 * Modification Log: errSymLib.c
 * -----------------
 * .01  mm-dd-yy        rcz     See below for Creation/Merge
 ****	Merged Modification Logs:
 * Modification Log: errMessage.c
 * -----------------
 * .01  10-08-91	mrk	Allow logMsg or printF
 * .02  03-10-93	joh	expanded errMessage() to accept the
 *				same format string and variable
 *				length arguments as printf()	
 *				and renamed it errPrintf()
 * .03  03-11-93        joh     added __FILE__ and __LINE__ to errPrintf()
 *				created macro errMessage() that calls
 *				errPrintf with __FILE__ and __LINE__
 *				as arguments
 * errMessage.c - Handle error messages 
 ***************************************************************************
 * This must ultimately be replaced by a facility that allows remote
 * nodes access to the error messages. A message handling communication
 * task should be written that allows multiple remote nodes to request
 * notification of all error messages.
 * For now lets just print messages and last errno via logMsg or printf
 ***************************************************************************
 * Modification Log: errPrint.c
 * -----------------
 * .01  10-08-91	mrk	Allow logMsg or printf
 * .02  04-29-93	joh 	extra arg for errPrint()	
 * .03  04-29-93	joh 	errPrint() became errPrintStatus()	
 * .04	05-06-93	joh	errPrintStatus() get var args 
 *				for vprintf()
 * Modification Log: errSymLib.c
 * -----------------
 * .01  09-04-93        rcz     Merged errMessage.c, errPrint.c, errSymFind.c
 *			rcz	into one file (errSymLib.c) and changed method
 *			rcz	of errSymTable lookup.
 */

/*
 * TODO
 * 1. Put everything in  errSymLib.c - DONE
 *	get rid of error.h - DONE
 *	function prototypes in errMdef.h - (DONE ???)
 * 2. Nick to fix calink.h error comments.
 * 3. Fix trailing blanks on S_... -- DONE
 * 4. In iocinit - make a call to errSymBld to make it initialize.
 *
 */

/******************************
 * FUNCTIONS
 * errhash - NEW
 * errPrintf - N/C
 * errPrintStatus  - N/C
 * verrPrintStatus - N/C
 * errSymBld - complete rework
 * errSymbolAdd  - NEW
 * errSymbolFind - NEW
 * errSymFind 
 * UnixSymFind - N/C
 * ModSymFind
 * errSymDump    - NEW
 * errSymFindTst - NEW
 * main - in errMtst.c
 *		Program errMtst calls:
 *			errSymBld
 *			errSymDump
 *			errSymFindTst
 *
 *	errSymTest - in errMtst.c - not implemented yet (not needed ??? )
 *			errSymTest(501, 0, 100)
 *      		errSymTest(int modnum, int begErrNum, int endErrNum)
 ***********************************/

#include <ellLib.h>
#include <dbDefs.h>
#include <errMdef.h>
#include <stdio.h>

#ifdef vxWorks
#include <vxWorks.h>
#include <taskLib.h>
#include <logLib.h>
#include <types.h>
#include <symLib.h>

extern SYMTAB_ID  statSymTbl;

extern int errToLogMsg;
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
extern int errno;
extern int      sys_nerr;
extern char    *sys_errlist[];
#endif


static ELLLIST errnumlist;
static ERRNUMNODE hashtable[NHASH];
static int initialized = FALSE;
extern ERRSYMTAB_ID errSymTbl;


#ifdef vxWorks
int errToLogMsg = FALSE;
#endif

/*Declare storage for errVerbose( defined in errMdef.h)*/
int errVerbose=0;


/****************************************************************
 * HASH
 * returns the hash index of errNum
****************************************************************/
#ifdef __STDC__
static
unsigned short errhash(long errNum)
#else
static
unsigned short errhash(errNum)
long errNum;
#endif /* __STDC__ */
{
unsigned short modnum;
unsigned short errnum;

	modnum = errNum >> 16;
	errnum = errNum & 0xffff;
	return((((modnum - 500) * 10) + errnum) % NHASH);
}

/****************************************************************
 * ERRMESSAGE - now a macro to call errPrintf
 * ERRPRINTF  - print an error symbol message
 ***************************************************************/
#ifdef __STDC__
void errPrintf(long status, char *pFileName, int lineno, char *pformat, ...)
#else
void errPrintf(va_alist)
va_dcl
#endif
{
    va_list 	   pvar;
    static char    *pReformat;
    static int	   reformatSize;
    static char	   pAdd[] = {'\n', '\0'};
#ifndef __STDC__
    long     	   status;
    char           *pformat;
    char	   *pFileName;
    int		   lineno;
#endif
#ifdef vxWorks
    int             id;
    static int      saveid = -1;
    char           *pname;
#endif

#ifdef __STDC__
    va_start(pvar, pformat);
#else
    va_start(pvar);
    status = va_arg(pvar, long);
    pFileName = va_arg(pvar, char *);
    lineno = va_arg(pvar, int);
    pformat = va_arg(pvar, char *);
#endif

#ifdef vxWorks
    if(!errToLogMsg) {
	id = taskIdSelf();
	if (saveid != id) {
	    saveid = id;
	    pname = taskName(id);
	    printf("taskid=%x taskname=%s ", id, pname);
	}
    }
#endif

    if(pFileName && errVerbose){
#ifdef vxWorks
      	if(errToLogMsg) {
        	logMsg("filename=\"%s\" line number=%d\n", pFileName, lineno);
      	}
      	else{
        	printf("filename=\"%s\" line number=%d\n", pFileName, lineno);
      	}
#else
      	printf("filename=\"%s\" line number=%d\n", pFileName, lineno);
#endif
    }

    if (pformat != NULL) {
	int		size;

	size = strlen(pformat)+NELEMENTS(pAdd);
	
	if(reformatSize < size){
		/*
		 * use a reasonable size string
		 */
		size = max(0xff, size);
		if(pReformat){
			free(pReformat);
		}

		pReformat = (char *) malloc(size);
		if(pReformat){
			reformatSize = size;
		}
		else{
#ifdef vxWorks
		    logMsg("%s: calloc error\n", __FILE__);
#else
		    printf("%s: calloc error\n", __FILE__);
#endif
		    return;
		}
	}
	strcpy(pReformat, pformat);
	strcat(pReformat, pAdd);

    }

    verrPrintStatus(status, pReformat, pvar);

    return;
}

/****************************************************************
 * ERRSYMBLD
 *
 * Create the normal ell LIST of sorted error messages nodes
 * Followed by linked hash lists - that thread together those
 * ell nodes that have a common hash number.
 *
 * The hash will get you to the top of a singly linked hash list
 * A linear search is then performed to find the place to
 * plant/retrieve the next hashed link/message. 
 ***************************************************************/
int errSymBld()
{
    ERRSYMBOL      *errArray = errSymTbl->symbols;
    ERRSYMBOL      *perrArray;
    ELLLIST        *perrnumlist = &errnumlist;
    ERRNUMNODE     *perrNumNode = NULL;
    ERRNUMNODE     *pNextNode = NULL;
    ERRNUMNODE     *pLastNode = NULL;
    ERRNUMNODE     *phashtable = hashtable;
    int             i;
    int             modnum;
    unsigned short  hashInd;

    for (i = 0; i < errSymTbl->nsymbols; i++, errArray++) {
	modnum = errArray->errNum >> 16;
	if (modnum < 501) {
	    printf("errSymBld: ERROR - Module number in errSymTbl < 501\n");
	    return (-1);
	}
	if ((errSymbolAdd(errArray->errNum, errArray->name))  <0 ) {
	    printf("errSymBld: ERROR - errSymbolAdd() failed \n");
	    return (-1);
	}
    }
    perrNumNode = (ERRNUMNODE *) ellFirst(perrnumlist);
    while (perrNumNode) {
	/* hash each perrNumNode->errNum */
	hashInd = errhash(perrNumNode->errNum);

	/* if the hash table entry is empty - fill it */
	if ( phashtable[hashInd].hashnode == NULL ) {
		phashtable[hashInd].hashnode = perrNumNode;
		perrNumNode = (ERRNUMNODE *) ellNext((ELLNODE *) perrNumNode);
		continue; /* and go to next entry */
	}
	/* else search for the end of this hashed link list */
	pNextNode = phashtable[hashInd].hashnode;
	/* search for last node (NULL) of hashnode linked list */
	while (pNextNode) {
	    pLastNode = pNextNode;
	    pNextNode = pNextNode->hashnode;
	}
	pLastNode->hashnode = perrNumNode;
	perrNumNode = (ERRNUMNODE *) ellNext((ELLNODE *) perrNumNode);
    }
    initialized = TRUE;
    return(0);
}

/****************************************************************
 * ERRPRINTSTATUS
 ***************************************************************/
#ifdef __STDC__
int errPrintStatus(long status, char *pFormat, ...)
#else
int errPrintStatus(va_alist)
va_dcl
#endif
{
	va_list        pvar;
#ifndef __STDC__
	long		status;
	char		*pFormat;
#endif

#ifdef __STDC__
	va_start(pvar, pFormat);
#else
	va_start(pvar);
	status = va_arg(pvar, long);
	pFormat = va_arg(pvar, char *);
#endif

	return verrPrintStatus(status, pFormat, pvar);
}

/****************************************************************
 * VERRPRINTSTATUS
 ***************************************************************/
#ifdef __STDC__
int verrPrintStatus(long status, char *pFormatString, va_list pvar)
#else
int verrPrintStatus(status, pFormatString, pvar)
long 	status;
char	*pFormatString;
va_list	pvar;
#endif
{
	static char	ctxToLarge[] = "** Context String Overflow **";
	char 		name[256];
	long 		value;
	int 		rtnval;
	int		namelen;
	int		formatlen;


	name[0] = '\0';
	value = (status? status : MYERRNO );
	if (!value)
	    return(0);

	if(status == -1){
		rtnval = 0;
	}
	else if(status < 0){
		rtnval = status;
	}
	else{
		rtnval = errSymFind(value,name);
	}

	if(rtnval) {
		sprintf(name, 
			"Error status =0x%8lx not in symbol table", 
			status);
	}

	if(pFormatString){
		namelen = strlen(name);
		formatlen = strlen(pFormatString);
		strcat(name," ");
		if(sizeof(name)-namelen-1 > formatlen){
			strcat(name, pFormatString);
		}
		else if(sizeof(name)-namelen-1 > sizeof(ctxToLarge)){
			strcat(name, ctxToLarge);
		}
		else{
			fprintf(stderr,ctxToLarge);
		}
	}

#ifdef vxWorks
	if(errToLogMsg){
		int     i;
		int     logMsgArgs[6];

		for(i=0; i< NELEMENTS(logMsgArgs); i++){
			logMsgArgs[i] = va_arg(pvar, int);
		}

		logMsg(
			name,
			logMsgArgs[0],
			logMsgArgs[1],
			logMsgArgs[2],
			logMsgArgs[3],
			logMsgArgs[4],
			logMsgArgs[5]);
	}
	else{
		vprintf(name, pvar);
	}
#else
	vprintf(name, pvar);
#endif

	return rtnval;
}

/****************************************************************
 * ERRSYMBOLFIND
 ***************************************************************/
#ifdef __STDC__
static ERRNUMNODE *errSymbolFind (long errNum)
#else
static ERRNUMNODE *errSymbolFind(errNum)
long errNum;
#endif /* __STDC__ */
{
    unsigned short  hashInd;
    ERRNUMNODE     *pNextNode = NULL;
    ERRNUMNODE     *phashtable = hashtable;

    hashInd = errhash(errNum);
    if ((pNextNode = phashtable[hashInd].hashnode) == NULL) {
	return (NULL);
    }
    pNextNode = phashtable[hashInd].hashnode;
    /* search for match of errNum */
    while (pNextNode) {
        if (pNextNode->errNum == errNum)
    	    return(pNextNode);
        pNextNode = pNextNode->hashnode;
    }
    return (NULL);
}

/****************************************************************
 * ERRSYMBOLADD
 * adds symbols to the master errnumlist in sorted errNum order 
 ***************************************************************/
#ifdef __STDC__
int errSymbolAdd (long errNum,char *name)
#else
int errSymbolAdd (errNum,name)
long errNum;
char *name;
#endif /* __STDC__ */
{
    ELLLIST        *perrnumlist = &errnumlist;
    ERRNUMNODE     *pNew;
    ERRNUMNODE     *perrNumNode;
    ERRNUMNODE     *prevNumNode;
    int             insert;
    int             len;

    pNew = (ERRNUMNODE*)dbCalloc(1, sizeof(ERRNUMNODE));
    pNew->errNum = errNum;
    len = strlen(name);
    pNew->message = (char *)dbCalloc(1, len+1);
    memcpy(pNew->message, name, len);
    /* find the place to put the new node */
    perrNumNode = (ERRNUMNODE *) ellFirst(perrnumlist);
    insert = 0;
    while (perrNumNode) {
	if (perrNumNode->errNum >= errNum) { /* too far */
		insert = 1;
		break;
	}
	perrNumNode = (ERRNUMNODE *) ellNext((ELLNODE *) perrNumNode);
    }
    if (insert) {
	prevNumNode = (ERRNUMNODE*)ellPrevious(perrNumNode);
	ellInsert(perrnumlist,(ELLNODE*)prevNumNode,(ELLNODE*)pNew);
    } else {
	ellAdd(perrnumlist,(ELLNODE*)pNew);
    }
    return(0);
}

/****************************************************************
 * UNIXSYMFIND
 ***************************************************************/
#ifndef vxWorks
#ifdef __STDC__
int UnixSymFind(long status, char *pname, long *pvalue);
#else
int UnixSymFind(status, pname, pvalue)
    long            status;
    char           *pname;
    long           *pvalue;
#endif /* __STDC__ */
{
    if (status >= sys_nerr || status < 1) {
	*pvalue = -1;
	return;
    }
    strcpy(pname, sys_errlist[status]);
    *pvalue = status;
    return;
}
#endif

/****************************************************************
 * MODSYMFIND
 ***************************************************************/
#ifdef __STDC__
int ModSymFind(long status, char *pname, long *pvalue)
#else
int ModSymFind(status, pname, pvalue)
    long            status;
    char           *pname;
    long           *pvalue;
#endif /* __STDC__ */
{
    unsigned short  modNum;
    ERRNUMNODE *pRetNode;

    modNum = (status >> 16);
    if (modNum < 501) {
	*pvalue = -1;
	return;
    }
    if ((pRetNode = errSymbolFind(status)) == NULL) {
	*pvalue = -1;
	return;
    }
    strcpy(pname, pRetNode->message);
    *pvalue = status;
    return;
}

/****************************************************************
 * ERRSYMFIND
 ***************************************************************/
#ifdef __STDC__
int errSymFind(long status, char *name)
#else
/* errSymFind - Locate error symbol */
int errSymFind(status, name)
    long            status;
    char           *name;
#endif /* __STDC__ */
{
    long            value;
#ifdef vxWorks
    unsigned char   type;
#endif
    unsigned short  modnum;

    if (!initialized) {
	printf("\nerrSymFind: Aborting because errSymBld didn't initialize\n");
	return (-1);
    }

    modnum = (status >> 16);
    if (modnum <= 500)
#ifdef vxWorks
	symFindByValue((SYMTAB_ID)statSymTbl, status, name,(int*) &value, (SYM_TYPE*)&type);
#else
	UnixSymFind(status, name, &value);
#endif
    else
	ModSymFind(status, name, &value);
    if (value != status)
	return (-1);
    else
	return (0);
}

/****************************************************************
 * ERRSYMFINDTST
 ***************************************************************/
void errSymFindTst()
{
    ERRNUMNODE     *phashtable = hashtable;
    ERRNUMNODE     *pNextNode;
    char	   message[128];
    unsigned short modnum;
    unsigned short errnum;
    int            i;

    for ( i=0; i<NHASH; i++) {
        if ((pNextNode = phashtable[i].hashnode) == NULL) {
            continue;
        }
        /* search down the linked list for each errNum */
        while (pNextNode)
	{
            modnum = (pNextNode->errNum >> 16);
            errnum = pNextNode->errNum & 0xffff;
            if((errSymFind (pNextNode->errNum, message)) <0 )
            {
                printf("errSymFindTst: errSymFind FAILED - modnum=%d errnum=%d\n"
			 ,modnum ,errnum);
            }
            printf("hash_no=%d mod=%d num=%d errmess=\"%s\"\n"
			, i, modnum, errnum, message);
            pNextNode = pNextNode->hashnode;
        }
    }
return;
}

/****************************************************************
 * errSymDump
 ***************************************************************/
#ifdef __STDC__
void errSymDump()
#else
void errSymDump()
#endif /* __STDC__ */
{
ERRNUMNODE     *phashtable;
ERRNUMNODE     *pNextNode;
ERRNUMNODE     *pLastNode;
int i;
int modnum;
int errnum;
int msgcount;

	phashtable = hashtable;
	msgcount = 0;
	printf("\nerrSymDump: HASH NUMBER=%d\n", NHASH);		
	for ( i=0; i < NHASH; i++) {
		if ( (pNextNode = phashtable[i].hashnode) == NULL )
		    continue;
		printf("\nHASHNODE=%d\n", i);		
		while (pNextNode) {
		    modnum = pNextNode->errNum >> 16;
		    errnum = pNextNode->errNum & 0xffff;
		    printf("\tmodule=%d errnum=%d\n\tmessage=\"%s\"\n"
			, modnum , errnum , pNextNode->message);
		    msgcount++;
		    pLastNode = pNextNode;
		    pNextNode=pNextNode->hashnode;
		}
	}
	printf("\nerrSymDump: total number of error messages=%d\n", msgcount);		
}
