#ifndef DB_MAPPER_H
#define DB_MAPPER_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 *
 * *Revision 1.1  1996/05/31 13:15:24  jbk
 * *add new stuff
 *
 */

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
// represents an array, or zero if the db_struct is a scaler.
typedef gdd* (*to_gdd)(void* db_struct, aitIndex element_count);

// Function proto to convert from a gdd to a dbr structure, returns the
// number of elements that the value field of db_struct points to if the
// gdd points to an array.  The db_struct will reference the data
// contained within the gdd (which is probably also referenced from the user).
typedef int (*to_dbr)(void* db_struct, gdd*);

struct gddDbrMapFuncTable {
	to_gdd	conv_gdd;
	to_dbr	conv_dbr;
};
typedef struct gddDbrMapFuncTable gddDbrMapFuncTable;

struct gddDbrToAitTable {
	aitEnum		type;
	aitUint16	app;
	char*		app_name;
};
typedef struct gddDbrToAitTable gddDbrToAitTable;

extern gddDbrToAitTable gddDbrToAit[];
extern const chtype gddAitToDbr[];
extern gddDbrMapFuncTable gddMapDbr[];
void gddMakeMapDBR(gddApplicationTypeTable& tt);
void gddMakeMapDBR(gddApplicationTypeTable* tt);

#endif

