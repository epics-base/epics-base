
// Author: Jim Kowalkowski
// Date: 2/96
// 
// $Id$
//
// $Log$
// Revision 1.6  1997/08/05 00:51:03  jhill
// fixed problems in aitString and the conversion matrix
//
// Revision 1.5  1997/04/23 17:12:53  jhill
// fixed export of symbols from WIN32 DLL
//
// Revision 1.4  1996/11/02 01:24:39  jhill
// strcpy => styrcpy (shuts up purify)
//
// Revision 1.3  1996/08/22 21:05:37  jbk
// More fixes to make strings and fixed string work better.
//
// Revision 1.2  1996/08/13 15:07:42  jbk
// changes for better string manipulation and fixes for the units field
//
// Revision 1.1  1996/06/25 19:11:28  jbk
// new in EPICS base
//
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
#define epicsExportSharedSymbols
#include "aitConvert.h"

int aitNoConvert(void* /*dest*/,const void* /*src*/,aitIndex /*count*/) {return -1;}

#ifdef AIT_CONVERT
#undef AIT_CONVERT
#endif
#ifdef AIT_TO_NET_CONVERT
#undef AIT_TO_NET_CONVERT
#endif
#ifdef AIT_FROM_NET_CONVERT
#undef AIT_FROM_NET_CONVERT
#endif

#ifndef min
#define min(A,B) ((A)<(B)?(A):(B))
#endif

/* put the fixed conversion functions here (ones not generated) */

/* ------- extra string conversion functions --------- */
static int aitConvertStringString(void* d,const void* s,aitIndex c)
{
	// does not work - need to be fixed
	aitIndex i;
	aitString *in=(aitString*)s, *out=(aitString*)d;

	for(i=0;i<c;i++) out[i]=in[i];
	return 0;
}
static int aitConvertToNetStringString(void* d,const void* s,aitIndex c)
{ return aitConvertStringString(d,s,c);}
static int aitConvertFromNetStringString(void* d,const void* s,aitIndex c)
{ return aitConvertStringString(d,s,c);}

/* ------ all the fixed string conversion functions ------ */
static int aitConvertFixedStringFixedString(void* d,const void* s,aitIndex c)
{
	aitUint32 len = c*AIT_FIXED_STRING_SIZE;
	memcpy(d,s,len);
	return 0;
}
static int aitConvertToNetFixedStringFixedString(void* d,const void* s,aitIndex c)
{ return aitConvertFixedStringFixedString(d,s,c);}
static int aitConvertFromNetFixedStringFixedString(void* d,const void* s,aitIndex c)
{ return aitConvertFixedStringFixedString(d,s,c);}

static int aitConvertStringFixedString(void* d,const void* s,aitIndex c)
{
	aitIndex i;
	aitString* out = (aitString*)d;
	aitFixedString* in = (aitFixedString*)s;

	for(i=0;i<c;i++) out[i].copy(in[i].fixed_string);
	return 0;
}

static int aitConvertFixedStringString(void* d,const void* s,aitIndex c)
{
	aitIndex i;
	aitString* in = (aitString*)s;
	aitFixedString* out = (aitFixedString*)d;

	//
	// joh - changed this from strcpy() to stncpy() in order to:
	// 1) shut up purify 
	// 2) guarantee that all fixed strings will be terminated
	// 3) guarantee that we will not overflow a fixed string
	//
	for(i=0;i<c;i++){
		strncpy(out[i].fixed_string,in[i].string(),AIT_FIXED_STRING_SIZE);
		out[i].fixed_string[AIT_FIXED_STRING_SIZE-1u] = '\0';
	}
	return 0;
}

static int aitConvertToNetStringFixedString(void* d,const void* s,aitIndex c)
{ return aitConvertStringFixedString(d,s,c); }
static int aitConvertFromNetFixedStringString(void* d,const void* s,aitIndex c)
{ return aitConvertFixedStringString(d,s,c); }
static int aitConvertToNetFixedStringString(void* d,const void* s,aitIndex c)
{ return aitConvertStringFixedString(d,s,c); }
static int aitConvertFromNetStringFixedString(void* d,const void* s,aitIndex c)
{ return aitConvertFixedStringString(d,s,c); }

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

