/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	10-10-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .00	10-10-90	rac	initial version
 * .01	06-18-91	rac	installed in SCCS
 * .02  06-25-91	rac	changed nextNonSpaceField to handle quotes
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	nextFieldSubr.c - text field scanning routines
*
* GENERAL DESCRIPTION
*	The routines in this module provide for scanning fields in text
*	strings.  They can be used as the basis for parsing text input
*	to a program.
*
* QUICK REFERENCE
* char	(*pText)[];
* char	(*pField)[];
* char	*pDelim;
*
*   int  nextAlphField(		<>pText,   >pField,   >pDelim		)
*   int  nextAlph1UCField(	<>pText,   >pField,   >pDelim		)
*   int  nextANField(		<>pText,   >pField,   >pDelim		)
*   int  nextChanNameField(	<>pText,   >pField,   >pDelim		)
*   int  nextFltField(		<>pText,   >pField,   >pDelim		)
*   int  nextFltFieldAsDbl(	<>pText,   >pDblVal,  >pDelim		)
*   int  nextIntField(		<>pText,   >pField,   >pDelim		)
*   int  nextIntFieldAsInt(	<>pText,   >pIntVal,  >pDelim		)
*   int  nextIntFieldAsLong(	<>pText,   >pLongVal, >pDelim		)
*   int  nextNonSpace(	        <>pText					)
*   int  nextNonSpaceField(	<>pText,   >pField,   >pDelim		)
*
* DESCRIPTION
*	The input text string is scanned to identify the beginning and
*	end of a field.  At return, the input pointer points to the character
*	following the delimiter and the delimiter has been returned through
*	its pointer; the field contents are `returned' either as a pointer
*	to the first character of the field or as a value returned through
*	a pointer.
*
*	For nextNonSpaceField, the field is defined as either:
*	1)  from the next non-white-space character up to the following
*	    white-space character, or
*	2)  from a " through a matching ", with the "s not being included
*	    in the characters returned to the caller.  The delimeter will
*	    be the character following the closing ".
*
*	In the input string, a '\0' is stored in place of the delimiter,
*	so that standard string handling tools can be used for text fields.
*
*	nextAlphField		scans the next alphabetic field
*	nextAlph1UCField	scans the next alphabetic field, changes
*				the first character to upper case, and
*				changes the rest to lower case
*	nextANField		scans the next alpha-numeric field
*	nextChanNameField	scans the next field as a channel name,
*				delimited by white space or a comma
*	nextFltField		scans the next float field
*	nextFltFieldAsDbl	scans the next float field as a double
*	nextIntField		scans the next integer field
*	nextIntFieldAsInt	scans the next integer field as an int
*	nextIntFieldAsLong	scans the next integer field as a long
*	nextNonSpace		scans to the next non-space character
*	nextNonSpaceField	scans the next field, delimited by white
*				space
*
* RETURNS
*	count of characters in field, including the delimiter.  A special
*	case exists when only '\0' is encountered; in this case 0 is returned.
*	(For quoted alpha strings, the count will include the " characters.)
*
* BUGS
* o	use of type checking macros isn't protected by isascii()
* o	for nextANField, with string in quotes: embedded " isn't handled;
*	if " isn't encountered before end of line, no error message is
*	generated.
*
* SEE ALSO
*	tsTextToStamp()
*
* NOTES
* 1.	fields involving alpha types consider underscore ('_') to be
*	alphabetic.
*
* EXAMPLE
*	char text[]="process 30 samples"
*	char *pText;	pointer into text string
*	char *pCmd;	pointer to first field, to use as a command
*	int  count;	value of second field, number of items to process
*	char *pUnits;	pointer to third field, needed for command processing
*	int  length;	length of field
*	char delim;	delimiter for field
*
*	pText = text;
*	if (nextAlphField(&pText, &pCmd, &delim) <= 1)
*	    error action if empty field
*	if (nextIntFieldAsInt(&pText, &count, &delim) <= 1)
*	    error action if empty field
*	if (nextAlphField(&pText, &pUnits, &delim) <= 1)
*	    error action if empty field
*	printf("command=%s, count=%d, units=%s\n", pCmd, count, pUnits);
*
*-***************************************************************************/
#include <genDefs.h>
#ifdef vxWorks
#   include <ctype.h>
#else
#   include <ctype.h>
#endif

/*-----------------------------------------------------------------------------
*    the preamble skips over leading white space, stopping either at
*    end of string or at a non-white-space character.  If EOS is encountered,
*    then the appropriate return conditions are set up and a return 0 is done.
*----------------------------------------------------------------------------*/
#define NEXT_PREAMBLE \
    char	*pDlm;		/* pointer to field delimiter */ \
    int		count;		/* count of characters (plus delim) */ \
 \
    assert(ppText != NULL); \
    assert(*ppText != NULL); \
    assert(ppField != NULL); \
    assert(pDelim != NULL); \
 \
    if (**ppText == '\0') { \
	*ppField = *ppText; \
	*pDelim = **ppText; \
	return 0; \
    } \
    while (**ppText != '\0' && isspace(**ppText)) \
	(*ppText)++;		/* skip leading white space */ \
    pDlm = *ppField = *ppText; \
    if (*pDlm == '\0') { \
	*pDelim = **ppText; \
	return 0; \
    } \
    count = 1;			/* include delimiter in count */
/*-----------------------------------------------------------------------------
*    the postamble is called for each character in the field.  It moves
*    the pointers and increments the count.  If the loop terminates, then
*    the caller is informed of the delimiter and the source character
*    string has '\0' inserted in place of the delimiter, so that the field
*    is now a proper '\0' terminated string.
*----------------------------------------------------------------------------*/
#define NEXT_POSTAMBLE \
	pDlm++; \
	count++; \
    } \
    *pDelim = *pDlm; \
    *ppText = pDlm; \
    if (*pDlm != '\0') { \
	(*ppText)++;		/* point to next available character */ \
	*pDlm = '\0'; \
    }

int
nextAlphField(ppText, ppField, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
char	**ppField;	/* O pointer to pointer to field */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    NEXT_PREAMBLE
	while (isalpha(*pDlm) || *pDlm == '_') {
	    NEXT_POSTAMBLE
    return count;
}
int
nextAlph1UCField(ppText, ppField, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
char	**ppField;	/* O pointer to pointer to field */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    NEXT_PREAMBLE
    while (isalpha(*pDlm) || *pDlm == '_') {
	if (count == 1) {
	    if (islower(*pDlm))
		*pDlm = toupper(*pDlm);
	}
	else {
	    if (isupper(*pDlm))
		*pDlm = tolower(*pDlm);
	}
	NEXT_POSTAMBLE
    return count;
}
int
nextANField(ppText, ppField, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
char	**ppField;	/* O pointer to pointer to field */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    NEXT_PREAMBLE
    while (isalnum(*pDlm) || *pDlm == '_') {
	NEXT_POSTAMBLE
    return count;
}
int
nextChanNameField(ppText, ppField, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
char	**ppField;	/* O pointer to pointer to field */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    NEXT_PREAMBLE
    while (!isspace(*pDlm)) {
	if (*pDlm == '\0')
	    break;
	else if (*pDlm == ',')
	    break;
	NEXT_POSTAMBLE
    return count;
}
int
nextFltField(ppText, ppField, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
char	**ppField;	/* O pointer to pointer to field */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    NEXT_PREAMBLE
    while (isdigit(*pDlm) || *pDlm=='-' || *pDlm=='+' || *pDlm=='.') {
	NEXT_POSTAMBLE
    return count;
}
int
nextFltFieldAsDbl(ppText, pDblVal, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
double	*pDblVal;	/* O pointer to return field's value */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    char	*pField;	/* pointer to field */
    int		count;		/* count of char in field, including delim */

    assert(pDblVal != NULL);

    count = nextFltField(ppText, &pField, pDelim);
    if (count > 1) {
	if (sscanf(pField, "%lf", pDblVal) != 1)
	    assert(0);
    }

    return count;
}
int
nextIntField(ppText, ppField, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
char	**ppField;	/* O pointer to pointer to field */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    NEXT_PREAMBLE
    while (isdigit(*pDlm) || ((*pDlm=='-' || *pDlm=='+') && count==1)) {
	NEXT_POSTAMBLE
    return count;
}
int
nextIntFieldAsInt(ppText, pIntVal, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
int	*pIntVal;	/* O pointer to return field's value */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    char	*pField;	/* pointer to field */
    int		count;		/* count of char in field, including delim */

    assert(pIntVal != NULL);

    count = nextIntField(ppText, &pField, pDelim);
    if (count > 1) {
	if (sscanf(pField, "%d", pIntVal) != 1)
	    assert(0);
    }

    return count;
}
int
nextIntFieldAsLong(ppText, pLongVal, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
long	*pLongVal;	/* O pointer to return field's value */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    char	*pField;	/* pointer to field */
    int		count;		/* count of char in field, including delim */

    assert(pLongVal != NULL);

    count = nextIntField(ppText, &pField, pDelim);
    if (count > 1) {
	if (sscanf(pField, "%ld", pLongVal) != 1)
	    assert(0);
    }

    return count;
}
int
nextNonSpace(ppText)
char	**ppText;	/* I/O pointer to pointer to text to scan */
{
    while (isspace(**ppText))
	(*ppText)++;
    return 0;
}
int
nextNonSpaceField(ppText, ppField, pDelim)
char	**ppText;	/* I/O pointer to pointer to text to scan */
char	**ppField;	/* O pointer to pointer to field */
char	*pDelim;	/* O pointer to return field's delimiter */
{
    NEXT_PREAMBLE
    if (**ppField == '"') {
	(*ppField)++;		/* skip over leading double quote */
	count++;
	pDlm++;
	while (*pDlm != '"') {	/* scan until find another " */
	    NEXT_POSTAMBLE
	if (*pDelim == '"') {
	    pDlm++;
	    while (isspace(*pDlm))
		pDlm++;		/* skip trailing white space */
	    if (*pDlm == '"')
		pDlm--;
	    *pDelim = *pDlm;	/* give caller the delim and set */
	    *ppText = pDlm;	/*     for scanning the next field */
	    if (*pDlm != '\0')
		(*ppText)++;
	}
    }
    else {
	while (!isspace(*pDlm)) {
	    NEXT_POSTAMBLE
    }
    return count;
}
