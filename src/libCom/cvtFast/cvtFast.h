/* $Id$
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
 * .01  mrk	12-09-92	Taken form dbAccess and made into library
 * .02  mda	01-12-92	New routines, cleanup 
 * .03	joh	03-30-93	Added bit field extract/insert
 * .04	mrk	07-22-93	Support both old C and ANSI C
 */
#ifndef INCcvtFasth
#define INCcvtFasth

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "shareLib.h"

#if defined(__STDC__) || defined(__cplusplus)

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
	cvtCharToString(char value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtUcharToString(unsigned char value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtShortToString(short value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtUshortToString(unsigned short value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtLongToString(long value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtUlongToString(unsigned long value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtLongToHexString(long value, char *pstring);
epicsShareFunc int epicsShareAPI 
	cvtLongToOctalString(long value, char *pstring);
epicsShareFunc unsigned long epicsShareAPI cvtBitsToUlong(
	unsigned long  src,
	unsigned bitFieldOffset,
	unsigned  bitFieldLength);
epicsShareFunc unsigned long epicsShareAPI cvtUlongToBits(
	unsigned long src,
	unsigned long dest,
	unsigned      bitFieldOffset,
	unsigned      bitFieldLength);

#else /*__STDC__*/

epicsShareFunc int epicsShareAPI cvtFloatToString();
epicsShareFunc int epicsShareAPI cvtDoubleToString();
epicsShareFunc int epicsShareAPI cvtFloatToExpString();
epicsShareFunc int epicsShareAPI cvtDoubleToExpString();
epicsShareFunc int epicsShareAPI cvtFloatToCompactString();
epicsShareFunc int epicsShareAPI cvtDoubleToCompactString();
epicsShareFunc int epicsShareAPI cvtCharToString();
epicsShareFunc int epicsShareAPI cvtUcharToString();
epicsShareFunc int epicsShareAPI cvtShortToString();
epicsShareFunc int epicsShareAPI cvtUshortToString();
epicsShareFunc int epicsShareAPI cvtLongToString();
epicsShareFunc int epicsShareAPI cvtUlongToString();
epicsShareFunc int epicsShareAPI cvtLongToHexString();
epicsShareFunc int epicsShareAPI cvtLongToOctalString();
epicsShareFunc unsigned long epicsShareAPI cvtBitsToUlong();
epicsShareFunc unsigned long epicsShareAPI cvtUlongToBits();

#endif /*__STDC__*/

#ifdef __cplusplus
}
#endif

#endif /*INCcvtFasth*/
