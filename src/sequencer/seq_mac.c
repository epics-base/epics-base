/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)seq_mac.c	1.3	4/23/91
	DESCRIPTION: Macro routines for Sequencer.

	ENVIRONMENT: VxWorks
***************************************************************************/
#include	<stdio.h>
#include	<ctype.h>
#include	"vxWorks.h"
#include	"seq.h"

/*#define	DEBUG*/

/* seqMacTblInit - initialize macro table */
seqMacTblInit(mac_ptr)
MACRO	*mac_ptr;
{
	int	i;

	for (i = 0 ; i < MAX_MACROS; i++, mac_ptr++)
	{
		mac_ptr->name = NULL;
		mac_ptr->value = NULL;
	}
}	

/* seqMacEval - substitute macro value into a string */
seqMacEval(pInStr, pOutStr, maxChar, macTbl)
char	*pInStr;
char	*pOutStr;
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
	*pOutStr == 0;
}

/* seqMacValGet - given macro name, return pointer to its value */
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

/* Return number of characters in a macro name */
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
/* seqMacParse - parse the macro definition string and build
the macro table (name/value pairs). Returns number of macros parsed.
Assumes the table may already contain entries (values may be changed).
String for name and value are allocated dynamically from pool.
*/
int seqMacParse(pMacStr, sp_ptr)
char		*pMacStr;	/* macro definition string */
SPROG		*sp_ptr;
{
	int		nMac, nChar;
	char		*skipBlanks();
	MACRO		*macTbl;	/* macro table */
	MACRO		*pMacTbl;	/* macro tbl entry */
	char		*name, *value;

	macTbl = sp_ptr->mac_ptr;
	for ( ;; )
	{
		/* Skip blanks */
		pMacStr = skipBlanks(pMacStr);

		/* Parse the macro name */

		nChar = seqMacParseName(pMacStr);
		if (nChar == 0)
			break; /* finished or error */
		name = seqAlloc(sp_ptr, nChar+1);
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
		value = seqAlloc(sp_ptr, nChar+1);
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

LOCAL char *skipBlanks(pChar)
char	*pChar;
{
	while (*pChar == ' ')
		pChar++;
	return	pChar;
}

/* seqMacTblGet - find a match for the specified name, otherwise
return an empty slot in macro table */
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
