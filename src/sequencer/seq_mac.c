/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory


 	seq_mac.c,v 1.2 1995/06/27 15:25:56 wright Exp

	DESCRIPTION: Macro routines for Sequencer.
	The macro table contains name & value pairs.  These are both pointers
	to strings.

	ENVIRONMENT: VxWorks

	HISTORY:
01mar94,ajk	Added seq_macValueGet() as state program interface routine.

***************************************************************************/
#define		ANSI
#include	"seq.h"

LOCAL int seqMacParseName(char *);
LOCAL int seqMacParseValue(char *);
LOCAL char *skipBlanks(char *);
LOCAL MACRO *seqMacTblGet(MACRO *, char *);

/*#define	DEBUG*/

/* 
 *seqMacEval - substitute macro value into a string containing:
 * ....{mac_name}....
 */
VOID seqMacEval(pInStr, pOutStr, maxChar, pMac)
char	*pInStr;
char	*pOutStr;
int	maxChar;
MACRO	*pMac;
{
	char		name[50], *pValue, *pTmp;
	int		nameLth, valLth;

#ifdef	DEBUG
	logMsg("seqMacEval: InStr=%s\n", pInStr);
	taskDelay(30);
#endif
	pTmp = pOutStr;
	while (*pInStr != 0 && maxChar > 0)
	{
		if (*pInStr == '{')
		{	/* Do macro substitution */
			pInStr++; /* points to macro name */
			/* Copy the macro name */
			nameLth = 0;
			while (*pInStr != '}' && *pInStr != 0)
			{
				name[nameLth] = *pInStr++;
				if (nameLth < (sizeof(name) - 1))
					nameLth++;
			}
			name[nameLth] = 0;
			if (*pInStr != 0)
				pInStr++;
				
#ifdef	DEBUG
			logMsg("Macro name=%s\n", name);
			taskDelay(30);
#endif
			/* Find macro value from macro name */
			pValue = seqMacValGet(pMac, name);
			if (pValue != NULL)
			{	/* Substitute macro value */
				valLth = strlen(pValue);
				if (valLth > maxChar)
					valLth = maxChar;
#ifdef	DEBUG
				logMsg("Value=%s\n", pValue);
#endif
				strncpy(pOutStr, pValue, valLth);
				maxChar -= valLth;
				pOutStr += valLth;
			}
			
		}
		else
		{	/* Straight susbstitution */
			*pOutStr++ = *pInStr++;
			maxChar--;
		}
	}
	*pOutStr = 0;
#ifdef	DEBUG
	logMsg("OutStr=%s\n", pTmp);
	taskDelay(30);
#endif
}
/* 
 * seq_macValueGet - given macro name, return pointer to its value.
 */
char	*seq_macValueGet(ssId, pName)
SS_ID		ssId;
char		*pName;
{
	SPROG		*pSP;
	MACRO		*pMac;

	pSP = ((SSCB *)ssId)->sprog;
	pMac = pSP->pMacros;

	return seqMacValGet(pMac, pName);
}
/*
 * seqMacValGet - internal routine to convert macro name to macro value.
 */
char *seqMacValGet(pMac, pName)
MACRO		*pMac;
char		*pName;
{
	int		i;

#ifdef	DEBUG
	logMsg("seqMacValGet: name=%s", pName);
#endif	/* DEBUG */
	for (i = 0 ; i < MAX_MACROS; i++, pMac++)
	{
		if (pMac->pName != NULL)
		{
			if (strcmp(pName, pMac->pName) == 0)
			{
#ifdef	DEBUG
				logMsg(", value=%s\n", pMac->pValue);
#endif	/* DEBUG */
				return pMac->pValue;
			}
		}
	}
#ifdef	DEBUG
	logMsg(", no value\n");
#endif	/* DEBUG */
	return NULL;
}
/*
 * seqMacParse - parse the macro definition string and build
 * the macro table (name/value pairs). Returns number of macros parsed.
 * Assumes the table may already contain entries (values may be changed).
 * String for name and value are allocated dynamically from pool.
 */
int seqMacParse(pMacStr, pSP)
char		*pMacStr;	/* macro definition string */
SPROG		*pSP;
{
	int		nMac, nChar;
	char		*skipBlanks();
	MACRO		*pMac;		/* macro table */
	MACRO		*pMacTbl;	/* macro tbl entry */
	char		*pName, *pValue;

	pMac = pSP->pMacros;
	for ( ;; )
	{
		/* Skip blanks */
		pMacStr = skipBlanks(pMacStr);

		/* Parse the macro name */

		nChar = seqMacParseName(pMacStr);
		if (nChar == 0)
			break; /* finished or error */
		pName = (char *)calloc(nChar+1, 1);
		if (pName == NULL)
			break;
		bcopy(pMacStr, pName, nChar);
		pName[nChar] = 0;
#ifdef	DEBUG
		logMsg("name=%s, nChar=%d\n", pName, nChar);
		taskDelay(30);
#endif
		pMacStr += nChar;

		/* Find a slot in the table */
		pMacTbl = seqMacTblGet(pMac, pName);
		if (pMacTbl == NULL)
			break; /* table is full */
		if (pMacTbl->pName == NULL)
		{	/* Empty slot, insert macro name */
			pMacTbl->pName = pName;
		}

		/* Skip over blanks and equal sign or comma */
		pMacStr = skipBlanks(pMacStr);
		if (*pMacStr == ',')
		{	/* no value after the macro name */
			pMacStr++;
			continue;
		}
		if (*pMacStr++ != '=')
			break;
		pMacStr = skipBlanks(pMacStr);

		/* Parse the value */
		nChar = seqMacParseValue(pMacStr);
		if (nChar == 0)
			break;

		/* Remove previous value if it exists */
		pValue = pMacTbl->pValue;
		if (pValue != NULL)
			free(pValue);

		/* Copy value string into newly allocated space */
		pValue = (char *)calloc(nChar+1, 1);
		if (pValue == NULL)
			break;
		pMacTbl->pValue = pValue;
		bcopy(pMacStr, pValue, nChar);
		pValue[nChar] = 0;
#ifdef	DEBUG
		logMsg("value=%s, nChar=%d\n", pValue, nChar);
		taskDelay(30);
#endif

		/* Skip past last value and over blanks and comma */
		pMacStr += nChar;
		pMacStr = skipBlanks(pMacStr);
		if (*pMacStr++ != ',')
			break;
	}
	if (*pMacStr == 0)
		return 0;
	else
		return -1;
}

/*
 * seqMacParseName() - Parse a macro name from the input string.
 */
LOCAL int seqMacParseName(pStr)
char	*pStr;
{
	int	nChar;

	/* First character must be [A-Z,a-z] */
	if (!isalpha(*pStr))
		return 0;
	pStr++;
	nChar = 1;
	/* Character must be [A-Z,a-z,0-9,_] */
	while ( isalnum(*pStr) || *pStr == '_' )
	{
		pStr++;
		nChar++;
	}
	/* Loop terminates on any non-name character */
	return nChar;
}

/*
 * seqMacParseValue() - Parse a macro value from the input string.
 */
LOCAL int seqMacParseValue(pStr)
char	*pStr;
{
	int	nChar;

	nChar = 0;
	/* Character string terminates on blank, comma, or EOS */
	while ( (*pStr != ' ') && (*pStr != ',') && (*pStr != 0) )
	{
		pStr++;
		nChar++;
	}
	return nChar;
}

/* skipBlanks() - skip blank characters */
LOCAL char *skipBlanks(pChar)
char	*pChar;
{
	while (*pChar == ' ')
		pChar++;
	return	pChar;
}

/*
 * seqMacTblGet - find a match for the specified name, otherwise
 * return an empty slot in macro table.
 */
LOCAL MACRO *seqMacTblGet(pMac, pName)
MACRO	*pMac;
char	*pName;	/* macro name */
{
	int		i;
	MACRO		*pMacTbl;

#ifdef	DEBUG
	logMsg("seqMacTblGet: name=%s\n", pName);
	taskDelay(30);
#endif
	for (i = 0, pMacTbl = pMac; i < MAX_MACROS; i++, pMacTbl++)
	{
		if (pMacTbl->pName != NULL)
		{
			if (strcmp(pName, pMacTbl->pName) == 0)
			{
				return pMacTbl;
			}
		}
	}

	/* Not found, find an empty slot */
	for (i = 0, pMacTbl = pMac; i < MAX_MACROS; i++, pMacTbl++)
	{
		if (pMacTbl->pName == NULL)
		{
			return pMacTbl;
		}
	}

	/* No empty slots available */
	return NULL;
}
