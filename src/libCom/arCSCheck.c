/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	04-01-90
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
 * .01	04-01-90	rac	initial version
 * .02	07-31-91	rac	installed in SCCS
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	arCSCheck.c - check assumptions about definitions in arCS.h
*
* DESCRIPTION
*
*-***************************************************************************/

#include <genDefs.h>
#include <tsDefs.h>
#include <arCS.h>
#ifndef INCLcadefh
#   include <cadef.h>
#endif

#ifndef INCLdb_accessh
#   include <db_access.h>
#endif



/*+/subr**********************************************************************
* NAME	arCSCheck - check assumptions used by AR about the control system
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	doesn't actually check against GTACS #define's
*
* SEE ALSO
*	arCS.h
*
* EXAMPLE
*
*-*/
int
arCSCheck()
{
#ifdef NDEBUG
#    define NDEBUG		/* force checks even if compiled NDEBUG */
#endif

/*----------------------------------------------------------------------------
*    dimensions for text strings must be multiples of 4 for proper alignment
*    of structures.  In addition, specific dimensions are checked for string
*    types which are stored in files--changing these dimensions requires
*    reformatting existing files.
*
*    Just for documentation purposes, alarm severity and status are checked.
*    Again, these are important because of file layout.
*---------------------------------------------------------------------------*/
    assert(AR_NAME_DIM % 4 == 0);
    assert(AR_NAME_DIM == 36);
    assert(AR_NAME_DIM == db_name_dim);
    assert(AR_STRVAL_DIM == 40);
    assert(AR_STRVAL_DIM == db_strval_dim);
    assert(AR_DESC_DIM % 4 == 0);
    assert(AR_DESC_DIM == 24);
    assert(AR_DESC_DIM == db_desc_dim);
    assert(AR_UNITS_DIM % 4 == 0);
    assert(AR_UNITS_DIM == 8);
    assert(AR_UNITS_DIM == db_units_dim);
    assert(AR_STATE_DIM == 16);
    assert(AR_STATE_DIM == db_state_dim);
    assert(AR_STATE_TEXT_DIM == 26);
    assert(AR_STATE_TEXT_DIM == db_state_text_dim);
    assert(sizeof(AR_ALM_SEV) == 1);
    assert(sizeof(AR_ALM_STAT) == 1);

/*----------------------------------------------------------------------------
*    check for  incompatibilities between the assumptions used by AR and
*    the actual sizes in the control system.
*---------------------------------------------------------------------------*/
    assert(AR_STRVAL_DIM == dbr_size[DBR_STRING]);
#ifndef AR_CS_NEW_DBR
    assert(2 == dbr_size[DBR_INT]);
#else
    assert(2 == dbr_size[DBR_SHORT]);
#endif
    assert(4 == dbr_size[DBR_FLOAT]);

/*----------------------------------------------------------------------------
*    miscellaneous
*---------------------------------------------------------------------------*/
    assert(4 == sizeof(long));	/* block numbers require 32 bit integer;
				also important for file layout */
    assert(2 == sizeof(short));	/* important for file layout */
    assert(4 == sizeof(float));	/* important for file layout */
    assert(1 == sizeof(char));	/* important for file layout */
    assert(2 == sizeof(USHORT));/* important for file layout */
    assert(4 == sizeof(ULONG));	/* important for file layout */
    assert(TS_EPOCH_YEAR == 1990);/* existing file time stamps are
				invalidated if epoch year changes*/

    return OK;
}
