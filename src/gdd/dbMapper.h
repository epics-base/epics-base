#ifndef DB_MAPPER_H
#define DB_MAPPER_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
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

extern "C" {
#include "db_access.h"
#include "cadef.h"
}

class gddApplicationTypeTable;

// Function proto to convert from a db_struct to a gdd.  The gdd will
// reference the data in the db_struct if the db_struct points to an
// array.  The second argument is the number of elements if db_struct
// represents an array, or zero if the db_struct is a scalar.
typedef gdd* (*to_gdd)(void* db_struct, aitIndex element_count);

// Function proto to convert from a gdd to a dbr structure, returns the
// number of elements that the value field of db_struct points to if the
// gdd points to an array or -1 if the number of elements in the value
// field is not identical to element_count available in db_struct.
typedef int (*to_dbr)(void* db_struct, aitIndex element_count, 
                      const gdd &, const vector<string> &enumStringTable);

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
epicsShareExtern const chtype gddAitToDbr[12];
epicsShareExtern const gddDbrMapFuncTable gddMapDbr[DBM_N_DBR_TYPES];

epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable& tt);
epicsShareFunc void gddMakeMapDBR(gddApplicationTypeTable* tt);

inline bool gddDbrToAitValidIndex (unsigned index) 
{
    epicsShareExtern const unsigned gddDbrToAitNElem;
    return index < gddDbrToAitNElem; 
}

inline bool gddAitToDbrValidIndex (unsigned index) 
{ 
    epicsShareExtern const unsigned gddAitToDbrNElem;
    return index < gddAitToDbrNElem; 
}

inline bool gddMapDbrValidIndex (unsigned index) 
{
    epicsShareExtern const unsigned gddMapDbrSize;
    return index < gddMapDbrSize; 
}

#endif

