/****************************************************************************
			GTA PROJECT   AT division

	Copyright, 1990, The Regents of the University of California
		      Los Alamos National Laboratory

	FILE PATH:	~gta/ar/arCSCheck.c
	ENVIRONMENT:	SunOS, VxWorks
	MAKE OPTIONS:
	SCCS VERSION:	$Id$
*+/mod***********************************************************************
* TITLE	arCSCheck.c - check assumptions about definitions in arCS.h
*
* DESCRIPTION
*
*-
* Modification History
* version   date    programmer   comments
* ------- -------- ------------ -----------------------------------
* 1.1     04/01/90 R. Cole      initial version
*
*****************************************************************************/
#ifdef AR_CS_NEW_DBR
#    define DB_TEXT_GLBLSOURCE
#endif

#include <genDefs.h>
#include <tsDefs.h>
#include <arCS.h>
#ifndef INCLcadefh
#   include <cadef.h>
#endif

#ifndef INCLdb_accessh
#   ifndef AR_CS_NEW_DBR
#       include <db_access.h>
#   else
#       include <new_access.h>
#   endif
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
