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

#ifdef __STDC__

/*
 * each of these functions return the number of characters "transmitted"
 *	(as in ANSI-C/POSIX.1/XPG3 sprintf() functions)
 */
int cvtFloatToString(float value, char *pstring, unsigned short precision);
int cvtDoubleToString(double value, char *pstring, unsigned short precision);
int cvtFloatToExpString(float value, char *pstring, unsigned short precision);
int cvtDoubleToExpString(double value, char *pstring, unsigned short precision);
int cvtFloatToCompactString(float value, char *pstring, unsigned short precision);
int cvtDoubleToCompactString(double value, char *pstring, unsigned short precision);
int cvtCharToString(char value, char *pstring);
int cvtUcharToString(unsigned char value, char *pstring);
int cvtShortToString(short value, char *pstring);
int cvtUshortToString(unsigned short value, char *pstring);
int cvtLongToString(long value, char *pstring);
int cvtUlongToString(unsigned long value, char *pstring);
int cvtLongToHexString(long value, char *pstring);
int cvtLongToOctalString(long value, char *pstring);
unsigned long cvtBitsToUlong(
	unsigned long  src,
	unsigned bitFieldOffset,
	unsigned  bitFieldLength);
unsigned long cvtUlongToBits(
	unsigned long src,
	unsigned long dest,
	unsigned      bitFieldOffset,
	unsigned      bitFieldLength);

#else /*__STDC__*/

int cvtFloatToString();
int cvtDoubleToString();
int cvtFloatToExpString();
int cvtDoubleToExpString();
int cvtFloatToCompactString();
int cvtDoubleToCompactString();
int cvtCharToString();
int cvtUcharToString();
int cvtShortToString();
int cvtUshortToString();
int cvtLongToString();
int cvtUlongToString();
int cvtLongToHexString();
int cvtLongToOctalString();
unsigned long cvtBitsToUlong();
unsigned long cvtUlongToBits();

#endif /*__STDC__*/

#ifdef __cplusplus
}
#endif

#endif /*INCcvtFasth*/
