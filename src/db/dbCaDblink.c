/* dbCaDblink.c */
/* share/src/db  $Id$ */

/****************************************************************
*
*      Author:		Nicholas T. Karonis
*      Date:		01-01-92
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
*
****************************************************************/

/****************************************************************
*
*      This file exists because it is currently not possible to have a single .c
* file that contains BOTH channel access calls AND calls to the NEW db 
* functions.  This file calls a number of NEW db functions (dbNameToAddr(),
* dbScanLock(), dbScanUnlock(), and dbPut()).  If it were possible to mix 
* channel access calls and new db calls, the contents of this file should be
* placed into dbCaLink.c as the all the externally visible functions in this
* file are only called from functions in that file.  Additionally, many of the
* data structures found in this file would no longer be necessary.
*
****************************************************************/

/* needed for NULL */
#include <vxWorks.h>

/* needed for typedef of caddr_t which is used in struct dbAddr */
#include <types.h>

/* needed for PVNAME_SZ and FLDNAME_SZ ... must be before dbAccess.h */
#include <dbDefs.h>

/* needed for dbNameToAddr() */
#include <dbAccess.h>

/* needed for struct dbCommon ... should be removed when */
/* MAXSEVERITY_FROM_PDBADDR moved to another header file */
#include <dbCommon.h>

/* needed for (db_field_log *) NULL parm passed to dbGetField() */
#include <db_field_log.h>

#include <errMdef.h>
#include <calink.h>

/* needed for recGblSetSevr() macro */
#include <recSup.h>

/* these should be in dbFldTypes.h where   */
/* the valid DBF and DBR types are defined */

#define INVALID_DBR -1
#define INVALID_DBF -1

/* this macro was defined by list of DBF and DBR types    */
/* found in dbFldTypes.h and belongs in that header file. */

#define DBF_TO_DBR(DBF) \
    ((DBF) == DBF_STRING ? DBR_STRING : \
    ((DBF) == DBF_CHAR ? DBR_CHAR : \
    ((DBF) == DBF_UCHAR ? DBR_UCHAR : \
    ((DBF) == DBF_SHORT ? DBR_SHORT : \
    ((DBF) == DBF_USHORT ? DBR_USHORT : \
    ((DBF) == DBF_LONG ? DBR_LONG : \
    ((DBF) == DBF_ULONG ? DBR_ULONG : \
    ((DBF) == DBF_FLOAT ? DBR_FLOAT : \
    ((DBF) == DBF_DOUBLE ? DBR_DOUBLE : \
    ((DBF) == DBF_ENUM ? DBR_ENUM : \
    INVALID_DBF ))))))))))

/* this macro should be in recSup.h  */
/* to hide the fact that the precord */
/* field needs to be accessed from a */
/* dbaddr to set a new severity.     */
#define MAXSEVERITY_FROM_PDBADDR(PDBADDR, NEWSTATUS, NEWSEVERITY) \
    recGblSetSevr((struct dbCommon *) (PDBADDR)->precord, \
		(NEWSTATUS), (NEWSEVERITY)); 

/* START definitions from db_access.c */

#define oldDBF_STRING      0
#define oldDBF_INT         1
#define oldDBF_SHORT       1
#define oldDBF_FLOAT       2
#define oldDBF_ENUM        3
#define oldDBF_CHAR        4
#define oldDBF_LONG        5
#define oldDBF_DOUBLE      6

/* data request buffer types */
#define oldDBR_STRING           oldDBF_STRING
#define oldDBR_INT              oldDBF_INT
#define oldDBR_SHORT            oldDBF_INT
#define oldDBR_FLOAT            oldDBF_FLOAT
#define oldDBR_ENUM             oldDBF_ENUM
#define oldDBR_CHAR             oldDBF_CHAR
#define oldDBR_LONG             oldDBF_LONG
#define oldDBR_DOUBLE           oldDBF_DOUBLE
#define oldDBR_STS_STRING       7
#define oldDBR_STS_INT          8
#define oldDBR_STS_SHORT        8
#define oldDBR_STS_FLOAT        9
#define oldDBR_STS_ENUM         10
#define oldDBR_STS_CHAR         11
#define oldDBR_STS_LONG         12
#define oldDBR_STS_DOUBLE       13
#define oldDBR_TIME_STRING      14
#define oldDBR_TIME_INT         15
#define oldDBR_TIME_SHORT       15
#define oldDBR_TIME_FLOAT       16
#define oldDBR_TIME_ENUM        17
#define oldDBR_TIME_CHAR        18
#define oldDBR_TIME_LONG        19
#define oldDBR_TIME_DOUBLE      20
#define oldDBR_GR_STRING        21
#define oldDBR_GR_INT           22
#define oldDBR_GR_SHORT         22
#define oldDBR_GR_FLOAT         23
#define oldDBR_GR_ENUM          24
#define oldDBR_GR_CHAR          25
#define oldDBR_GR_LONG          26
#define oldDBR_GR_DOUBLE        27
#define oldDBR_CTRL_STRING      28
#define oldDBR_CTRL_INT         29
#define oldDBR_CTRL_SHORT       29
#define oldDBR_CTRL_FLOAT       30
#define oldDBR_CTRL_ENUM        31
#define oldDBR_CTRL_CHAR        32
#define oldDBR_CTRL_LONG        33
#define oldDBR_CTRL_DOUBLE      34

/* END definitions from db_access.c */

/**************************************/
/*                                    */
/* Externally Visible Local Functions */
/*                                    */
/**************************************/

long dbCaCopyPvar();          /* called from dbCaLink.c */
long dbCaDbPut();             /* called from dbCaLink.c */
long dbCaMaximizeSeverity();  /* called from dbCaLink.c */
long dbCaNameToAddr();        /* called from dbCaLink.c */

/* conversion routines */

short dbCaNewDbfToNewDbr();       /* called from dbCaLink.c */
/* these two go away when channel access learns new db technology */
short dbCaNewDbrToOldDbr();    /* called from dbCaLink.c */
short dbCaOldDbrToNewDbr();    /* called from dbCaLink.c */
short dbCaOldDbrToOldDbrSts(); /* called from dbCaLink.c */

/* these should be moved to db access */
char *dbCaRecnameFromDbCommon(); /* called from dbCaLink.c */
short dbCaDbfFromDbAddr();       /* called from dbCaLink.c */
long dbCaNelementsFromDbAddr();      /* called from dbCaLink.c */

/****************************************************************
*
* EXTERNALLY VISIBLE FUNCTIONS
*
****************************************************************/

/****************************************************************
*
* Description:
*
* Input:
*
* Output:
*
* Returns:
*
* Logic:
*
* Notes:
*     Typically this routine is called from a place that has no
* knowledge about struct dbAddr, therefore pdbaddr comes in as
* a void *.
*
****************************************************************/

short dbCaDbfFromDbAddr(pdbaddr)
void *pdbaddr;
{

short rc;

    if (pdbaddr)
	rc = ((struct dbAddr *) pdbaddr)->field_type;
    else
	rc = (short) INVALID_DBF;

    return rc;

} /* end dbCaDbfFromDbAddr() */

/****************************************************************
*
* long dbCaNameToAddr(pvarname)
* char *pvarname;
*
* Description:
*     During record initialization a record was found to have an input link
* whose source is in a different physical database (on another IOC).  This
* function registers that input link as remote.
*
* Input:
*     char *pvarname   name of destination pvar for input link
*
* Output: None.
*
* Returns:
*     any rc from dbNameToAddr()
*     S_dbCa_nullarg - received a NULL pointer in one of the args
*     S_dbCa_failedmalloc - could not dynamically allocate memory
*
* Notes:
*     The name of the destination pvar for this input link serves as
* the primary key when relating the the Input List in this file
* to the Input List in dbCaLink.c as each input link has exactly one source
* (i.e., name of destination of input link --FD--> input link).
*
*     This function is called by dbCaAddInlink() in dbCaLink.c.
*
****************************************************************/

long dbCaNameToAddr(pvarname, ppdbaddr)
char *pvarname;
void **ppdbaddr;
{

char errmsg[100];
long rc;

    if (pvarname)
    {
	if (ppdbaddr)
	{
	    if (*ppdbaddr = (void *) calloc 
		((unsigned) 1, (unsigned) sizeof (struct dbAddr)))
	    {
		rc = dbNameToAddr(pvarname, (struct dbAddr *) *ppdbaddr);

		if (!RTN_SUCCESS(rc))
		    free(*ppdbaddr);
	    } 
	    else
	    {
		rc = S_dbCa_failedmalloc;
		sprintf(errmsg, 
	    "ERROR: dbCaNameToAddr() could not calloc dbAddr for dpvar >%s<",
		    pvarname);
		errMessage(S_dbCa_failedmalloc, errmsg);
	    } /* endif */
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg,
		"ERROR: dbCaNameToAddr() got NULL ppdest_dbaddr");
	} /* endif */
    } 
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR: dbCaNameToAddr() got NULL pvarname");
    } /* endif */

    return rc;

} /* end dbCaNameToAddr() */

/****************************************************************
*
* Description:
*     A record is being processed that has an output link that refers to a
* process variable on another physical database (another IOC).  This function
* copies the value from the source pvar into the temporary store of the output
* node.  
*
* Input:
*
* Output: None.
*
* Returns:
*     any rc from dbGetField()
*     S_dbCa_nullarg - received a NULL pointer in one of the args
*
* Notes:
*     The source pvar name is used as the primary key to index the Output List
* in this file.
*     The two arguments poptions and pnrequest are passed because this function
* uses dbGetField() to transfer the data from the record to the temporary store
* in the Output List.
*     This function is called by dbCaPutLink() in dbCaLink.c.
*
****************************************************************/

long dbCaCopyPvar(psource_dbaddr, dest_old_dbr_type, pval, 
    poptions, pnrequest)
void *psource_dbaddr;
short dest_old_dbr_type;
void *pval;
long *poptions;
long *pnrequest;
{

char errmsg[100];
long rc;

    if (psource_dbaddr)
    {
	if (pval)
	{
	    if (poptions)
	    {
		if (pnrequest)
		    rc = dbGetField((struct dbAddr *) psource_dbaddr, 
			    dest_old_dbr_type, (caddr_t) pval, 
			    poptions, pnrequest, (db_field_log *) NULL);
		else
		{
		    rc = S_dbCa_nullarg;
		    errMessage(S_dbCa_nullarg,
			"ERROR: dbCaCopyPvar() got NULL pnrequest");
		} /* endif */
	    }
	    else
	    {
		rc = S_dbCa_nullarg;
		errMessage(S_dbCa_nullarg,
		    "ERROR: dbCaCopyPvar() got NULL poptions");
	    } /* endif */
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg, "ERROR: dbCaCopyPvar() got NULL pval");
	} /* endif */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR: dbCaCopyPvar() got NULL psource_dbaddr");
    } /* endif */

    return rc;

} /* end dbCaCopyPvar() */

/****************************************************************
*
* long dbCaMaximizeSeverity(dest_pvarname, new_severity, new_status)
* char dest_pvarname;
* unsigned short new_severity;
* unsigned short new_status;
*
* Description:
*     A record is being processed that has an input link that refers to a
* process variable on another physical database (another IOC) and the input 
* link is MS.  The severity of the source record is appropriately propogated 
* to the destination record.
*
* Input:
*     char dest_pvarname             destination pvar name
*     unsigned short new_severity    new alarm severity
*     unsigned short new_status      new alarm status
*
* Output: None.
*
* Returns:
*     0 - Success
*     S_dbCa_foundnull - found a NULL pointer where one should not be
*     S_dbCa_nullarg - encountered a NULL pointer in one of the input args
*
* Notes:
*     The dest pvar name is used as the primary key to index the Input List
* in this file.
*     This function is called by dbCaGetLink() in dbCaLink.c.
*
****************************************************************/

long dbCaMaximizeSeverity(pdest_dbaddr, new_severity, new_status)
void *pdest_dbaddr;
unsigned short new_severity;
unsigned short new_status;
{

char errmsg[100];
long rc;

    if (pdest_dbaddr)
    {
	rc = 0L;

	MAXSEVERITY_FROM_PDBADDR ((struct dbAddr *) pdest_dbaddr, 
	    new_status, new_severity)
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR:dbCaMaximizeSeverity() got NULL pdest_dbaddr");
    } /* endif */

    return rc;

} /* end dbCaMaximizeSeverity() */

/****************************************************************
*
* Description:
*     A record is being processed that has an input link that refers to a
* process variable on another physical database (another IOC).  This function
* copies the value in the temporary store of the input node into the
* destination.
*
* Input:
*
* Output: None.
*
* Returns:
*     any rc from dbPut()
*     S_dbCa_nullarg - encountered a NULL pointer in one of the input args
*
* Notes:
*     The source pvar name is used as the primary key to index the Input List
* in this file.
*     This function is called by dbCaGetLink() in dbCaLink.c.
*
****************************************************************/

long dbCaDbPut(pdest_dbaddr, dest_revised_new_dbr_type, pval, nelements)
void *pdest_dbaddr;
short dest_revised_new_dbr_type;
void *pval;
long nelements;
{

char errmsg[100];
long rc;

    if (pdest_dbaddr)
    {
	if (nelements > 0L)
	{
/* printf("dbCaDbPut(): writing to >%s< link revised new dbr type %d nelements %ld\n", (((struct dbAddr *) pdest_dbaddr)->precord)->name, dest_revised_new_dbr_type, nelements); */
	    if (pval)
		/* this stuff is copied from dbPutField() without */
		/* honoring the ascii definition process passive  */
		/* for writing to this field within the record.   */
		/*                                                */
		/* since this function is called during record    */
		/* processing, it is assumed that the record has  */
		/* been successfully locked.                      */

		rc = dbPut((struct dbAddr *) pdest_dbaddr, 
			dest_revised_new_dbr_type, (caddr_t) pval, nelements);
	    else
	    {
		rc = S_dbCa_nullarg;
		errMessage(S_dbCa_nullarg, "ERROR: dbCaDbPut() got NULL pval");
	    } /* endif */
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg, 
		"ERROR: dbCaDbPut() got non-positive nelements");
	} /* endif */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg, "ERROR: dbCaDbPut() got NULL ppdest_dbaddr");
    } /* endif */

    return rc;

} /* end dbCaDbPut() */

/****************************************************************
*
* Description:
*
* Input:
*
* Output:
*
* Returns:
*
* Logic:
*
* Notes:
*     Typically this routine is called from a place that has no
* knowledge about struct dbAddr, therefore pdbaddr comes in as
* a void *.
*
****************************************************************/

long dbCaNelementsFromDbAddr(pdbaddr)
void *pdbaddr;
{

long rc;

    if (pdbaddr)
	rc = ((struct dbAddr *) pdbaddr)->no_elements;
    else
	rc = -1L;

    return rc;

} /* end dbCaNelementsFromDbAddr() */

/****************************************************************
*
* short dbCaNewDbfToNewDbr(new_dbf_type)
* short new_dbf_type;
*
* Description:
*     This function converts DBF_XXX -> DBR_XXX both using new db technology.
*
* Input:
*     short new_dbf_type    DBF type to be converted.
*
* Output: None.
*
* Returns:
*     -1 - conversion failed
*     new DBR type
*
* Notes:
*     It is assumed that -1 is an invalid new DBR type and that DBF_INT and
* DBF_SHORT are defined to be the same thing.
*
****************************************************************/

short dbCaNewDbfToNewDbr(new_dbf_type)
short new_dbf_type;
{

    return ((short) DBF_TO_DBR(new_dbf_type));

} /* end dbCaNewDbfToNewDbr() */

/****************************************************************
*
* short dbCaNewDbrToOldDbr(new_dbr_type)
* short new_dbr_type;
*
* Description:
*     This function converts new DBR_XXX types old DBR_XXX types.
*
* Input:
*     short new_dbr_type    new DBR type to be converted
*
* Output: None.
*
* Returns:
*     -1 - conversion failed
*     old DBR type
*
* Notes:
*     It is assumed that -1 is an invalid old DBR type and that DBR_INT and
* DBR_SHORT are defined to be the same thing.
*
****************************************************************/

short dbCaNewDbrToOldDbr(new_dbr_type)
short new_dbr_type;
{

short old_dbr_type;
    
    switch (new_dbr_type)
    {
	case DBR_STRING: old_dbr_type = oldDBR_STRING; break;
	case DBR_CHAR:   old_dbr_type = oldDBR_CHAR;   break;
	case DBR_UCHAR:  old_dbr_type = oldDBR_CHAR;   break;
	case DBR_SHORT:  old_dbr_type = oldDBR_SHORT;  break;
	case DBR_USHORT: old_dbr_type = oldDBR_ENUM;   break;
	case DBR_LONG:   old_dbr_type = oldDBR_LONG;   break;
	case DBR_ULONG:  old_dbr_type = oldDBR_LONG;   break;
	case DBR_FLOAT:  old_dbr_type = oldDBR_FLOAT;  break;
	case DBR_DOUBLE: old_dbr_type = oldDBR_DOUBLE; break;
	case DBR_ENUM:   old_dbr_type = oldDBR_ENUM;   break;

	/* Unrecognizable ... don't know what to do for now */
	default:         old_dbr_type = -1;            break;
    } /* end switch() */

    return old_dbr_type;

} /* end dbCaNewDbrToOldDbr() */

/****************************************************************
*
* short dbCaOldDbrToNewDbr(new_dbr_type)
* short new_dbr_type;
*
* Description:
*     This function converts old DBR_XXX types new DBR_XXX types.
*
* Input:
*     short old_dbr_type    old DBR type to be converted
*
* Output: None.
*
* Returns:
*     -1 - conversion failed
*     new DBR type
*
* Notes:
*     It is assumed that -1 is an invalid new DBR type and that oldDBR_INT and
* oldDBR_SHORT are defined to be the same thing.
*
****************************************************************/

short dbCaOldDbrToNewDbr(old_dbr_type)
short old_dbr_type;
{

short new_dbr_type;
    
    switch (old_dbr_type)
    {

	case oldDBR_STRING: new_dbr_type = DBR_STRING; break;
	/* case oldDBR_INT:     */
	case oldDBR_SHORT:  new_dbr_type = DBR_SHORT;  break;
	case oldDBR_FLOAT:  new_dbr_type = DBR_FLOAT;  break;
	case oldDBR_ENUM:   new_dbr_type = DBR_ENUM;   break;
	case oldDBR_CHAR:   new_dbr_type = DBR_UCHAR;  break;
	case oldDBR_LONG:   new_dbr_type = DBR_LONG;   break;
	case oldDBR_DOUBLE: new_dbr_type = DBR_DOUBLE; break;

	/* Unrecognizable ... don't know what to do for now */
	default:         new_dbr_type = -1;            break;
    } /* end switch() */

    return new_dbr_type;

} /* end dbCaOldDbrToNewDbr() */

/****************************************************************
*
* short dbCaOldDbrToOldDbrSts(old_dbr_type)
* short old_dbr_type;
*
* Description:
*     This function converts old DBR_XXX types to old DBR_STS_XXX types.
*
* Input:
*     short old_dbr_type      old DBR type to be converted
*
* Output: None.
*
* Returns:
*     -1 - conversion failed
*     old DBR_STS type
*
* Logic:
*     convert
*
* Notes:
*     It is assumed that -1 is an invalid old DBR_STS type and that oldDBR_INT
* and oldDBR_SHORT are defined to be the same thing.
*
****************************************************************/

short dbCaOldDbrToOldDbrSts(old_dbr_type)
short old_dbr_type;
{

short old_dbr_sts_type;

    switch (old_dbr_type)
    {

        case oldDBR_STRING: old_dbr_sts_type = oldDBR_STS_STRING; break;
        /* case oldDBR_INT:     */
        case oldDBR_SHORT:  old_dbr_sts_type = oldDBR_STS_SHORT;  break;
        case oldDBR_FLOAT:  old_dbr_sts_type = oldDBR_STS_FLOAT;  break;
        case oldDBR_ENUM:   old_dbr_sts_type = oldDBR_STS_ENUM;   break;
        case oldDBR_CHAR:   old_dbr_sts_type = oldDBR_STS_CHAR;   break;
        case oldDBR_LONG:   old_dbr_sts_type = oldDBR_STS_LONG;   break;
        case oldDBR_DOUBLE: old_dbr_sts_type = oldDBR_STS_DOUBLE; break;

        /* Unrecognizable ... don't know what to do for now */
        default:            old_dbr_sts_type = -1;                break;
    } /* end switch() */

    return old_dbr_sts_type;

} /* end dbCaOldDbrToOldDbrSts() */

/****************************************************************
*
* Description:
*
* Input:
*
* Output:
*
* Returns:
*
* Logic:
*
* Notes:
*     Typically this routine is called from a place that has no
* knowledge about struct dbCommon, therefore precord comes in as
* a void *.
*
****************************************************************/

char *dbCaRecnameFromDbCommon(precord)
void *precord;
{

char *rc;

    if (precord)
	rc = ((struct dbCommon *) precord)->name;
    else
	rc = (char *) NULL;

    return rc;

} /* end dbCaRecnameFromDbCommon() */
