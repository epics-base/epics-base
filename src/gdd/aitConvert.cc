
// Author: Jim Kowalkowski
// Date: 2/96
// 
// $Id$
//
// $Log$
//
// *Revision 0.4  1996/06/25 18:58:58  jbk
// *more fixes for the aitString management functions and mapping menus
// *Revision 0.3  1996/06/24 03:15:28  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 0.2  1996/06/17 15:24:04  jbk
// *many mods, string class corrections.
// *gdd operator= protection.
// *dbmapper uses aitString array for menus now
// *Revision 0.1  1996/05/31 13:15:17  jbk
// *add new stuff

#define AIT_CONVERT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include "aitConvert.h"

extern "C" {
void aitNoConvert(void* /*dest*/,const void* /*src*/,aitIndex /*count*/) { }
}

#ifdef AIT_CONVERT
#undef AIT_CONVERT
#endif
#ifdef AIT_TO_NET_CONVERT
#undef AIT_TO_NET_CONVERT
#endif
#ifdef AIT_FROM_NET_CONVERT
#undef AIT_FROM_NET_CONVERT
#endif

/* put the fixed conversion functions here (ones not generated) */

/* ------- extra string conversion functions --------- */
static void aitConvertStringString(void* d,const void* s,aitIndex c)
{
	// does not work - need to be fixed
	int i;
	aitString *in=(aitString*)s, *out=(aitString*)d;

	for(i=0;i<c;i++) out[i]=in[i];
}
static void aitConvertToNetStringString(void* d,const void* s,aitIndex c)
{ aitConvertStringString(d,s,c); }
static void aitConvertFromNetStringString(void* d,const void* s,aitIndex c)
{ aitConvertStringString(d,s,c); }

/* ------ all the fixed string conversion functions ------ */
static void aitConvertFixedStringFixedString(void* d,const void* s,aitIndex c)
{
	aitUint32 len = c*AIT_FIXED_STRING_SIZE;
	memcpy(d,s,len);
}
static void aitConvertToNetFixedStringFixedString(void* d,const void* s,aitIndex c)
{ aitConvertFixedStringFixedString(d,s,c); }
static void aitConvertFromNetFixedStringFixedString(void* d,const void* s,aitIndex c)
{ aitConvertFixedStringFixedString(d,s,c); }

static void aitConvertStringFixedString(void* d,const void* s,aitIndex c)
{
	int i;
	aitString* out = (aitString*)d;
	aitFixedString* in = (aitFixedString*)s;

	for(i=0;i<c;i++) out[i].installString(in[i].fixed_string);
}

static void aitConvertFixedStringString(void* d,const void* s,aitIndex c)
{
	int i;
	aitString* in = (aitString*)s;
	aitFixedString* out = (aitFixedString*)d;

	for(i=0;i<c;i++) strcpy(out[i].fixed_string,in[i].string());
}

static void aitConvertToNetStringFixedString(void* d,const void* s,aitIndex c)
{ aitConvertStringFixedString(d,s,c); }
static void aitConvertFromNetFixedStringString(void* d,const void* s,aitIndex c)
{ aitConvertFixedStringString(d,s,c); }
static void aitConvertToNetFixedStringString(void* d,const void* s,aitIndex c)
{ aitConvertStringFixedString(d,s,c); }
static void aitConvertFromNetStringFixedString(void* d,const void* s,aitIndex c)
{ aitConvertFixedStringString(d,s,c); }

#define AIT_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_CONVERT

/* include the network byte order functions if needed */
#ifdef AIT_NEED_BYTE_SWAP

#define AIT_TO_NET_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_TO_NET_CONVERT

#define AIT_FROM_NET_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_FROM_NET_CONVERT

#endif

