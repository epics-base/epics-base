/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Macro routines for Sequencer.
	The macro table contains name & value pairs.  These are both pointers
	to strings.

	ENVIRONMENT: VxWorks

	HISTORY:
***************************************************************************/
#define		ANSI
#include	"seq.h"

LOCAL int macNameLth(char *);
LOCAL int seqMacParseName(char *);
LOCAL int seqMacParseValue(char *);
LOCAL char *skipBlanks(char *);
LOCAL MACRO *seqMacTblGet(char *, MACRO *);

/*#define	DEBUG*/

/* 
 *seqMacTblInit - initialize macro the table.
 */
seqMacTblInit(pMac)
MACRO	*pMac;
{
	int	i;

	for (i = 0 ; i < MAX_MACROS; i++, pMac++)
	{
		pMac->name = NULL;
		pMac->value = NULL;
	}
}	

/* 
 *seqMacEval - substitute macro value into a string containing:
 * ....{mac_name}....
 */
seqMacEval(pInStr, pOutStr, maxChar, macTbl)
char	*pInStr;
char	*pOutStr;
int	maxChar;
MACRO	*macTbl;
{
	char		*pMacVal, *pTmp;
	int		nameLth, valLth;

#ifdef	DEBUG
	printf("seqMacEval: InStr=%s\n", pInStr);
#endif
	pTmp = pOutStr;
	while (*pInStr != 0 && maxChar > 0)
	{
		if (*pInStr == '{')
		{	/* Macro substitution */
			pInStr++; /* points to macro name */
			nameLth = macNameLth(pInStr);
#ifdef	DEBUG
			printf("Name=%s[%d]\n", pInStr, nameLth);
#endif
			/* Find macro value from macro name */
			pMacVal = seqMacValGet(pInStr, nameLth, macTbl);
			if (pMacVal != NULL)
			{	/* Substitute macro value */
				valLth = strlen(pMacVal);
				if (valLth > maxChar)
					valLth = maxChar;
#ifdef	DEBUG
				printf("Val=%s[%d]\n", pMacVal, valLth);
#endif
				strncpy(pOutStr, pMacVal, valLth);
				maxChar -= valLth;
				pOutStr += valLth;
			}
			pInStr += nameLth;
			if (*pInStr != 0)
				pInStr++; /* skip '}' */
			
		}
		else
		{	/* Straight susbstitution */
			*pOutStr++ = *pInStr++;
			maxChar--;
		}
		*pOutStr = 0;
#ifdef	DEBUG
		printf("OutStr=%s\n", pTmp);
#endif
	}
	*pOutStr = 0;
}
/* 
 * seqMacValGet - given macro name, return pointer to its value.
 */
char	*seqMacValGet(macName, macNameLth, macTbl)
char	*macName;
int	macNameLth;
MACRO	*macTbl;
{
	int	i;

	for (i = 0 ; i < MAX_MACROS; i++, macTbl++)
	{
		if ((macTbl->name != NULL) &&
		 (strlen(macTbl->name) == macNameLth))
		{
			if (strncmp(macName, macTbl->name, macNameLth) == 0)
			{
				return macTbl->value;
			}
		}
	}
	return NULL;
}

/*
 * macNameLth() - Return number of characters in a macro name */
LOCAL int macNameLth(pstr)
char	*pstr;
{
	int	nchar;

	nchar = 0;
	while ( (*pstr != 0) && (*pstr != '}') )
	{
		pstr++;
		nchar++;
	}
	return nchar;
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
	MACRO		*macTbl;	/* macro table */
	MACRO		*pMacTbl;	/* macro tbl entry */
	char		*name, *value;

	macTbl = pSP->mac_ptr;
	for ( ;; )
	{
		/* Skip blanks */
		pMacStr = skipBlanks(pMacStr);

		/* Parse the macro name */

		nChar = seqMacParseName(pMacStr);
		if (nChar == 0)
			break; /* finished or error */
		name = seqAlloc(pSP, nChar+1);
		if (name == NULL)
			break;
		bcopy(pMacStr, name, nChar);
		name[nChar] = 0;
#ifdef	DEBUG
		printf("name=%s, nChar=%d\n", name, nChar);
#endif
		pMacStr += nChar;

		/* Find a slot in the table */
		pMacTbl = seqMacTblGet(name, macTbl);
		if (pMacTbl == NULL)
			break; /* table is full */
		if (pMacTbl->name == NULL)
		{	/* Empty slot, insert macro name */
			pMacTbl->name = name;
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
		value = seqAlloc(pSP, nChar+1);
		if (value == NULL)
			break;
		bcopy(pMacStr, value, nChar);
		value[nChar] = 0;
		pMacStr += nChar;
#ifdef	DEBUG
		printf("value=%s, nChar=%d\n", value, nChar);
#endif

		/* Insert or replace macro value */
		pMacTbl->value = value;

		/* Skip blanks and comma */
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
 */LOCAL int seqMacParseValue(pStr)
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
LOCAL MACRO *seqMacTblGet(name, macTbl)
char	*name;	/* macro name */
MACRO	*macTbl;
{
	int		i;
	MACRO		*pMacTbl;

#ifdef	DEBUG
	printf("seqMacTblGet: name=%s\n", name);
#endif
	for (i = 0, pMacTbl = macTbl; i < MAX_MACROS; i++, pMacTbl++)
	{
		if (pMacTbl->name != NULL)
		{
			if (strcmp(name, pMacTbl->name) == 0)
			{
				return pMacTbl;
			}
		}
	}

	/* Not found, find an empty slot */
	for (i = 0, pMacTbl = macTbl; i < MAX_MACROS; i++, pMacTbl++)
	{
		if (pMacTbl->name == NULL)
		{
			return pMacTbl;
		}
	}

	/* No empty slots available */
	return NULL;
}
