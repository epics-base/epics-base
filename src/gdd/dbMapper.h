#ifndef DB_MAPPER_H
#define DB_MAPPER_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.13  2000/10/12 21:52:49  jhill
 * changes to support compilation by borland
 *
 * Revision 1.12  2000/10/12 16:10:53  jhill
 * changing include order fixes GNU warning
 *
 * Revision 1.11  2000/06/27 22:32:22  jhill
 * backed out over-zelous use of smart pointers
 *
 * Revision 1.10  2000/04/28 01:40:08  jhill
 * many changes
 *
 * Revision 1.9  2000/03/08 16:23:29  jhill
 * dont wrap include files with extern "C" - let the files themselves take care of it
 *
 * Revision 1.8  1999/10/28 23:33:41  jhill
 * use fully qualified namespace names for C++ RTL classes
 *
 * Revision 1.7  1999/10/28 00:25:44  jhill
 * defined new dbr types
 *
 * Revision 1.6  1999/05/11 00:32:29  jhill
 * fixed const warnings
 *
 * Revision 1.5  1999/04/30 15:24:53  jhill
 * fixed improper container index bug
 *
 * Revision 1.4  1997/08/05 00:51:10  jhill
 * fixed problems in aitString and the conversion matrix
 *
 * Revision 1.3  1997/04/23 17:12:57  jhill
 * fixed export of symbols from WIN32 DLL
 *
 * Revision 1.2  1997/04/10 19:59:25  jhill
 * api changes
 *
 * Revision 1.1  1996/06/25 19:11:36  jbk
 * new in EPICS base
 *
 *
 * *Revision 1.1  1996/05/31 13:15:24  jbk
 * *add new stuff
 *
 */

#include "shareLib.h"
#include "aitTypes.h"
#include "gdd.h"
#include "smartGDDPointer.h"

#include "db_access.h"
#include "cadef.h"

class gddApplicationTypeTable;

// Function proto to convert from a db_struct to a gdd.  The gdd will
// reference the data in the db_struct if the db_struct points to an
// array.  The second argument is the number of elements if db_struct
// represents an array, or zero if the db_struct is a scalar.
typedef smartGDDPointer (*to_gdd)(void* db_struct, aitIndex element_count);

// Function proto to convert from a gdd to a dbr structure, returns the
// number of elements that the value field of db_struct points to if the
// gdd points to an array or -1 if the number of elements in the value
// field is not identical to element_count available in db_struct.
typedef int (*to_dbr)(void* db_struct, aitIndex element_count, 
                      const gdd &, const gddEnumStringTable &enumStringTable);

struct gddDbrMapFuncTable {
	to_gdd	conv_gdd;
	to_dbr	conv_dbr;
};
typedef struct gddDbrMapFuncTable gddDbrMapFuncTable;

struct gddDbrToAitTable {
	aitEnum		type;
	aitUint16	app;
	const char*	app_name;
};
typedef struct gddDbrToAitTable gddDbrToAitTable;

#define DBM_N_DBR_TYPES 39
epicsShareExtern gddDbrToAitTable gddDbrToAit[DBM_N_DBR_TYPES];
epicsShareExtern const int gddAitToDbr[aitConvertLast+1];
epicsShareExtern const gddDbrMapFuncTable gddMapDbr[DBM_N_DBR_TYPES];

epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable& tt);
epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable* tt);

#endif

