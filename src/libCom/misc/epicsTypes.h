/* $Id  */

/*
 *      Author: 	Jeff Hill  
 *      Date:          	5-95 
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
 * $Log$
 * Revision 1.3  1997/04/10 20:00:40  jhill
 * VMS changes
 *
 * Revision 1.2  1996/06/20 16:27:33  jhill
 * eliminated sbufs
 *
 * Revision 1.1  1996/01/25 21:38:48  mrk
 * moved files from /base/include
 *
 * Revision 1.7  1995/12/19 19:46:44  jhill
 * added epicsStatus typedef
 *
 * Revision 1.6  1995/09/29  21:41:41  jhill
 * added use of sbufs
 *
 */

#ifndef INCepicsTypesh
#define INCepicsTypesh 1

#include <shareLib.h>

#ifndef stringOf
#	ifdef __STDC__ 
#		define stringOf(TOKEN) #TOKEN
#	else
#		define stringOf(TOKEN) "TOKEN"
#	endif
#endif

typedef enum {
	epicsFalse=0, 
	epicsTrue=1 } 	epicsBoolean;

/*
 * Architecture Independent Data Types
 * (so far this is sufficient for all archs we have ported to)
 */
typedef char		epicsInt8;
typedef unsigned char   epicsUInt8;
typedef short           epicsInt16;
typedef unsigned short  epicsUInt16;
typedef epicsUInt16	epicsEnum16;
typedef int             epicsInt32;
typedef unsigned 	epicsUInt32;
typedef float           epicsFloat32;
typedef double          epicsFloat64;
typedef	unsigned long	epicsIndex;
typedef epicsInt32	epicsStatus;	

typedef struct {
	unsigned	length;
	char		*pString;
}epicsString;

/*
 * !! Dont use this - it may vanish in the future !!
 *
 * Provided only for backwards compatibility with
 * db_access.h 
 *
 */
#define MAX_STRING_SIZE 40
typedef char            epicsOldString[MAX_STRING_SIZE]; 

/*
 * union of all types
 *
 * Strings included here as pointers only so that we support
 * large string types.
 *
 * Arrays included here as pointers because large arrays will 
 * not fit in this union.
 */
typedef union epics_any{
        epicsInt8       int8;
        epicsUInt8      uInt8;
        epicsInt16      int16;
        epicsUInt16     uInt16;
        epicsEnum16     enum16;
        epicsInt32      int32;
        epicsUInt32     uInt32;
        epicsFloat32    float32;
        epicsFloat64    float64;
	epicsString	string;
}epicsAny;
	
/*
 * Corresponding Type Codes
 * (this enum must start at zero)
 *
 * !! Update epicsTypeToDBR_XXXX[] and DBR_XXXXToEpicsType
 * 	in db_access.h if you edit this enum !!
 */
typedef enum {
                epicsInt8T,
                epicsUInt8T,
                epicsInt16T,
                epicsUInt16T,
                epicsEnum16T,
                epicsInt32T,
                epicsUInt32T,
                epicsFloat32T,
                epicsFloat64T,
                epicsStringT,
                epicsOldStringT
}epicsType;
#define firstEpicsType epicsInt8T
#define lastEpicsType epicsOldStringT 
#define validEpicsType(x) 	((x>=firstEpicsType) && (x<=lastEpicsType))
#define invalidEpicsType(x)	((x<firstEpicsType) || (x>lastEpicsType))


/*
 * The enumeration "epicsType" is an index to this array
 * of type name strings.
 */
#ifdef epicsTypesGLOBAL
epicsShareDef READONLY char *epicsTypeNames [lastEpicsType+1] = {
		"epicsInt8",
		"epicsUInt8",
		"epicsInt16",
		"epicsUInt16",
		"epicsEnum16",
		"epicsInt32",
		"epicsUInt32",
		"epicsFloat32",
		"epicsFloat64",
		"epicsString",
		"epicsOldString",
};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY char *epicsTypeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/*
 * The enumeration "epicsType" is an index to this array
 * of type code name strings.
 */
#ifdef epicsTypesGLOBAL
epicsShareDef READONLY char *epicsTypeCodeNames [lastEpicsType+1] = {
		"epicsInt8T",
		"epicsUInt8T",
		"epicsInt16T",
		"epicsUInt16T",
		"epicsEnum16T",
		"epicsInt32T",
		"epicsUInt32T",
		"epicsFloat32T",
		"epicsFloat64T",
		"epicsStringT",
		"epicsOldStringT",
};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY char *epicsTypeCodeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

#ifdef epicsTypesGLOBAL
epicsShareDef READONLY unsigned epicsTypeSizes [lastEpicsType+1] = {
		sizeof (epicsInt8),
		sizeof (epicsUInt8),
		sizeof (epicsInt16),
		sizeof (epicsUInt16),
		sizeof (epicsEnum16),
		sizeof (epicsInt32),
		sizeof (epicsUInt32),
		sizeof (epicsFloat32),
		sizeof (epicsFloat64),
		sizeof (epicsString),
		sizeof (epicsOldString),
};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY unsigned epicsTypeSizes [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/*
 * The enumeration "epicsType" is an index to this array
 * of type class identifiers.
 */
typedef enum {
	epicsIntC, 
	epicsUIntC, 
	epicsEnumC,
	epicsFloatC, 
	epicsStringC,
	epicsOldStringC} epicsTypeClass;
#ifdef epicsTypesGLOBAL
epicsShareDef READONLY epicsTypeClass epicsTypeClasses [lastEpicsType+1] = {
		epicsIntC,
		epicsUIntC,
		epicsIntC,
		epicsUIntC,
		epicsEnumC,
		epicsIntC,
		epicsUIntC,
		epicsFloatC,
		epicsFloatC,
		epicsStringC,
		epicsOldStringC
	};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY epicsTypeClass epicsTypeClasses [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */


#ifdef epicsTypesGLOBAL
epicsShareDef READONLY char *epicsTypeAnyFieldName [lastEpicsType+1] = {
			"int8",
			"uInt8",
			"int16",
			"uInt16",
			"enum16",
			"int32",
			"uInt32",
			"float32",
			"float64",
			"string",
			"", /* Old Style Strings will not be in epicsAny type */
	};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY char *epicsTypeAnyFieldName [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

#endif /* INCepicsTypesh */

