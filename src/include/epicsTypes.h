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
 * Revision 1.7  1995/12/19 19:46:44  jhill
 * added epicsStatus typedef
 *
 * Revision 1.6  1995/09/29  21:41:41  jhill
 * added use of sbufs
 *
 */

#ifndef INCepicsTypesh
#define INCepicsTypesh 1

#ifdef __STDC__
#	define READONLY const
#else
#	define READONLY 
#endif

#include <stdio.h>

#include <shareLib.h>
#include <sbufLib.h>

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

typedef struct {
	epicsIndex	first;
	epicsIndex	count;
}epicsArrayIndex;

/*
 * Arrays are stored in a stored in a shared buffer 
 * in order to avoid redundant copying. See "sbufLib.h".
 *
 * The first segment in a shared buffer containing an
 * EPICS array will always contain only the 
 * "epicsArrayDescriptor" described below. Subsequent
 * segments will contain the array data (in C language
 * dimension order).
 *
 * Segments in the shared buffer must end on natural 
 * boundaries for the element type stored in the array.
 *
 * nDim - the number of dimensions. nDim==0 => scaler
 * pDim - pointer to an array of lengths for each dimension
 *
 */
typedef SBUFID		epicsArray;
typedef struct {
	unsigned 	nDim;	/* the number of dimensions */
	epicsIndex	*pDim;	/* array of dimension lengths */
}epicsArrayDescriptor;

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
	epicsArray	array;
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
READONLY char *epicsTypeNames [lastEpicsType+1] = {
		stringOf (epicsInt8),
		stringOf (epicsUInt8),
		stringOf (epicsInt16),
		stringOf (epicsUInt16),
		stringOf (epicsEnum16),
		stringOf (epicsInt32),
		stringOf (epicsUInt32),
		stringOf (epicsFloat32),
		stringOf (epicsFloat64),
		stringOf (epicsString),
		stringOf (epicsOldString),
};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY char *epicsTypeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/*
 * The enumeration "epicsType" is an index to this array
 * of type code name strings.
 */
#ifdef epicsTypesGLOBAL
READONLY char *epicsTypeCodeNames [lastEpicsType+1] = {
		stringOf (epicsInt8T),
		stringOf (epicsUInt8T),
		stringOf (epicsInt16T),
		stringOf (epicsUInt16T),
		stringOf (epicsEnum16T),
		stringOf (epicsInt32T),
		stringOf (epicsUInt32T),
		stringOf (epicsFloat32T),
		stringOf (epicsFloat64T),
		stringOf (epicsStringT),
		stringOf (epicsOldStringT),
};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY char *epicsTypeCodeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

#ifdef epicsTypesGLOBAL
READONLY unsigned epicsTypeSizes [lastEpicsType+1] = {
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
READONLY epicsTypeClass epicsTypeClasses [lastEpicsType+1] = {
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
READONLY char *epicsTypeAnyFieldName [lastEpicsType+1] = {
			stringOf (int8),
			stringOf (uInt8),
			stringOf (int16),
			stringOf (uInt16),
			stringOf (enum16),
			stringOf (int32),
			stringOf (uInt32),
			stringOf (float32),
			stringOf (float64),
			stringOf (string),
			"", /* Old Style Strings will not be in epicsAny type */
	};
#else /* epicsTypesGLOBAL */
epicsShareExtern READONLY char *epicsTypeAnyFieldName [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

#endif /* INCepicsTypesh */

