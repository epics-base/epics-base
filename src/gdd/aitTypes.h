#ifndef AIT_TYPES_H
#define AIT_TYPES_H 1

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.2  1996/08/22 21:05:40  jbk
 * More fixes to make strings and fixed string work better.
 *
 * Revision 1.1  1996/06/25 19:11:33  jbk
 * new in EPICS base
 *
 *
 * *Revision 1.2  1996/06/17 15:24:07  jbk
 * *many mods, string class corrections.
 * *gdd operator= protection.
 * *dbmapper uses aitString array for menus now
 * *Revision 1.1  1996/05/31 13:15:22  jbk
 * *add new stuff
 *
 */

/* This is the file the user sets up for a given architecture */

#define AIT_FIXED_STRING_SIZE 40

#include <sys/types.h>
#include <string.h>

typedef char			aitInt8;
typedef unsigned char	aitUint8;
typedef short			aitInt16;
typedef unsigned short	aitUint16;
typedef aitUint16		aitEnum16;
typedef int             aitInt32;
typedef unsigned int 	aitUint32;
typedef float			aitFloat32;
typedef double			aitFloat64;
typedef	aitUint32		aitIndex;
typedef	void*			aitPointer;
typedef aitUint32		aitStatus;

/* should the bool be added as a conversion type? it currently is not */
typedef enum {
	aitFalse=0,
	aitTrue
} aitBool;

typedef struct {
	char fixed_string[AIT_FIXED_STRING_SIZE];
} aitFixedString;

#ifdef __cplusplus
#include "aitHelpers.h"
#else
/* need time stamp structure different from posix unfortunetly */
typedef struct {
	aitUint32 tv_sec;
	aitUint32 tv_nsec;
} aitTimeStamp;

/* strings are a struct so they are different then aitInt8 */
typedef	struct {
	char* string;
	aitUint32 len;
} aitString;
#endif

#ifndef vxWorks
#if (_POSIX_C_SOURCE < 3) && !defined(solaris) && !defined(SOLARIS)
struct timespec
{
	time_t tv_sec;
	long tv_nsec;
};
typedef struct timespec timespec;
#endif
#endif

/* all normal types */
#define aitTotal 13
#define aitFirst aitEnumInvalid
#define aitLast aitEnumContainer
#define aitValid(x) ((x)<=aitLast && (x)>aitFirst)

/* all conversion types */
#define aitConvertTotal 11
#define aitConvertFirst aitEnumInt8
#define aitConvertLast aitEnumString
#define aitConvertAutoFirst aitEnumInt8
#define aitConvertAutoLast aitEnumFloat64
#define aitConvertValid(x) ((x)>=aitConvertFirst && (x)<=aitConvertLast)

/* currently no 64-bit integer support */
typedef enum {
	aitEnumInvalid=0,
	aitEnumInt8,
	aitEnumUint8,
	aitEnumInt16,
	aitEnumUint16,
	aitEnumEnum16,
	aitEnumInt32,
	aitEnumUint32,
	aitEnumFloat32,
	aitEnumFloat64,
	aitEnumFixedString,
	aitEnumString,
	aitEnumContainer
} aitEnum;

typedef union {
	aitInt8		Int8;
	aitUint8	Uint8;
	aitInt16	Int16;
	aitUint16	Uint16;
	aitEnum16	Enum16;
	aitInt32	Int32;
	aitUint32	Uint32;
	aitFloat32	Float32;
	aitFloat64	Float64;
	aitIndex	Index;
	aitPointer	Pointer;
	aitFixedString* FString;
	aitUint8	Dumb1[sizeof(aitString)];		/* aitString String; */
	aitUint8	Dumb3[sizeof(aitTimeStamp)];	/* aitTimeStamp	Stamp; */
} aitType;

/*
 classes are not allowed in union that required construction/destruction
 so I am insuring that the size of aitType is large enough to hold
 strings and timestamps in an obsure, horrible way with the Dumb variables
*/

#ifndef AIT_TYPES_SOURCE
#ifdef __cplusplus
extern "C" {
#endif
extern const size_t aitSize[aitTotal];
extern const char*  aitName[aitTotal];
extern const char*  aitStringType[aitTotal];
#ifdef __cplusplus
}
#endif
#endif

#endif
