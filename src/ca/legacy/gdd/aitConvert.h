/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef AIT_CONVERT_H__
#define AIT_CONVERT_H__

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 */

#include <sys/types.h>

#if defined(_MSC_VER) && _MSC_VER < 1200
#   pragma warning ( push )
#   pragma warning ( disable:4786 )
#endif

#include "shareLib.h"
#include "osiSock.h"

#include "aitTypes.h"
#include "gddEnumStringTable.h"

#if defined(__i386) || defined(i386)
#define AIT_NEED_BYTE_SWAP 1
#endif

#ifdef AIT_NEED_BYTE_SWAP
#define aitLocalNetworkDataFormatSame 0
#else
#define aitLocalNetworkDataFormatSame 1
#endif

typedef enum { aitLocalDataFormat=0, aitNetworkDataFormat } aitDataFormat;

/* all conversion functions have this prototype */
typedef int (*aitFunc)(void* dest,const void* src,aitIndex count,const gddEnumStringTable *pEnumStringTable);

#ifdef __cplusplus
extern "C" {
#endif

/* main conversion table */
epicsShareExtern aitFunc aitConvertTable[aitTotal][aitTotal];
/* do not make conversion table if not needed */
#ifdef AIT_NEED_BYTE_SWAP
epicsShareExtern aitFunc aitConvertToNetTable[aitTotal][aitTotal];
epicsShareExtern aitFunc aitConvertFromNetTable[aitTotal][aitTotal];
#else
#define aitConvertToNetTable	aitConvertTable
#define aitConvertFromNetTable	aitConvertTable
#endif /* AIT_NEED_BYTE_SWAP */

#ifdef __cplusplus
}
#endif

/* ---------- convenience routines for performing conversions ---------- */

#if defined(__cplusplus)

inline int aitConvert(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count, 
 const gddEnumStringTable *pEnumStringTable = 0 )
  { return (*aitConvertTable[desttype][srctype])(dest,src,count,pEnumStringTable); }

inline int aitConvertToNet(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count, 
 const gddEnumStringTable *pEnumStringTable = 0 )
  { return (*aitConvertToNetTable[desttype][srctype])(dest,src,count,pEnumStringTable); }

inline int aitConvertFromNet(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count, 
 const gddEnumStringTable *pEnumStringTable = 0 )
  { return (*aitConvertFromNetTable[desttype][srctype])(dest,src,count,pEnumStringTable); }

#else

#define aitConvert(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  (*aitConvertTable[DESTTYPE][SRCTYPE])(DEST,SRC,COUNT)

#define aitConvertToNet(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  (*aitConvertToNetTable[DESTTYPE][SRCTYPE])(DEST,SRC,COUNT)

#define aitConvertFromNet(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  (*aitConvertFromNetTable[DESTTYPE][SRCTYPE])(DEST,SRC,COUNT)

#endif

/* ----------- byte order conversion definitions ------------ */

#define aitUint64 aitUint32

#if defined(__cplusplus)

inline void aitToNetOrder16(aitUint16* dest,aitUint16* src)
	{ *dest=htons(*src); }
inline void aitToNetOrder32(aitUint32* dest,aitUint32* src)
	{ *dest=htonl(*src); }
inline void aitFromNetOrder16(aitUint16* dest,aitUint16* src)
	{ *dest=ntohs(*src); }
inline void aitFromNetOrder32(aitUint32* dest,aitUint32* src)
	{ *dest=ntohl(*src); }
inline void aitToNetOrder64(aitUint64* dest, aitUint64* src)
{
	aitUint32 ait_temp_var_;
	ait_temp_var_=(aitUint32)htonl(src[1]);
	dest[1]=(aitUint32)htonl(src[0]);
	dest[0]=ait_temp_var_;
}
inline void aitFromNetOrder64(aitUint64* dest, aitUint64* src)
{
	aitUint32 ait_temp_var_;
	ait_temp_var_=(aitUint32)ntohl(src[1]);
	dest[1]=(aitUint32)ntohl(src[0]);
	dest[0]=ait_temp_var_;
}

#else

/* !The following generate code! */
#define aitToNetOrder16(dest,src)	(*(dest)=htons(*(src)))
#define aitToNetOrder32(dest,src)	(*(dest)=htonl(*(src)))
#define aitFromNetOrder16(dest,src)	(*(dest)=ntohs(*(src)))
#define aitFromNetOrder32(dest,src)	(*(dest)=ntohl(*(src)))

/* be careful using these because of brackets, these should be functions */
#define aitToNetOrder64(dest,src) \
{ \
	aitUint32 ait_temp_var_; \
	ait_temp_var_=(aitUint32)htonl((src)[1]); \
	(dest)[1]=(aitUint32)htonl((src)[0]); \
	(dest)[0]=ait_temp_var_; \
}
#define aitFromNetOrder64(dest,src) \
{ \
	aitUint32 ait_temp_var_; \
	ait_temp_var_=(aitUint32)ntohl((src)[1]); \
	(dest)[1]=(aitUint32)ntohl((src)[0]); \
	(dest)[0]=ait_temp_var_; \
}

#endif /* __cpluspluc */

#define aitToNetFloat64 aitToNetOrder64
#define aitToNetFloat32 aitToNetOrder32
#define aitFromNetFloat64 aitFromNetOrder64
#define aitFromNetFloat32 aitFromNetOrder32

bool getStringAsDouble ( const char * pString, 
    const gddEnumStringTable *pEST, double & result );

bool putDoubleToString ( 
    const double in, const gddEnumStringTable * pEST, 
    char * pString, size_t strSize );

#if defined ( _MSC_VER ) && _MSC_VER < 1200
#   pragma warning ( pop )
#endif

#endif

