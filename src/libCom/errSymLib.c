====================INCLUDES===============================
===================================================
/*
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
 * Modification Log:
 * -----------------
 * .01  mm-dd-yy        iii     Comment
 */



/*
 *
 * 1. Put everything in  errSymLib.c
 *	get rid of error.h
 *	function prototypes in errMdef.h
 * 2. Nick to fix calink.h error comments.
 * 3. Fix trailing blanks on S_...
 * 4. In iocinit - make a call to errSymFind to make it initialize.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */


typedef struct errNumNode {
    ELLNODE             node;
    struct errNumNode  *hashnode;
    long                errNum;
} ERRNUMNODE;

#define NHASH 256

static ELLLIST     errNumList;

static ERRNUMNODE *hashtable[NHASH];

/*
 * when building keep in sorted order
 */

hash = (NHASH - (modnum-500) + errNum) % NHASH;



static int initialized = FALSE;

#if 0 /* from error.h */
#define LOCAL   static
#define NELEMENTS(array)		/* number of elements in an array */ \
		(sizeof (array) / sizeof ((array) [0]))

typedef struct		/* ERRSYMBOL - entry in symbol table */
    {
    char *name;		/* pointer to symbol name */
    long errNum;	/* errMessage symbol number */
    } ERRSYMBOL;
typedef struct		/* ERRSYMTAB - symbol table */
    {
    short nsymbols;	/* current number of symbols in table */
    ERRSYMBOL *symbols;	/* ptr to array of symbol entries */
    } ERRSYMTAB;
typedef ERRSYMTAB *ERRSYMTAB_ID;

#ifdef vxWorks
#define MYERRNO	(errnoGet()&0xffff)
#else
#define MYERRNO	errno
#endif

#endif /* from error.h */


/*********************************************************************/
/* @(#)errMessage.c	1.8	8/11/93 */
/* Modification Log:
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
 */

/* errMessage.c - Handle error messages */

/***************************************************************************
 * This must ultimately be replaced by a facility that allows remote
 * nodes access to the error messages. A message handling communication
 * task should be written that allows multiple remote nodes to request
 * notification of all error messages.
 * For now lets just print messages and last errno via logMsg or printf
 ***************************************************************************/

#ifdef vxWorks
#include <vxWorks.h>
/*#include <stdioLib.h> */
/*#include <strLib.h> */
/*#include <errnoLib.h>*/
#include <taskLib.h>
#include <types.h>
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#endif
#include <error.h>
#include <errMdef.h>
#include <dbDefs.h>

extern ERRSYMTAB_ID errSymTbl;
#if 0
extern char    *calloc();
#endif
struct errDes  *dbErrDes = NULL;
#ifdef vxWorks
int errToLogMsg = FALSE;
#endif

/*Declare storage for errVerbose( defined in errMdef.h)*/

int errVerbose=0;

/*
 * ERRMESSAGE
 */
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

    if (!dbErrDes)
	if (errSymBld())
	    exit(1);

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
#			ifdef vxWorks
				logMsg("%s: calloc error\n", __FILE__);
#			else
				printf("%s: calloc error\n", __FILE__);
#			endif
			return;
		}
	}
	strcpy(pReformat, pformat);
	strcat(pReformat, pAdd);

    }

    verrPrintStatus(status, pReformat, pvar);

    return;
}

/*
 * ERRSYMBLD
 *
 * Create and initializes the dbErrDes structure.
 * This allows direct lookup of Module error messages.
 *
 */

errSymBld()
{
    struct errDes  *pTdbErrDes;
    ERRSYMBOL      *errArray = errSymTbl->symbols;
    ERRSYMBOL      *perrArray;

    int             i;
    int             j;
    int             modnum;
    int             modoff;
    int             errnum;
    int             erroff;
    int             entDim;
    int             modDim = 0;
    long            Size = 0;
    char           *addrI;
    perrArray = errArray;

    /* determine the maximum module number */
    for (i = 0; i < errSymTbl->nsymbols; i++, errArray++) {
	modnum = errArray->errNum >> 16;
	modoff = modnum - 501;
	errnum = errArray->errNum & 0xffff;
	erroff = errnum >> 1;
	if (modnum < 501) {
	    printf("errSymBld: ERROR - Module number in errSymTbl < 501\n");
	    return (-1);
	}
	modDim = modDim <= modoff ? modoff + 1 : modDim;
    }
    /* allocate pTdbErrDes structure */
    if ((pTdbErrDes = (struct errDes *)
	 calloc(1, (unsigned) sizeof(struct errDes))) == NULL) {
	printf("errSymBld: calloc error\n");
	return (-1);
    }
    Size = sizeof(struct errDes);

    /* allocate papErrSet pointer array */
    if ((pTdbErrDes->papErrSet = (struct errSet **)
	 calloc(1, (unsigned) modDim * sizeof(caddr_t))) == NULL) {
	printf("errSymBld: calloc error\n");
	return (-1);
    }
    pTdbErrDes->number = modDim;
    Size += pTdbErrDes->number * sizeof(caddr_t);

    /* allocate a temp structure for each module */
    /* and insert the maximum dimension for it's  messages */
    errArray = perrArray ;
    for (i = 0; i < errSymTbl->nsymbols; i++, errArray++) {
	modnum = errArray->errNum >> 16;
	modoff = modnum - 501;
	errnum = errArray->errNum & 0xffff;
	erroff = errnum >> 1;
	/* has module been allocated yet */
	if (!pTdbErrDes->papErrSet[modoff]) {
	    /* assign a structure for each module */
	    if ((pTdbErrDes->papErrSet[modoff] = (struct errSet *)
		 calloc(1, (unsigned) sizeof(struct errSet))) == NULL) {
		printf("errSymBld: calloc error\n");
		return (-1);
	    }
	    Size += sizeof(struct errSet);
	}
	/* determine max message number in each module */
	/* max entries in each module */
	entDim = pTdbErrDes->papErrSet[modoff]->number;
	entDim = entDim <= erroff ? erroff + 1 : entDim;
	pTdbErrDes->papErrSet[modoff]->number = entDim;
    }
    /* allocate a temp pointer array for each module */
    /* and insert pointer to err string */

    errArray = perrArray ;
    for (i = 0; i < errSymTbl->nsymbols; i++, errArray++) {
	modnum = errArray->errNum >> 16;
	modoff = modnum - 501;
	errnum = errArray->errNum & 0xffff;
	erroff = errnum >> 1;
	if (pTdbErrDes->papErrSet[modoff]) {
	    if (!pTdbErrDes->papErrSet[modoff]->papName) {
		/* allocate ptr array */
		if (!pTdbErrDes->papErrSet[modoff]->papName) {
		    if ((pTdbErrDes->papErrSet[modoff]->papName = (caddr_t *)
			 calloc(1, (unsigned) pTdbErrDes->papErrSet[modoff]->number
				* sizeof(caddr_t))) == NULL) {
			printf("errSymBld: calloc error\n");
			return (-1);
		    }
		    Size += pTdbErrDes->papErrSet[modoff]->number * sizeof(caddr_t);
		}
	    }
	    if (!pTdbErrDes->papErrSet[modoff]->papName[erroff]) {
		pTdbErrDes->papErrSet[modoff]->papName[erroff] = errArray->name;
	    }
	}
    }

    /* allocate the contiguous area */
    if ((dbErrDes = (struct errDes *) calloc(1, (unsigned) Size)) == NULL) {
	printf("errSymBld: calloc error\n");
	return (-1);
    }
    addrI = (caddr_t) dbErrDes + sizeof(struct errDes);
    dbErrDes->papErrSet = (struct errSet **) addrI;
    dbErrDes->number = pTdbErrDes->number;
    addrI += dbErrDes->number * sizeof(caddr_t);

    /* insert number nd pointers and strings */
    for (i = 0; i < dbErrDes->number; i++) {
	if (pTdbErrDes->papErrSet[i]) {
	    dbErrDes->papErrSet[i] = (struct errSet *) addrI;
	    addrI += sizeof(struct errSet);
	    dbErrDes->papErrSet[i]->number = pTdbErrDes->papErrSet[i]->number;
	    dbErrDes->papErrSet[i]->papName = (caddr_t *) addrI;
	    addrI += dbErrDes->papErrSet[i]->number * sizeof(caddr_t);
	    for (j = 0; j < dbErrDes->papErrSet[i]->number; j++) {
		if (pTdbErrDes->papErrSet[i]->papName[j]) {
		    dbErrDes->papErrSet[i]->papName[j]
			= pTdbErrDes->papErrSet[i]->papName[j];
		}
	    }
	}
    }
return(0);
}				/* end of  errMessage.c */
/*********************************************************************/
/* errPrint.c */
/* @(#)errPrint.c	1.6	8/13/93*/
/*
 *      Author:          Bob Zieman
 *      Date:            6-1-91
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
 * .01  10-08-91	mrk	Allow logMsg or printf
 * .02  04-29-93	joh 	extra arg for errPrint()	
 * .03  04-29-93	joh 	errPrint() became errPrintStatus()	
 * .04	05-06-93	joh	errPrintStatus() get var args 
 *				for vprintf()
 */

/* errPrint.c - print error symbol */
#ifdef vxWorks
	extern int errToLogMsg;
#	include <vxWorks.h>
#	include <types.h>
#	include <errnoLib.h>
#	include <logLib.h>
#else
	extern int errno;
#endif vxWorks

#include <stdio.h>
#include <errMdef.h>
#include <error.h>


/*
 * errPrintStatus()
 */
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


/*
 * verrPrintStatus
 */
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
/*********************************************************************/
errSymFind.c
/*********************************************************************/
