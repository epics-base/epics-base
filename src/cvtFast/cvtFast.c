/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Fast numeric to string conversions
 *
 * Original Authors:
 *    Bob Dalesio, Mark Anderson and Marty Kraimer
 *    Date:            12 January 1993
 */

#include <string.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "cvtFast.h"
#include "epicsMath.h"
#include "epicsStdio.h"

/*
 * These routines convert numbers up to +/- 10,000,000.
 * They defer to sprintf() for numbers requiring more than
 * 8 places of precision.
 */
static epicsInt32 frac_multiplier[] =
    {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};

int cvtFloatToString(float flt_value, char *pdest,
    epicsUInt16 precision)
{
        int got_one, i;
	epicsInt32 whole, iplace, number, fraction, fplace;
	float		ftemp;
	char		*startAddr;

	/* can this routine handle this conversion */
	if (isnan(flt_value) || precision > 8 ||
	    flt_value > 10000000.0 || flt_value < -10000000.0) {
		if (precision > 8 || flt_value >= 1e8 || flt_value <= -1e8) {
		    if (precision > 12) precision = 12; /* FIXME */
		    sprintf(pdest, "%*.*e", precision+6, precision, (double) flt_value);
		} else {
		    if (precision > 3) precision = 3; /* FIXME */
		    sprintf(pdest, "%.*f", precision, (double) flt_value);
		}
		return((int)strlen(pdest));
	}
	startAddr = pdest;

	/* determine the sign */
        if (flt_value < 0){
                *pdest++ = '-';
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
                        *pdest = number + '0';
                        pdest++;
                }else if (got_one){
                        *pdest = '0';
                        pdest++;
                }
        }
        if (!got_one){
                *pdest = '0';
                pdest++;
        }

        /* fraction */
        if (precision > 0){
		/* convert fractional portional to ASCII */
                *pdest = '.';
                pdest++;
                for (fplace /= 10, i = precision; i > 0; fplace /= 10,i--){
                        number = fraction / fplace;
                        fraction -= number * fplace;
                        *pdest = number + '0';
                        pdest++;
                }
        }
        *pdest = 0;

        return((int)(pdest - startAddr));
}

int cvtDoubleToString(
	double flt_value,
	char  *pdest,
	epicsUInt16 precision)
{
        epicsUInt16	got_one,i;
	epicsInt32		whole,iplace,number,fraction,fplace;
	double		ftemp;
	char		*startAddr;

	/* can this routine handle this conversion */
	if (isnan(flt_value) || precision > 8 || flt_value > 10000000.0 || flt_value < -10000000.0) {
		if (precision > 8 || flt_value > 1e16 || flt_value < -1e16) {
		    if(precision>17) precision=17;
		    sprintf(pdest,"%*.*e",precision+7,precision,
			flt_value);
		} else {
		    if(precision>3) precision=3;
		    sprintf(pdest,"%.*f",precision,flt_value);
		}
		return((int)strlen(pdest));
	}
	startAddr = pdest;

	/* determine the sign */
        if (flt_value < 0){
                *pdest++ = '-';
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
                        *pdest = number + '0';
                        pdest++;
                }else if (got_one){
                        *pdest = '0';
                        pdest++;
                }
        }
        if (!got_one){
                *pdest = '0';
                pdest++;
        }

        /* fraction */
        if (precision > 0){
		/* convert fractional portional to ASCII */
                *pdest = '.';
                pdest++;
                for (fplace /= 10, i = precision; i > 0; fplace /= 10,i--){
                        number = fraction / fplace;
                        fraction -= number * fplace;
                        *pdest = number + '0';
                        pdest++;
                }
        }
        *pdest = 0;

        return((int)(pdest - startAddr));
}

/*
 * These routines are provided for backwards compatibility,
 * extensions such as MEDM, edm and histtool use them.
 */

/*
 * cvtFloatToExpString
 *
 * Converts a float to a %e formatted string
 */
int cvtFloatToExpString(float val, char *pdest, epicsUInt16 precision)
{
    return epicsSnprintf(pdest, MAX_STRING_SIZE, "%.*e", precision, val);
}

/*
 * cvtFloatToCompactString
 *
 * Converts a float to a %g formatted string.
 * The result uses %f notation for 10e-4 < |value| < 10e+4,
 * otherwise %e notation.
 */
int cvtFloatToCompactString(float val, char *pdest, epicsUInt16 precision)
{
    if ((val < 1.e4 && val > 1.e-4) ||
        (val > -1.e4 && val < -1.e-4) ||
        val == 0.0)
        return cvtFloatToString(val, pdest, precision);

    return cvtFloatToExpString(val, pdest, precision);
}


/*
 * cvtDoubleToExpString
 *
 * Converts a double to a %e formatted string
 */

int cvtDoubleToExpString(double val, char *pdest, epicsUInt16 precision)
{
    return epicsSnprintf(pdest, MAX_STRING_SIZE, "%.*e", precision, val);
}


/*
 * cvtDoubleToCompactString
 *
 * Converts a double to %g formatted string.
 * The result uses %f notation for 10e-4 < |value| < 10e+4,
 * otherwise %e notation.
 */
int cvtDoubleToCompactString(double val, char *pdest, epicsUInt16 precision)
{
    if ((val < 1.e4 && val > 1.e-4) ||
        (val > -1.e4 && val < -1.e-4) ||
        val == 0.0)
        return cvtDoubleToString(val, pdest, precision);

    return cvtDoubleToExpString(val, pdest, precision);
}


/* Integer conversion primitives */

static size_t
    UInt32ToDec(epicsUInt32 val, char *pdest)
{
    int i;
    char digit[10];
    size_t len;

    for (i = 0; val; i++) {
        epicsUInt32 tenth = val / 10;

        digit[i] = val - tenth * 10 + '0';
        val = tenth;
    }
    len = i;

    while (i > 0)
        *pdest++ = digit[--i];

    *pdest = 0;
    return len;
}

static size_t
    UInt32ToBase(epicsUInt32 val, char *pdest, int base)
{
    int i;
    char digit, digits[32];
    size_t len;

    for (i = 0; val; i++) {
        epicsUInt32 tenth = val / base;

        digit = val - tenth * base;
        digits[i] = digit < 10 ? digit + '0' : digit - 10 + 'a';
        val = tenth;
    }
    len = i;

    while (i > 0)
        *pdest++ = digits[--i];

    *pdest = 0;
    return len;
}

static size_t
    UInt64ToDec(epicsUInt64 val, char *pdest)
{
    int i;
    char digit[20];
    size_t len;

    for (i = 0; val; i++) {
        epicsUInt64 tenth = val / 10;

        digit[i] = val - tenth * 10 + '0';
        val = tenth;
    }

    len = i;
    while (i > 0)
        *pdest++ = digit[--i];

    *pdest = 0;
    return len;
}

static size_t
    UInt64ToBase(epicsUInt64 val, char *pdest, int base)
{
    int i;
    char digit, digits[64];
    size_t len;

    for (i = 0; val; i++) {
        epicsUInt64 tenth = val / base;

        digit = val - tenth * base;
        digits[i] = digit < 10 ? digit + '0' : digit - 10 + 'a';
        val = tenth;
    }
    len = i;

    while (i > 0)
        *pdest++ = digits[--i];

    *pdest = 0;
    return len;
}


/* Integer conversion routines */

size_t
    cvtUInt32ToString(epicsUInt32 val, char *pdest)
{
    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 1;
    }

    return UInt32ToDec(val, pdest);
}

size_t
    cvtInt32ToString(epicsInt32 val, char *pdest)
{
    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 1;
    }

    if (val > 0)
        return UInt32ToDec(val, pdest);

    if (val == -0x80000000) {
        strcpy(pdest, "-2147483648");
        return strlen(pdest);
    }

    *pdest++ = '-';
    return 1 + UInt32ToDec(-val, pdest);
}


size_t
    cvtUInt64ToString(epicsUInt64 val, char *pdest)
{
    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 1;
    }

    return UInt64ToDec(val, pdest);
}

size_t
    cvtInt64ToString(epicsInt64 val, char *pdest)
{
    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 1;
    }

    if (val > 0)
        return UInt64ToDec(val, pdest);

    if (val == -0x8000000000000000LL) {
        strcpy(pdest, "-9223372036854775808");
        return strlen(pdest);
    }

    *pdest++ = '-';
    return 1 + UInt64ToDec(-val, pdest);
}


size_t
    cvtInt32ToHexString(epicsInt32 val, char *pdest)
{
    if (val < 0)
        *pdest++ = '-';

    *pdest++ = '0';
    *pdest++ = 'x';

    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 3;
    }

    if (val > 0)
        return 2 + UInt32ToBase(val, pdest, 16);

    if (val == -0x80000000) {
        strcpy(pdest, "80000000");
        return 11;
    }

    return 3 + UInt32ToBase(-val, pdest, 16);
}

size_t
    cvtUInt32ToHexString(epicsUInt32 val, char *pdest)
{
    *pdest++ = '0';
    *pdest++ = 'x';

    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 3;
    }

    return 2 + UInt32ToBase(val, pdest, 16);
}

size_t
    cvtInt32OctalString(epicsInt32 val, char *pdest)
{
    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 1;
    }

    if (val > 0) {
        *pdest++ = '0';
        return 1 + UInt32ToBase(val, pdest, 8);
    }

    if (val == -0x80000000) {
        strcpy(pdest, "-020000000000");
        return strlen(pdest);
    }

    *pdest++ = '-';
    *pdest++ = '0';
    return 2 + UInt32ToBase(-val, pdest, 8);
}

size_t
    cvtInt64ToHexString(epicsInt64 val, char *pdest)
{
    if (val < 0)
        *pdest++ = '-';

    *pdest++ = '0';
    *pdest++ = 'x';

    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 3;
    }

    if (val > 0)
        return 2 + UInt64ToBase(val, pdest, 16);

    if (val == -0x8000000000000000LL) {
        strcpy(pdest, "8000000000000000");
        return 19;
    }

    return 3 + UInt64ToBase(-val, pdest, 16);
}

size_t
    cvtUInt64ToHexString(epicsUInt64 val, char *pdest)
{
    *pdest++ = '0';
    *pdest++ = 'x';

    if (val == 0) {
        *pdest++ = '0';
        *pdest = 0;
        return 3;
    }

    return 2 + UInt64ToBase(val, pdest, 16);
}

