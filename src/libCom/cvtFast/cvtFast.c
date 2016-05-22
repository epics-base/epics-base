/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>		/* XPG2/XPG3/POSIX.1/FIPS151-1/ANSI-C */

#define epicsExportSharedSymbols
#include "cvtFast.h"
#include "epicsMath.h"

/*
 * This routine converts numbers less than 10,000,000. It defers to f_to_str for
 * numbers requiring more than 8 places of precision. There are only eight decimal
 */
static epicsInt32	frac_multiplier[] =
	{1,10,100,1000,10000,100000,1000000,10000000,100000000};

int epicsShareAPI cvtFloatToString(
	float flt_value,
	char  *pstr_value,
	unsigned short precision)
{
        unsigned short	got_one,i;
	epicsInt32		whole,iplace,number,fraction,fplace;
	float		ftemp;
	char		*startAddr;

	/* can this routine handle this conversion */
	if (isnan(flt_value) || precision > 8 || flt_value > 10000000.0 || flt_value < -10000000.0) {
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
	whole = (epicsInt32)flt_value;
	ftemp = flt_value - whole;

	/* multiplier to convert fractional portion to integer */
	fplace = frac_multiplier[precision];
	fraction = (epicsInt32)(ftemp * fplace * 10);	
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
	epicsInt32		whole,iplace,number,fraction,fplace;
	double		ftemp;
	char		*startAddr;

	/* can this routine handle this conversion */
	if (isnan(flt_value) || precision > 8 || flt_value > 10000000.0 || flt_value < -10000000.0) {
		if (precision > 8 || flt_value > 1e16 || flt_value < -1e16) {
		    if(precision>17) precision=17;
		    sprintf(pstr_value,"%*.*e",precision+7,precision,
			flt_value);
		} else {
		    if(precision>3) precision=3;
		    sprintf(pstr_value,"%.*f",precision,flt_value);
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
	whole = (epicsInt32)flt_value;
	ftemp = flt_value - whole;

	/* multiplier to convert fractional portion to integer */
	fplace = frac_multiplier[precision];
	fraction = (epicsInt32)(ftemp * fplace * 10);	
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
	signed char source,
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
    epicsInt32 source,
    char  *pdest)
{
    epicsInt32  val,temp;
    char  digit[11];
    int	  i,j;
    char  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == INT_MIN) {
	    sprintf(pdest,"%d",source);
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
    epicsUInt32 source,
    char	  *pdest)
{
    epicsUInt32 val,temp;
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
    epicsInt32 source,
    char  *pdest)
{
    epicsInt32  val,temp;
    char  digit[10];
    int	  i,j;
    char  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == INT_MIN) {
	    sprintf(pdest,"-0x%x",source);
	    return((int)strlen(pdest));
	}
	*pdest++ = '-';
	source = -source;
    }
    *pdest++ = '0'; *pdest++ = 'x';
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
    epicsInt32 source,
    char  *pdest)
{
    epicsInt32  val,temp;
    char  digit[16];
    int	  i,j;
    char  *startAddr = pdest;

    if(source==0) {
	*pdest++ = '0';
	*pdest = 0;
	return((int)(pdest-startAddr));
    }
    if(source<0) {
	if(source == INT_MIN) {
	    sprintf(pdest,"-0%o",source);
	    return((int)strlen(pdest));
	}
	*pdest++ = '-';
	source = -source;
    }
    *pdest++ = '0';
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
 * extract a bit field from the source epicsUInt32
 */
epicsUInt32 epicsShareAPI cvtBitsToUlong(
epicsUInt32   src,
unsigned        bitFieldOffset,
unsigned        bitFieldLength)
{
        epicsUInt32   mask;

        src = src >> bitFieldOffset;

        mask = (1<<bitFieldLength)-1;

        src = src & mask;

        return src;
}



/*
 *
 * cvtUlongToBits()
 *
 * insert a bit field from the source epicsUInt32
 * into the destination epicsUInt32
 */
epicsUInt32 epicsShareAPI cvtUlongToBits(
epicsUInt32   src,
epicsUInt32   dest,
unsigned        bitFieldOffset,
unsigned        bitFieldLength)
{
        epicsUInt32   mask;

        mask = (1<<bitFieldLength)-1;
        mask = mask << bitFieldOffset;
        src = src << bitFieldOffset;
        dest = (dest & ~mask) | (src & mask);

        return dest;
}
