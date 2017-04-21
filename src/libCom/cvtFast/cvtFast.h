/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Very efficient routines to convert numbers to strings
 *
 *      Author: Bob Dalesio wrote cvtFloatToString (called FF_TO_STR)
 *			Code is same for cvtDoubleToString
 *		Marty Kraimer wrote cvtCharToString,cvtUcharToString
 *			cvtShortToString,cvtUshortToString,
 *			cvtLongToString, and cvtUlongToString
 *		Mark Anderson wrote cvtLongToHexString, cvtLongToOctalString,
 *			adopted cvt[Float/Double]ExpString and
 *			cvt[Float/Double]CompactString from fToEStr
 *			and fixed calls to gcvt
 *      Date:   12-9-92
 */
#ifndef INCcvtFasth
#define INCcvtFasth

#include <string.h>

#include "epicsTypes.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * each of these functions return the number of characters "transmitted"
 *	(as in ANSI-C/POSIX.1/XPG3 sprintf() functions)
 */
epicsShareFunc int epicsShareAPI 
	cvtFloatToString(float value, char *pstring, unsigned short precision);
epicsShareFunc int epicsShareAPI 
	cvtDoubleToString(double value, char *pstring, unsigned short precision);
epicsShareFunc int epicsShareAPI 
	cvtFloatToExpString(float value, char *pstring, unsigned short precision);
epicsShareFunc int epicsShareAPI 
	cvtDoubleToExpString(double value, char *pstring, unsigned short precision);
epicsShareFunc int epicsShareAPI 
	cvtFloatToCompactString(float value, char *pstring, unsigned short precision);
epicsShareFunc int epicsShareAPI 
	cvtDoubleToCompactString(double value, char *pstring, unsigned short precision);
epicsShareFunc int epicsShareAPI 
	cvtCharToString(signed char value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtUcharToString(unsigned char value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtShortToString(short value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtUshortToString(unsigned short value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtLongToString(epicsInt32 value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtUlongToString(epicsUInt32 value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtLongToHexString(epicsInt32 value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtLongToOctalString(epicsInt32 value, char *pstring);
epicsShareFunc epicsUInt32 epicsShareAPI cvtBitsToUlong(
	epicsUInt32  src,
	unsigned bitFieldOffset,
	unsigned  bitFieldLength);
epicsShareFunc epicsUInt32 epicsShareAPI cvtUlongToBits(
	epicsUInt32 src,
	epicsUInt32 dest,
	unsigned      bitFieldOffset,
	unsigned      bitFieldLength);

#ifdef __cplusplus
}
#endif

#endif /*INCcvtFasth*/
