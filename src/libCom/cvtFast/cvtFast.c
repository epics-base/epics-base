/* $Id$*/
/* Very efficient routines to convert numbers to strings
 *      Author: Bob Dalesio wrote cvtFloatToString (called FF_TO_STR)
 *			Code is same for cvtDoubleToString
 *		Marty Kraimer wrote cvtCharToString,cvtUcharToString
 *			cvtShortToString,cvtUshortToString,
 *			cvtLongToString, and cvtUlongToString
 *		Mark Anderson wrote cvtLongToHexString, cvtLongToOctalString,
 *			adopted cvt[Float/Double]ExpString and
 *			cvt[Float/Double]CompactString from fToEStr
 *			and fixed calls to gcvt
 *
 *      Date:            12 January 1993
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
 * .01	mrk	12-09-92	Taken from dbAccess and made into library
 * .02  mda	01-12-93	Add cvt[Float/Double]ToExpString,
 *				cvt[Float/Double]ToCompactString,
 *				cvtLongToHex, cvtLongToOctal routines, fix
 *				calls to gcvt, etc.
 * .03	joh	03-30-93	added bit field extract/ insert routines
 * .04	mrk	01-28-94	replaced gcvt by e conversion
 * .05	mrk	05-06-95	changed ExpString routines to just use sprintf
 *				It had a bug and performance was not that much
 *				better (2-5 times) then sprintf.
 *				Also they are not used on vxWorks
 */

#ifdef vxWorks
#include <vxWorks.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>		/* XPG2/XPG3/POSIX.1/FIPS151-1/ANSI-C */

#define epicsExportSharedSymbols
#include "cvtFast.h"

/*
 * This routine converts numbers less than 10,000,000. It defers to f_to_str for
 * numbers requiring more than 8 places of precision. There are only eight decimal
 */
static long	frac_multiplier[] =
	{1,10,100,1000,10000,100000,1000000,10000000,100000000};

int epicsShareAPI cvtFloatToString(
	float flt_value,
	char  *pstr_value,
	unsigned short precision)
{
        unsigned short	got_one,i;
	long		whole,iplace,number,fraction,fplace;
	float		ftemp;
	char		*startAddr;

	/* can this routine handle this conversion */
	if (precision > 8 || flt_value > 10000000.0 || flt_value < -10000000.0) {
		sprintf(pstr_value,"%12.5e",(double)flt_value);
		return((int)strlen(pstr_value));
	}
	startAddr = pstr_value;

	/* determine the sign */
        if (flt_value < 0){
                *pstr_value++ = '-';
                flt_value = -flt_value;
        };

 	/* remove the whole number portion */
	whole = (long)flt_value;
	ftemp = flt_value - whole;

	/* multiplier to convert fractional portion to integer */
	fplace = frac_multiplier[precision];
	fraction = (long)(ftemp * fplace * 10);	
	fraction = (fraction + 5) / 10;		/* round up */

	/* determine rounding into the whole number portion */
	if ((fraction / fplace) >= 1){
		whole++;
		fraction -= fplace;
	}

	/* whole numbers */
        got_one = 0;
        for (iplace = 10000000; iplace >= 1; iplace /= 10){
                if (whole >= iplace){
                        got_one = 1;
                        number = whole / iplace;
                        whole = whole - (number * iplace);
                        *pstr_value = number + '0';
                        pstr_value++;
                }else if (got_one){
                        *pstr_value = '0';
                        pstr_value++;
                }
        }
        if (!got_one){
                *pstr_value = '0';
                pstr_value++;
        }

        /* fraction */
        if (precision > 0){
		/* convert fractional portional to ASCII */
                *pstr_value = '.';
                pstr_value++;
                for (fplace /= 10, i = precision; i > 0; fplace /= 10,i--){
                        number = fraction / fplace;
                        fraction -= number * fplace;
                        *pstr_value = number + '0';
                        pstr_value++;
                }
        }
        *pstr_value = 0;

        return((int)(pstr_value - startAddr));
}

int epicsShareAPI cvtDoubleToString(
	double flt_value,
	char  *pstr_value,
	unsigned short precision)
{
        unsigned short	got_one,i;
	long		whole,iplace,number,fraction,fplace;
	double		ftemp;
	char		*startAddr;

	/* can this routine handle this conversion */
	if (precision > 8 || flt_value > 10000000.0 || flt_value < -10000000.0) {
		if (precision > 8 || flt_value > 1e16 || flt_value < -1e16) {
		    if(precision>17) precision=17;
		    sprintf(pstr_value,"%*.*e",precision+7,precision,
			flt_value);
		} else {
		    sprintf(pstr_value,"%.0f",flt_value);
		}
		return((int)strlen(pstr_value));
	}
	startAddr = pstr_value;

	/* determine the sign */
        if (flt_value < 0){
                *pstr_value++ = '-';
                flt_value = -flt_value;
        };

 	/* remove the whole number portion */
	whole = (long)flt_value;
	ftemp = flt_value - whole;

	/* multiplier to convert fractional portion to integer */
	fplace = frac_multiplier[precision];
	fraction = (long)(ftemp * fplace * 10);	
	fraction = (fraction + 5) / 10;		/* round up */

	/* determine rounding into the whole number portion */
	if ((fraction / fplace) >= 1){
		whole++;
		fraction -= fplace;
	}

	/* whole numbers */
        got_one = 0;
        for (iplace = 10000000; iplace >= 1; iplace /= 10){
                if (whole >= iplace){
                        got_one = 1;
                        number = whole / iplace;
                        whole = whole - (number * iplace);
                        *pstr_value = number + '0';
                        pstr_value++;
                }else if (got_one){
                        *pstr_value = '0';
                        pstr_value++;
                }
        }
        if (!got_one){
                *pstr_value = '0';
                pstr_value++;
        }

        /* fraction */
        if (precision > 0){
		/* convert fractional portional to ASCII */
                *pstr_value = '.';
                pstr_value++;
                for (fplace /= 10, i = precision; i > 0; fplace /= 10,i--){
                        number = fraction / fplace;
                        fraction -= number * fplace;
                        *pstr_value = number + '0';
                        pstr_value++;
                }
        }
        *pstr_value = 0;

        return((int)(pstr_value - startAddr));
}

/*
 * cvtFloatToExpString
 *
 * converts floating point numbers to E-format NULL terminated strings
 */
int epicsShareAPI cvtFloatToExpString(
  float			f_value,
  char			*pstr_value,
  unsigned short	f_precision)
{
    /*sunos uses char*sprint as function prototype*/
    sprintf(pstr_value,"%.*e",(int)f_precision,(double)f_value);
    return((int)strlen(pstr_value));
}

/*
 * cvtFloatToCompactString
 *
 * Converts floating point numbers to %g format NULL terminated strings,
 * resulting in the most "compact" expression of the value
 * ("f" notation if 10-4 < |value| < 10+4, otherwise "e" notation)
 */
int epicsShareAPI cvtFloatToCompactString(
  float			f_value,
  char			*pstr_value,
  unsigned short	f_precision )
{
  if ((f_value < 1.e4 && f_value > 1.e-4) ||
		(f_value > -1.e4 && f_value < -1.e-4) || f_value == 0.0) {
    return(cvtFloatToString(f_value,pstr_value,f_precision));
  } else {
    return(cvtFloatToExpString(f_value,pstr_value,f_precision));
  }
}



/*
 * cvtDoubleToExpString
 *
 * converts double precision floating point numbers to E-format NULL
 *	terminated strings
 */

int epicsShareAPI cvtDoubleToExpString(
  double		f_value,
  char			*pstr_value,
  unsigned short	f_precision )
{
    sprintf(pstr_value,"%.*e",(int)f_precision,f_value);
    return((int)strlen(pstr_value));
}


/*
 * cvtDoubleToCompactString
 *
 * Converts double precision floating point numbers to %g format NULL
 *	terminated strings, resulting in the most "compact" expression
 *	of the value ("f" notation if 10-4 < |value| < 10+4, otherwise
 *	"e" notation)
 */
int epicsShareAPI cvtDoubleToCompactString(
  double		f_value,
  char			*pstr_value,
  unsigned short	f_precision )
{
  if ((f_value < 1.e4 && f_value > 1.e-4) ||
		(f_value > -1.e4 && f_value < -1.e-4) || f_value == 0.0) {
    return(cvtDoubleToString(f_value,pstr_value,f_precision));
  } else {
    return(cvtDoubleToExpString(f_value,pstr_value,f_precision));
  }
}

/* Convert various integer types to ascii */

static char digit_to_ascii[10]={'0','1','2','3','4','5','6','7','8','9'};

int epicsShareAPI cvtCharToString(
	char source,
	char *pdest)
{
    unsigned char val,temp;
    char	  digit[3];
    int		  i,j;
    char	  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == CHAR_MIN) {
	    sprintf(pdest,"%d",CHAR_MIN);
	    return((int)strlen(pdest));
	}
	*pdest++ = '-';
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}


int epicsShareAPI cvtUcharToString(
    unsigned char source,
    char	  *pdest)
{
    unsigned char val,temp;
    char	  digit[3];
    int		  i,j;
    char	  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}


int epicsShareAPI cvtShortToString(
    short source,
    char  *pdest)
{
    short val,temp;
    char  digit[6];
    int	  i,j;
    char  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == SHRT_MIN) {
	    sprintf(pdest,"%d",SHRT_MIN);
	    return((int)(strlen(pdest)));
	}
	*pdest++ = '-';
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}


int epicsShareAPI cvtUshortToString(
    unsigned short source,
    char	  *pdest)
{
    unsigned short val,temp;
    char	  digit[5];
    int		  i,j;
    char	  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}


int epicsShareAPI cvtLongToString(
    long source,
    char  *pdest)
{
    long  val,temp;
    char  digit[11];
    int	  i,j;
    char  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == LONG_MIN) {
	    sprintf(pdest,"%ld",LONG_MIN);
	    return((int)strlen(pdest));
	}
	*pdest++ = '-';
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}


int epicsShareAPI cvtUlongToString(
    unsigned long source,
    char	  *pdest)
{
    unsigned long val,temp;
    char	  digit[10];
    int		  i,j;
    char	  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/10;
	digit[i] = digit_to_ascii[val - temp*10];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}


/* Convert hex digits to ascii */

static char hex_digit_to_ascii[16]={'0','1','2','3','4','5','6','7','8','9',
		'a','b','c','d','e','f'};


int epicsShareAPI cvtLongToHexString(
    long source,
    char  *pdest)
{
    long  val,temp;
    char  digit[10];
    int	  i,j;
    char  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == LONG_MIN) {
	    sprintf(pdest,"%x",LONG_MIN);
	    return((int)strlen(pdest));
	}
	*pdest++ = '-';
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/16;
	digit[i] = hex_digit_to_ascii[val - temp*16];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}


int epicsShareAPI cvtLongToOctalString(
    long source,
    char  *pdest)
{
    long  val,temp;
    char  digit[16];
    int	  i,j;
    char  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == LONG_MIN) {
	    sprintf(pdest,"%o",LONG_MIN);
	    return((int)strlen(pdest));
	}
	*pdest++ = '-';
	source = -source;
    }
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/8;
	/* reuse digit_to_ascii since octal is a subset of decimal */
	digit[i] = digit_to_ascii[val - temp*8];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return((int)(pdest-startAddr));
}




/*
 *
 * cvtBitsToUlong()
 *
 * extract a bit field from the source unsigend long
 */
unsigned long epicsShareAPI cvtBitsToUlong(
unsigned long   src,
unsigned        bitFieldOffset,
unsigned        bitFieldLength)
{
        unsigned long   mask;

        src = src >> bitFieldOffset;

        mask = (1<<bitFieldLength)-1;

        src = src & mask;

        return src;
}



/*
 *
 * cvtUlongToBits()
 *
 * insert a bit field from the source unsigend long
 * into the destination unsigned long
 */
unsigned long epicsShareAPI cvtUlongToBits(
unsigned long   src,
unsigned long   dest,
unsigned        bitFieldOffset,
unsigned        bitFieldLength)
{
        unsigned long   mask;

        mask = (1<<bitFieldLength)-1;
        mask = mask << bitFieldOffset;
        src = src << bitFieldOffset;
        dest = (dest & ~mask) | (src & mask);

        return dest;
}
