/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	02-05-92
 *
 *	Copyright 1992, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under U.S. Government contract:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory.
 *
 * Modification Log:
 * -----------------
 * .01  02-05-92 rac	initial version
 * .02  02-13-92 rac	allow specifying an optional set of parameters
 *
 */
/*+/mod***********************************************************************
* TITLE	recListProg.c - list information about EPICS database records
*
* DESCRIPTION
*	List information about one or several database records.  The
*	record names may be specified in a file or on the command line.
*	The information can be obtained from IOCs, or from DCT short
*	report files.  When using short reports, either a specific short
*	report or a file containing a list of paths to short reports can
*	be used.
*
* USAGE
*	% recListProg [options] [recName  recName  ...]
*
*	where options are:
*	    -I inFile		specifies a path to a file containing a
*				list of record names.  If this option
*				is used, any recName on command line
*				are ignored
*	    -O outFile
*	    -D dctPathFile	specifies either a short report or a file
*				containing a list of paths to short reports.
*	    -f field		specifies a field name to add to the list
*				of fields to print, as DBF_FLOAT; if `field'
*				is `clear', then all "canned" fields are
*				forgotten and only caller-specified fields
*				are listed.
*	    -s field		specifies a field name to add to the list
*				of fields to print, as DBF_SHORT; the special
*				field name `clear' can be used as for -f
*
* BUGS
* o	doesn't get record type or host name when getting information from
*	the IOCs via Channel Access
* o	doesn't allow specifying record type (plain or wildcard) (e.g.,
*	looking for all mbbo records)
* o	Channel Access status messages (<Trying>, <Extra>, etc.) are mixed
*	in with output.  (This is really a CA bug, since they can't be
*	suppressed and they go to stdout.)
* o	doesn't handle state records, enum fields, etc., gracefully
* o	doesn't handle array fields well
*
*-***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <genDefs.h>
#include <cadef.h>
#include <db_access.h>
#if 0
#include "iocmsg.h"
#include "iocinf.h"
extern struct ca_static *ca_static;
#define iiu             (ca_static->ca_iiu)
	if (i == 0 && pCh != NULL && dbf_type_is_valid(ca_field_type(pCh))) {
	    char *x;
	    x = host_from_addr(&iiu[pCh->iocix].sock_addr.sin_addr);
	    x = host_from_addr(&iiu[0].sock_addr.sin_addr);
	    strncpy(info[i].hostName,
		host_from_addr(&iiu[pCh->iocix].sock_addr.sin_addr), 8);
	    printf("\n%s\n", info[i].hostName);
	}
#endif

#define FLD_DIM 20
static int	nFld;
static char	*(fields[FLD_DIM]);
static chtype	types[FLD_DIM];
static int	notInDct[FLD_DIM];
static int	notInField[FLD_DIM];

static char dctValues[FLD_DIM][100];	/* values for the fields above */
static char dctName[100];
static char dctType[100];
static char dctNode[100];

#define DCT_DIM 200
static struct {
    char	fldName[db_name_dim];	/* actual fields in the records */
    char	fldVal[100];
} dctFields[DCT_DIM];

static int nDctFields;		/* number of fields for record just read */


typedef struct {
    int		inIOC;
    int		inDCT;
    chid	pCh;
    char	name[db_name_dim];
    char	val[db_strval_dim];
    short	shtval;
    float	fltval;
    chtype	type;
} CHAN;

typedef struct rec {
    char	name[db_name_dim];	/* gotten from user; may have .FLD */
    char	recName[db_name_dim];	/* stripped of .FLD */
    char	recType[db_name_dim];	/* type of record */
    int		aField;			/* 1 if name had a .FLD */
    char	fldName[db_name_dim];	/* actual field name if `aField' */
    char	filled;
    char	hostName[8];
    int		found;			/* 1 if record has been found */
    int		hasWild;		/* 1 if name has wildcard char's */
    struct rec	*pHead;
    struct rec	*pTail;
    struct rec	*pNext;
    struct rec	*pPrev;
    CHAN	info[FLD_DIM];
} REC;

#define REC_DIM 200
static REC	records[REC_DIM];/* records to be found */
static int	nRec=0;		/* number of records to be found */
static int	allFound;	/* all requested records have been found */


/*+/subr**********************************************************************
* NAME	main
*
*-*/
main(argc, argv)
int     argc;		/* number of command line args */
char   *argv[];		/* command line args */
{
    FILE	*pInFile, *pOutFile, *pDctFile;
    char	filled;
    char	rec[db_name_dim];
    int		c, i, argPtr;
    int		header=1;
    int		cmdLineMode=1;
    int		dctFlag=0;

    nFld = 0;
    initFields();
    pInFile = stdin;
    pOutFile = stdout;
    for (argPtr=1; argPtr<argc; argPtr++) {
	if (strcmp(argv[argPtr], "-I") == 0) {
	    if ((pInFile = fopen(argv[++argPtr], "r")) == NULL) {
		printf("couldn't open %s\n", argv[argPtr]);
		perror("");
		return 1;
	    }
	    cmdLineMode = 0;
	}
	else if (strcmp(argv[argPtr], "-O") == 0) {
	    if ((pOutFile = fopen(argv[++argPtr], "w+")) == NULL) {
		printf("couldn't open %s\n", argv[argPtr]);
		perror("");
		return 1;
	    }
	}
	else if (strcmp(argv[argPtr], "-D") == 0) {
	    if ((pDctFile = fopen(argv[++argPtr], "r")) == NULL) {
		printf("couldn't open %s\n", argv[argPtr]);
		perror("");
		return 1;
	    }
	    dctFlag = 1;
	}
	else if (strcmp(argv[argPtr], "-f") == 0) {
	    argPtr++;
	    if (strcmp(argv[argPtr], "clear") == 0)
		nFld = 1;
	    else if (nFld < FLD_DIM-1) {
		fields[nFld] = argv[argPtr];
		types[nFld] = DBR_FLOAT;
		notInDct[nFld] = 0;
		notInField[nFld] = 0;
		nFld++;
	    }
	}
	else if (strcmp(argv[argPtr], "-s") == 0) {
	    argPtr++;
	    if (strcmp(argv[argPtr], "clear") == 0)
		nFld = 1;
	    else if (nFld < FLD_DIM-1) {
		fields[nFld] = argv[argPtr];
		types[nFld] = DBR_SHORT;
		notInDct[nFld] = 0;
		notInField[nFld] = 0;
		nFld++;
	    }
	}
	else
	    break;
    }
    if (cmdLineMode) {
	for (; argPtr<argc; argPtr++) {
	    if (nRec >= REC_DIM) {
		printf("too many records\n");
		return 1;
	    }
	    if (strcmp(argv[argPtr], "F") == 0) {
		records[nRec].filled = 'F';
		argPtr++;
		if (argPtr >= argc)
		    break;
	    }
	    else
		records[nRec].filled = ' ';
	    strncpy(records[nRec].name, argv[argPtr], db_name_dim);
	    nRec++;
	}
    }
    else {
	while (getFieldFromStream(pInFile, rec, &filled) != EOF) {
	    if (strncmp(rec, "NAME:", 5) == 0) {
		while (getFieldFromStream(pInFile, rec, &filled) != EOF) {
		    if (strcmp(rec, "%endHeader%") == 0) {
			getFieldFromStream(pInFile, rec, &filled);
			break;
		    }
		}
	    }
	    if (nRec >= REC_DIM) {
		printf("too many records\n");
		return 1;
	    }
	    records[nRec].filled = filled;
	    strncpy(records[nRec].name, rec, db_name_dim);
	    nRec++;
	}
    }
    for (i=0; i<nRec; i++)
	setUpRec(&records[i], dctFlag);

    if (dctFlag)
	getInfo_DCT(pDctFile);
    else
	getInfo_IOC();

    for (i=0; i<nRec; i++) {
	printRec(pOutFile, &records[i], dctFlag, header);
	header = 0;
    }

    return 0;
}

/*+/subr**********************************************************************
* NAME	initFields
*
*-*/
static
initFields()
{
    static char *(f[])={"","INIT","STAT","SEVR","MDEL","ADEL","HYST",};
    static chtype t[]={DBR_STRING, DBR_SHORT, DBR_SHORT, DBR_SHORT,
			    DBR_FLOAT, DBR_FLOAT, DBR_FLOAT,};
    static int notDct[]={0, 1, 1, 1, 0, 0, 0, -1};
    static int notField[]={0, 0, 0, 0, 1, 1, 1, -1};

    for (nFld=0; notDct[nFld] >= 0; nFld++) {
	assert(nFld < FLD_DIM);
	fields[nFld] = f[nFld];
	types[nFld] = t[nFld];
	notInDct[nFld] = notDct[nFld];
	notInField[nFld] = notField[nFld];
    }
}

/*+/subr**********************************************************************
* NAME	setUpRec - initialize the structure for a record
*
*-*/
setUpRec(pRec, dctFlag)
REC	*pRec;
int	dctFlag;
{
    char	*dot, *wild;
    int		aField;
    int		i;

    strcpy(pRec->recName, pRec->name);
    strcpy(pRec->recType, ".");
    strcpy(pRec->hostName, "notFound");
    pRec->pHead = NULL;
    pRec->pTail = NULL;

    if ((dot = strchr(pRec->recName, '.')) != NULL) {
	*dot = '\0';	/* truncate pRec->recName at the '.' */
	if (strchr(dot+1, '*') != NULL || strchr(dot+1, '?') != NULL)
	    printf("\n*** wildcards in field name won't be acted on\n");
	if (strcmp(dot+1, "VAL") != 0) {
	    pRec->aField = aField = 1;
	    strcpy(pRec->fldName, dot+1);
	}
	else {
	    pRec->aField = aField = 0;
	    strcpy(pRec->fldName, ".");
	}
    }
    else {
	pRec->aField = aField = 0;
	strcpy(pRec->fldName, ".");
    }
    pRec->found = 0;
    if ((wild = strchr(pRec->recName, '*')) == NULL)
	wild = strchr(pRec->recName, '?');
    if (wild != NULL) {
	if (dctFlag) {
	    strcpy(pRec->recType, "wild");
	    pRec->hasWild = 1;
	}
	else
	    printf("\n*** wildcards in record name won't be acted on\n");
    }

    for (i=0; i<nFld; i++) {
	if (i == 0 && pRec->aField)
	    strcpy(pRec->info[i].name, pRec->name);
	else if (fields[i] != '\0')
	    sprintf(pRec->info[i].name, "%s.%s", pRec->recName, fields[i]);
	else
	    strcpy(pRec->info[i].name, pRec->name);
	pRec->info[i].pCh = NULL;
	pRec->info[i].type = types[i];
	pRec->info[i].inIOC = pRec->info[i].inDCT = 1;
	if (dctFlag) {
	    strcpy(pRec->info[i].val, ".");
	    if (notInDct[i])
		pRec->info[i].inDCT = 0;
	}
	else if (aField && notInField[i]) {
	    strcpy(pRec->info[i].val, "n/a");
	    pRec->info[i].inIOC = 0;
	}
	else
	    strcpy(pRec->info[i].val, "bad!");
    }
}

/*+/subr**********************************************************************
* NAME	getInfo_DCT - get information from DCT
*
* DESCRIPTION
*	Gets information from DCT.  This routine will process either
*	1) a file containing a list of white-space-separated path names
*	for DCT short reports; or 2) an actual DCT short report.
*
*-*/
getInfo_DCT(pDctFile)
FILE	*pDctFile;	/* I control file or short report */
{
    FILE	*pFile;
    char	file[100];

    while (getFieldFromStream(pDctFile, file, NULL) != EOF) {
	if (strcmp(file, "$$mono") == 0) {
	    getInfo_DCTfile(pDctFile);
	    break;
	}
	if ((pFile = fopen(file, "r")) == NULL) {
	    printf("couldn't open %s\n", file);
	    perror("");
	    return 1;
	}
	getInfo_DCTfile(pFile);
	fclose(pFile);
	if (allFound)
	    break;
    }
}

/*+/subr**********************************************************************
* NAME	getInfo_DCTfile - process a DCT short report
*
*-*/
getInfo_DCTfile(pFile)
FILE	*pFile;
{
    int		rec, fld;
    int		stat;
    int		i;
    int		c;
    char	field[100];
    char	value[100];
    int		gotOne=0;

    while (1) {
	c = getFieldFromStream(pFile, field, NULL);
	if (c == EOF || strcmp(field, "PV:") == 0) {
	    if (gotOne)
		getInfo_DCT_work();
	    allFound = 1;
	    for (i=0; i<nRec; i++) {
		if (records[i].found == 0) {
		    allFound = 0;
		    break;
		}
	    }
	    if (allFound)
		break;
	}
	if (c == EOF)
	    break;
	if (strcmp(field, "PV:") == 0) {
	    gotOne = 1;
	    getFieldFromStream(pFile, dctName, NULL);
	    strcpy(dctType, "n/a");
	    strcpy(dctNode, "n/a");
	    nDctFields = 0;
	    for (fld=0; fld<nFld; fld++)
		strcpy(dctValues[fld], "n/a");
	    c = getFieldFromStream(pFile, field, NULL);
	    if (strcmp(field, "Type:") == 0) {
		getFieldFromStream(pFile, dctType, NULL);
		c = getFieldFromStream(pFile, field, NULL);
	    }
	    if (strcmp(field, "Node:") == 0)
		c = getFieldFromStream(pFile, dctNode, NULL);
	}
	else {
	    c = getFieldFromStream(pFile, value, NULL);
	    if (nDctFields < DCT_DIM) {
		strcpy(dctFields[nDctFields].fldName, field);
		strcpy(dctFields[nDctFields].fldVal, value);
		nDctFields++;
	    }
	    for (fld=0; fld<nFld; fld++) {
		if (notInDct[fld])
		    ;		/* no action */
		else if (strcmp(field, fields[fld]) == 0) {
		    strcpy(dctValues[fld], value);
		    break;
		}
	    }
	}
	while (c != EOF && c != '\n')
	    c = fgetc(pFile);
    }
}

/*+/subr**********************************************************************
* NAME	getInfo_DCT_work - process a dct record
*
*-*/
getInfo_DCT_work()
{
    int		i, rec, fld;
    REC		*pRec;

    for (rec=0; rec<nRec; rec++) {
	if (records[rec].found)
	    ;		/* don't compare if already found */
	else if (wildMatch(dctName, records[rec].recName, 1)) {
	    if (records[rec].found || records[rec].hasWild) {
		if ((pRec = (REC *)GenMalloc(sizeof(REC))) == NULL) {
		    printf("couldn't allocate memory\n");
		    exit (1);
		}
		if (!records[rec].aField)
		    strcpy(pRec->name, dctName);
		else
		    sprintf(pRec->name, "%s.%s", dctName, records[rec].fldName);
		setUpRec(pRec, 1);
		pRec->filled = records[rec].filled;
		DoubleListAppend(pRec, records[rec].pHead, records[rec].pTail);
	    }
	    else
		pRec = &records[rec];
	    strcpy(pRec->hostName, dctNode);
	    strcpy(pRec->recType, dctType);
	    if (pRec->aField) {
		strcpy(pRec->info[0].val, "unknown");
		for (i=0; i<nDctFields; i++) {
		    if (strcmp(dctFields[i].fldName, pRec->fldName) == 0) {
			strcpy(pRec->info[0].val, dctFields[i].fldVal);
			break;
		    }
		}
	    }
	    else
		strcpy(pRec->info[0].val, ".");
	    if (pRec->hasWild == 0)
		pRec->found = 1;
	    for (fld=1; fld<nFld; fld++) {
		if (notInDct[fld])
		    ;		/* no action */
		else if (pRec->aField && notInField[fld])
		    ;		/* no action */
		else {
		    strcpy(pRec->info[fld].val, dctValues[fld]);
		    if (types[fld] == DBR_SHORT)
			sscanf(dctValues[fld], "%hd", &pRec->info[fld].shtval);
		    else if (types[fld] == DBR_FLOAT)
			sscanf(dctValues[fld], "%f", &pRec->info[fld].fltval);
		}
	    }
	}
    }
}

/*+/subr**********************************************************************
* NAME	getInfo_IOC - get information from IOCs
*
*-*/
getInfo_IOC()
{
    int		rec, fld;
    int		stat;
    int		i;
    chid	pCh;

/*-----------------------------------------------------------------------------
* make the connections
*----------------------------------------------------------------------------*/
    for (rec=0; rec<nRec; rec++) {
	for (fld=0; fld<nFld; fld++) {
	    if (records[rec].info[fld].inIOC) {
		stat = ca_search(records[rec].info[fld].name,
				&records[rec].info[fld].pCh);
		if (stat != ECA_NORMAL) {
		    records[rec].info[fld].inIOC = 0;
		    records[rec].info[fld].pCh = NULL;
		    strcpy(records[rec].info[fld].val, "err");
		}
		else
		    ca_pend_io(1.0);
	    }
	}
    }
    for (i=0; i<10; i++) {
	if (ca_pend_io(1.0) == ECA_NORMAL)
	    break;
    }

/*-----------------------------------------------------------------------------
* check connections and get the values for those that are complete
*----------------------------------------------------------------------------*/
    for (rec=0; rec<nRec; rec++) {
	for (fld=0; fld<nFld; fld++) {
	    if ((pCh = records[rec].info[fld].pCh) != NULL) {
		if (dbf_type_is_valid(ca_field_type(pCh))) {
		    strcpy(records[rec].info[fld].val, "bad!");
		    stat = ca_get(DBR_STRING, pCh, records[rec].info[fld].val);
		    if (types[fld] == DBR_SHORT) {
			stat = ca_get(types[fld], pCh,
					&records[rec].info[fld].shtval);
		    }
		    else if (types[fld] == DBR_FLOAT) {
			stat = ca_get(types[fld], pCh,
					&records[rec].info[fld].fltval);
		    }
		}
		else {
		    ca_clear_channel(pCh);
		    records[rec].info[fld].pCh = NULL;
		    if (fld == 0)
			strcpy(records[rec].info[fld].val, "notConn");
		    else
			strcpy(records[rec].info[fld].val, ".");
		}
	    }
	}
    }
    for (i=0; i<10; i++) {
	if (ca_pend_io(1.0) == ECA_NORMAL)
	    break;
    }
}

/*+/subr**********************************************************************
* NAME	printRec - print the information for a record
*
*-*/
printRec(pOutFile, pRec, dctFlag, header)
FILE	*pOutFile;
REC	*pRec;
int	dctFlag;
int	header;
{
    int		i;
    REC		*pR;
    char	fltText[100];

/*-----------------------------------------------------------------------------
* print the info
*----------------------------------------------------------------------------*/
    if (dctFlag) {
	if (header) {
	    fprintf(pOutFile, "\nnode     type %30s F %8s", "name", "value");
	    for (i=1; i<nFld; i++) {
		if (notInDct[i] == 0)
		    fprintf(pOutFile, " %4.4s", fields[i]);
	    }
	    fprintf(pOutFile, "\n");
	}
	if ((pR = pRec->pHead) != NULL) {
	    while (pR != NULL) {
		fprintf(pOutFile, "%-8s %4.4s %30s %c %8s", pR->hostName,
			pR->recType, pR->name, pR->filled, pR->info[0].val);
		for (i=1; i<nFld; i++) {
		    if (notInDct[i] == 0) {
			if (types[i] == DBR_FLOAT) {
			    cvtDblToTxt(fltText, 4, pR->info[i].fltval, 3);
			    fprintf(pOutFile, " %4.4s", fltText);
			}
			else
			    fprintf(pOutFile, " %4.4s", pR->info[i].val);
		    }
		}
		fprintf(pOutFile, "\n");
		pR = pR->pNext;
	    }
	    return;
	}
	fprintf(pOutFile, "%-8s %4.4s %30s %c %8s", pRec->hostName,
		pRec->recType, pRec->name, pRec->filled, pRec->info[0].val);
	for (i=1; i<nFld; i++) {
	    if (notInDct[i] == 0) {
		if (types[i] == DBR_FLOAT) {
		    cvtDblToTxt(fltText, 4, pRec->info[i].fltval, 3);
		    fprintf(pOutFile, " %4.4s", fltText);
		}
		else
		    fprintf(pOutFile, " %4.4s", pRec->info[i].val);
	    }
	}
	fprintf(pOutFile, "\n");
	return;
    }
    if (header) {
	fprintf(pOutFile, "\n%30s F %8s", "name", "value");
	for (i=1; i<nFld; i++)
	    fprintf(pOutFile, " %4.4s", fields[i]);
	fprintf(pOutFile, "\n");
    }
    fprintf(pOutFile, "%30s %c %8s",
			pRec->name, pRec->filled, pRec->info[0].val);
    for (i=1; i<nFld; i++) {
	if (types[i] == DBR_STRING ||
		strcmp(pRec->info[i].val, "bad!") == 0 ||
		strcmp(pRec->info[i].val, ".") == 0 ||
		strcmp(pRec->info[i].val, "n/a") == 0)
	    fprintf(pOutFile, " %4.4s", pRec->info[i].val);
	else if (types[i] == DBR_FLOAT) {
	    cvtDblToTxt(fltText, 4, pRec->info[i].fltval, 3);
	    fprintf(pOutFile, " %4.4s", fltText);
	}
	else
	    fprintf(pOutFile, " %4d", pRec->info[i].shtval);
    }
    fprintf(pOutFile, "\n");
}

/*+/subr**********************************************************************
* NAME	getFieldFromStream - get next name from file
*
* DESCRIPTION
*	Gets the next white-space-delimited field from a FILE* stream.
*	Blank lines are ignored.  The `#' character starts a comment--
*	all the rest of a line is ignored.  The `#' also serves as
*	a delimiter for a field.
*
*	If the pFilled argument isn't NULL, then a field of "F" is
*	treated special:  It is taken to be a modifier for the following
*	field.
*
* RETURNS
*	delimiter, or
*	EOF
*
*-*/
getFieldFromStream(pFile, pName, pFilled)
FILE	*pFile;
char	*pName;
char	*pFilled;
{
    char	*name=pName;
    int		c;
    char	filled;

    *name = '\0';
    filled = ' ';
    while ((c = fgetc(pFile)) != EOF) {
	if (!isascii(c))
	    break;
	if (isspace(c)) {
	    if (name == pName)
		;	/* skip over white space */
	    else if (strcmp(pName, "F") == 0) {
		if (pFilled == NULL)
		    break;
		filled = 'F';
		name = pName;
		*name = '\0';
	    }
	    else
		break;
	}
	else if (c == '#') {
	    while ((c = fgetc(pFile)) != EOF && c != '\n')
		;	/* keep skipping until \n */
	    if (name != pName)
		break;
	}
	else {
	    *(name++) = c;
	    *name = '\0';
	}
    }
    if (name == pName)
	return EOF;

    if (pFilled != NULL)
	*pFilled = filled;
    return c;
}

/*+/subr**********************************************************************
* NAME	cvtDblToTxt - convert double to text, being STINGY with space
*
* DESCRIPTION
*	Convert a double value to text.  The main usefulness of this routine
*	is that it maximizes the amount of information presented in a
*	minimum number of characters.  For example, if a 1 column width
*	is specified, only the sign of the value is presented.
*
*	When an exponent is needed to represent the value, for narrow
*	column widths only the exponent appears.  If there isn't room
*	even for the exponent, large positive exponents will appear as E*,
*	and large negative exponents will appear as E-.
*
*	Negative numbers receive some special treatment.  In narrow
*	columns, very large negative numbers may be represented as -* and

*
*-*/
static
cvtDblToTxt(text, width, value, decPl)
char	*text;		/* O text representation of value */
int	width;		/* I max width of text string (not counting '\0') */
double	value;		/* I value to print */
int	decPl;		/* I max # of dec places to print */
{
    double	valAbs;		/* absolute value of caller's value */
    int		wholeNdig;	/* number of digits in "whole" part of value */
    double	logVal;		/* log10 of value */
    int		decPlaces;	/* number of decimal places to print */
    int		expI;		/* exponent for frac values */
    double	expD;
    int		expWidth;	/* width needed for exponent field */
    int		excess;		/* number of low order digits which
				    won't fit into the field */
    char	tempText[100];	/* temp for fractional conversions */
    int		roomFor;
    int		minusWidth;	/* amount of room for - sign--0 or 1 */

/*-----------------------------------------------------------------------------
*    special cases
*----------------------------------------------------------------------------*/
    if (value == 0.) {
	(void)strcpy(text, "0");
	return;
    }
    else if (value == 1.) {
	(void)strcpy(text, "1");
	return;
    }
    if (width == 1) {
	if (value >= 0)
	    strcpy(text, "+");
	else
	    strcpy(text, "-");
	return;
    }
    else if (width == 2 && value <= -1.) {
	if (value == -1.)
	    strcpy(text, "-1");
	else
	    strcpy(text, "-");
	return;
    }

    valAbs = value>0. ? value : -value;
    logVal = log10(valAbs);
    if (logVal < 0.) {
/*-----------------------------------------------------------------------------
*    numbers with only a fractional part
*----------------------------------------------------------------------------*/
	if (width == 2) {
	    if (value > 0.)
		strcpy(tempText, "0E-");
	    else
		strcpy(tempText, "0-.");
	}
	else if (width == 3 && value < 0)
	    strcpy(tempText, "0-.");
	else {
	    if (value < 0.)
		minusWidth = 1;
	    else
		minusWidth = 0;
	    expI = -1 * (int)logVal;
	    if (expI < 9)
		expWidth = 3;		/* need E-n */
	    else if (expI < 99)
		expWidth = 4;		/* need E-nn */
	    else if (expI < 999)
		expWidth = 5;		/* need E-nnn */
	    else
		expWidth = 6;		/* need E-nnnn */
	    /* figure out how many sig digit can appear if print "as is" */
	    /* expI is the number of leading zeros */
	    roomFor = width - expI - 1 - minusWidth;	/* -1 for the dot */
	    if (roomFor >= 1) {
		decPlaces = expI + decPl;
		if (decPlaces > width -1 - minusWidth)
		    decPlaces = roomFor + expI;
		(void)sprintf(tempText, "%.*f", decPlaces, value);
		if (value < 0.)
		    tempText[1] = '-';
	    }
	    else {
		expD = expI;
		value *= exp10(expD);
		if (value > 0.)
		    sprintf(tempText, "0.E-%d", expI);
		else
		    sprintf(tempText, "--.E-%d", expI);
	    }
	}

	strncpy(text, &tempText[1], width);
	text[width] = '\0';
	return;
    }

/*-----------------------------------------------------------------------------
*    numbers with both an integer and a fractional part
*
*	find out how many columns are required to represent the integer part
*	of the value.  A - is counted as a column;  the . isn't.
*----------------------------------------------------------------------------*/
    wholeNdig = 1 + (int)logVal;
    if (value < 0.)
	wholeNdig++;
    if (wholeNdig < width-1) {
/*-----------------------------------------------------------------------------
*    the integer part fits well within the field.  Find out how many
*    decimal places can be printed (honoring caller's decPl limit).
*----------------------------------------------------------------------------*/
	decPlaces = width - wholeNdig - 1;
	if (decPl < decPlaces)
	    decPlaces = decPl;
	if (decPl > 0)
	    (void)sprintf(text, "%.*f", decPlaces, value);
	else
	    (void)sprintf(text, "%d", nint(value));
    }
    else if (wholeNdig == width || wholeNdig == width-1) {
/*-----------------------------------------------------------------------------
*    The integer part just fits within the field.  Print the value as an
*    integer, without printing the superfluous decimal point.
*----------------------------------------------------------------------------*/
	(void)sprintf(text, "%d", nint(value));
    }
    else {
/*-----------------------------------------------------------------------------
*    The integer part is too large to fit within the caller's field.  Print
*    with an abbreviated E notation.
*----------------------------------------------------------------------------*/
	expWidth = 2;				/* assume that En will work */
	excess = wholeNdig - (width - 2);
	if (excess > 999) {
	    expWidth = 5;			/* no! it must be Ennnn */
	    excess += 3;
	}
	else if (excess > 99) {
	    expWidth = 4;			/* no! it must be Ennn */
	    excess += 2;
	}
	else if (excess > 9) {
	    expWidth = 3;			/* no! it must be Enn */
	    excess += 1;
	}
/*-----------------------------------------------------------------------------
*	Four progressively worse cases, with all or part of exponent fitting
*	into field, but not enough room for any of the value
*		Ennn		positive value; exponent fits
*		-Ennn		negative value; exponent fits
*		+****		positive value; exponent too big
*		-****		negative value; exponent too big
*----------------------------------------------------------------------------*/
	if (value >= 0. && expWidth == width)
	    (void)sprintf(text, "E%d", nint(logVal));
	else if (value < 0. && expWidth == width-1)
	    (void)sprintf(text, "-E%d", nint(logVal));
	else if (value > 0. && expWidth > width)
	    (void)sprintf(text, "%.*s", width, "+*******");
	else if (value < 0. && expWidth > width-1)
	    (void)sprintf(text, "%.*s", width, "-*******");
	else {
/*-----------------------------------------------------------------------------
*	The value can fit, in exponential notation
*----------------------------------------------------------------------------*/
	    (void)sprintf(text, "%dE%d",
			nint(value/exp10((double)excess)), excess);
	}
    }
}
