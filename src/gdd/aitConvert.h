#ifndef AIT_CONVERT_H__
#define AIT_CONVERT_H__

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.2  1996/08/13 23:13:34  jhill
 * win NT changes
 *
 * Revision 1.1  1996/06/25 19:11:29  jbk
 * new in EPICS base
 *
 *
 * *Revision 1.1  1996/05/31 13:15:18  jbk
 * *add new stuff
 *
 */

#include <sys/types.h>

#ifdef WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

#include "aitTypes.h"

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
typedef void (*aitFunc)(void* dest,const void* src,aitIndex count);

#ifdef __cplusplus
extern "C" {
#endif

/* main conversion table */
extern aitFunc aitConvertTable[aitTotal][aitTotal];
/* do not make conversion table if not needed */
#ifdef AIT_NEED_BYTE_SWAP
extern aitFunc aitConvertToNetTable[aitTotal][aitTotal];
extern aitFunc aitConvertFromNetTable[aitTotal][aitTotal];
#else
#define aitConvertToNetTable	aitConvertTable
#define aitConvertFromNetTable	aitConvertTable
#endif /* AIT_NEED_BYTE_SWAP */

#ifdef __cplusplus
}
#endif

/* ---------- convenience routines for performing conversions ---------- */

#if defined(__cplusplus) && !defined(__GNUC__)

inline void aitConvert(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count)
  { aitConvertTable[desttype][srctype](dest,src,count); }

inline void aitConvertToNet(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count)
  { aitConvertToNetTable[desttype][srctype](dest,src,count); }

inline void aitConvertFromNet(aitEnum desttype, void* dest,
 aitEnum srctype, const void* src, aitIndex count)
  { aitConvertFromNetTable[desttype][srctype](dest,src,count); }

#else

#define aitConvert(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  aitConvertTable[DESTTYPE][SRCTYPE](DEST,SRC,COUNT)

#define aitConvertToNet(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  aitConvertToNetTable[DESTTYPE][SRCTYPE](DEST,SRC,COUNT)

#define aitConvertFromNet(DESTTYPE,DEST,SRCTYPE,SRC,COUNT) \
  aitConvertFromNetTable[DESTTYPE][SRCTYPE](DEST,SRC,COUNT)

#endif

/* ----------- byte order conversion definitions ------------ */

#define aitUint64 aitUint32

#if defined(__cplusplus) && !defined(__GNUC__)

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

#endif /* __cpluspluc && !__GNUC__ */

#define aitToNetFloat64 aitToNetOrder64
#define aitToNetFloat32 aitToNetOrder32
#define aitFromNetFloat64 aitFromNetOrder64
#define aitFromNetFloat32 aitFromNetOrder32

#endif

