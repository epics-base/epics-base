/* devSymbFind.c - Support routines for vxWorks global var device support */

/* $Id$
 *
 * Author:	William Lupton (Keck)
 * Date:	7 June 1996
 */

/* modification history:
 * $Log$
 * Revision 1.1  1998/01/21 20:47:45  mrk
 * restructure; new Symb support
 *
 * Revision 1.2.2.1  1997/10/11 02:26:54  wlupton
 * now working with R3.13
 *
 * Revision 1.2  1996/10/24 18:29:20  wlupton
 * Andrew Johnson's changes (upwards-compatible)
 *
 * Revision 1.2  1996/10/22 15:30:25  anj
 * vxWorks Variable support changed to do pointer indirection at
 * run-time.  Also added device support for the Waveform record.
 *
 * Revision 1.1  1996/09/17 09:55:37  anj
 * Added William Lupton's vxWorks global variable device support, and
 * some slight adjustment to the ANB display screens.
 *
 * Revision 1.1  1996/06/09  00:52:24  wlupton
 * initial insertion
 *
 */

/*
DESCRIPTION:

This module contains routines for converting a vxWorks global variable
name specification into its address. Two cases are supported:

1. If the supplied link field is of type INST_IO, its string value is
   assumed to be of the form:

   [ "*" ] name [ "[" index "]" ]

   where quoted items are literal and square brackets imply optional
   items.  White space is ignored. The leading "*", if specified,
   implies that the variable in question contains the address of the
   desired data. The name is the name of the vxWorks global variable (a
   leading underscore is added). The optional array index is multipled
   by the data element size (e.g. sizeof(char), sizeof(long) or
   sizeof(double)) and applied as an offset to the data address.

   For example:

   a) "fred" refers to the value of the vxWorks global variable "_fred"

   b) "*fred" refers to the value whose address is the value of the
      vxWorks global variable "_fred"

   c) "fred[1]" assumes that the vxWorks global variable "_fred" is an
      array and refers to its second element

   d) "*fred[1]" assumes that the vxWorks global variable "_fred"
      contains the address of an array and refers to the second element
      of the array

   Note that the interpretation of the "*" operator is not the same as
   in C.  For example, "fred" and "fred[0]" are the same and "*fred" and
   "fred[0]" are not the same.  In this version of the driver, the 
   value of the pointer is read at run-time rather than initialisation.
   
   Finally note that strings are not treated any differently from longs
   or doubles in that the address returned from this routine is simply
   the address of the data. "fred" is the entire string. "fred[2]"
   starts at the third character of the string.

2. Otherwise, behavior is the same as before: the name of the vxWorks
   global variable is derived from the record name by stripping off any
   prefix ending with the first ":" and any suffix starting with the
   last ";".  As in the other case, an underscore is automatically
   prefixed.

   For example:

   a) "ppp:fred;sss" refers to the vxWorks global variable "_fred"

   b) "a:b:c;d;e" refers to the vxWorks global variable "_b:c;d"

The second case is supported for backwards compatibility. The first
(INST_IO) is preferred.
*/

#include        <vxWorks.h>
#include        <sysSymTbl.h>

#include        <ctype.h>
#include        <stdio.h>
#include	<stdlib.h>
#include        <string.h>

#include        <dbDefs.h>
#include        <link.h>
#include	<devSymb.h>

/* forward references */
static int parseInstio(char *string, int *deref, char **name, int *index);
static int parseName(char *string, int *deref, char **name, int *index);

/*
 * Determine vxWorks variable name and return address of data
 */
int devSymbFind(char *name, struct link *plink, void *pdpvt)
{
    int  deref;
    char *nptr;
    int  index;
    SYM_TYPE stype;
    void *paddr;
    struct vxSym *private;
    struct vxSym **pprivate = (struct vxSym **) pdpvt;

    /* if link is of type INST_IO, parse INST_IO string */
    if (plink->type == INST_IO)
    {
	struct instio *pinstio = (struct instio *) &plink->value.instio;
	if (parseInstio(pinstio->string, &deref, &nptr, &index))
	    return ERROR;
    }

    /* otherwise derive variable name from record name as before */
    else
    {
	if (parseName(name, &deref, &nptr, &index))
	    return ERROR;
    }

    if (symFindByNameEPICS(sysSymTbl, nptr, (char **) &paddr, &stype))
	return ERROR;

    /* Name exists, allocate a private structure */
    private = (struct vxSym *) malloc(sizeof (struct vxSym));
    if (private == NULL)
    	return ERROR;
    
    /* Fill in the fields */
    private->index = index;
    
    /* Setup pointers to the found symbol address */
    if (deref) {
	private->ppvar = paddr;
	/* private->pvar is not needed with deref symbols */
    } else {
    	private->ppvar = &private->pvar;
    	private->pvar = paddr;
    }

    /* Pass new private structure back to caller */
    *pprivate = private;

    return OK;
}

/*
 * Parse string of the form ["*"]name["["index"]"] (white space is ignored).
 */
static int parseInstio(char *string, int *deref, char **name, int *index)
{
    static char pname[256];
    char *begin;

    /* copy leading underscore to name */
    strcpy(pname, "_");

    /* set default return values */
    *deref = 0;
    *name  = pname;
    *index = 0;

    /* skip leading white space */
    for (; isspace(*string); string++);

    /* if next char is "*", will dereference */
    if (*string == '*')
    {
	*deref = 1;
	string++;
    }

    /* skip white space */
    for (; isspace(*string); string++);

    /* variable name begins here */
    begin = string;

    /* search for white space or "[" */
    for (; *string && !isspace(*string) && *string != '['; string++);

    /* copy and terminate variable name */
    strncat(pname, begin, string-begin);
    pname[string-begin+1] = '\0';

    /* skip white space */
    for (; isspace(*string); string++);

    /* if found "[", parse index */
    if (*string == '[')
    {
	string++;

	for (; isspace(*string); string++);

	for(; isdigit(*string); string++)
	{
	    *index *= 10;
	    *index += *string - '0';
	}

	for (; isspace(*string); string++);

	if (*string != ']')
	{
	    printf("no trailing ]\n");
	    return ERROR;
	}

	string++;
    }

    /* skip trailing white space */
    for (; isspace(*string); string++);

    /* expect to be at end of string */
    if (*string != '\0')
    {
	printf("unexpected trailing characters\n");
	return ERROR;
    }

    return OK;
}

/*
 * Extract vxWorks global variable name from string of form "ppp:name;sss".
 */
static int parseName(char *string, int *deref, char **name, int *index)
{
    static char pname[256];

    /* no dereferencing */
    *deref = 0;

    /* variable names from C have a prepended underscore */
    strcpy(pname,"_");
    strcat(pname, string);

    /* find any suffix and terminate it */
    *name = strrchr(pname, ';');
    if (*name)
       **name = '\0';

    /* find any prefix and bypass it */
    *name = strchr(pname, ':');
    if (*name)
       **name = '_';
    else
       *name = pname;

    /* no indexing */
    *index = 0;

    return OK;
}

/*
 * conditionally compiled routine for testing parsing routines
 */
#ifdef TEST
int test(char *string)
{
    int  error;
    int  deref;
    char *nptr;
    int  index;
    char *addr;
    SYM_TYPE stype;

    printf( "instio: %s -> ", string);
    error = parseInstio(string, &deref, &nptr, &index);
    printf("%s: ", error ? "error" : "ok" );
    printf("deref=%d, name=%s, index=%d", deref, nptr, index);
    if (!symFindByNameEPICS(sysSymTbl, nptr, &addr, &stype))
    {
 	if (deref) addr = *((char **)addr);
	addr += sizeof(long) * index;
	printf(" -> value = %d", *(long *)addr);
    }
    printf("\n");

    printf( "name: %s -> ", string);
    error = parseName(string, &deref, &nptr, &index);
    printf("%s: ", error ? "error" : "ok" );
    printf("deref=%d, name=%s, index=%d", deref, nptr, index);
    if (!symFindByNameEPICS(sysSymTbl, nptr, &addr, &stype))
	printf(" -> value = %d", *(long *)addr);
    printf("\n");

    return OK;
}
#endif	/* TEST */
