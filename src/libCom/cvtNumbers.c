/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	unknown
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1992, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	unknown		rac	initial version, cloned from WETF code
 * .02	02-19-92	rac	added proper treatment for fractional values
 *				and printing for integers
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	cvtNumbers.c - convert numeric to text
*
* DESCRIPTION
*	These routines provide service to convert numeric values to text
*	form.
*
* QUICK REFERENCE
*    int cvtDblToTxt(    text,   width,   dblVal,   decPl                  )
*    int cvtLngToTxt(    text,   width,   lngVal                           )
*
*
*-***************************************************************************/
#ifdef vxWorks
#   include <vxWorks.h>
#   include <stdioLib.h>
#else
#   include <stdio.h>
#   include <math.h>
#endif

#include <genDefs.h>


#ifdef vxWorks
#define nint(value) (value>=0 ? (int)((value)+.5) : (int)((value)-.5))
#define exp10(value) (exp(value * log(10.)))
#endif

#if 0
main()
{
    float	x, y;
    char	text[100];
    int		i, iter;
    int		xi, yi;

    for (x=.000123; x<1000; x*=10.) {
	printf("%10.5f", x);
	for (i=1; i<10; i++) {
	    cvtDblToTxt(text, i, x, 3);
	    printf(" %s", text);
	}
	printf("\n");
	y = -1. * x;
	printf("%10.5f", y);
	for (i=1; i<10; i++) {
	    cvtDblToTxt(text, i, y, 3);
	    printf(" %s", text);
	}
	printf("\n");
    }
    for (iter=1,xi=0; iter<=12; iter++,xi+=xi*10+xi+1) {
	printf("%12d", xi);
	for (i=1; i<=iter; i++) {
	    cvtLngToTxt(text, i, xi);
	    printf(" %s", text);
	}
	printf("\n");
	yi = -1 * xi;
	printf("%12d", yi);
	for (i=1; i<=iter+1; i++) {
	    cvtLngToTxt(text, i, yi);
	    printf(" %s", text);
	}
	printf("\n");
    }
}
#endif

/*+/subr**********************************************************************
* NAME	cvtDblToTxt - convert double to text, being STINGY with space
*
* DESCRIPTION
*	Convert a double value to text.  The main usefulness of this routine
*	is that it maximizes the amount of information presented in a
*	minimum number of characters.  For example, if a 1 column width
*	is specified, only the sign of the value is presented.
*
*	When an exponent is needed to represent the value, for narrow
*	column widths only the exponent appears.  If there isn't room
*	even for the exponent, large positive exponents will appear as E*,
*	and large negative exponents will appear as E-.
*
*	Negative numbers receive some special treatment.  In narrow
*	columns, very large negative numbers may be represented as - and
*	very small negative numbers may be represented as -. or -.E-  .
*
*	Some example outputs follow (with 3 decimal places assumed):
*
*	value	printed values for column widths
*	         1   2    3     4      5       6
*
*       0.000    0   0    0     0      0       0
*      -1.000    -  -1   -1    -1     -1      -1
*       0.123    +  E-  .12  .123   .123    .123
*    -0.00123    -  -.   -.  -.E-  -.001   -.001
*       -12.3    -   -  -12   -12  -12.3  -12.30
*         123    +  E2  123   123  123.0  123.00
*
*-*/
cvtDblToTxt(text, width, value, decPl)
char	*text;		/* O text representation of value */
int	width;		/* I max width of text string (not counting '\0') */
double	value;		/* I value to print */
int	decPl;		/* I max # of dec places to print */
{
    double	valAbs;		/* absolute value of caller's value */
    int		wholeNdig;	/* number of digits in "whole" part of value */
    double	logVal;		/* log10 of value */
    int		decPlaces;	/* number of decimal places to print */
    int		expI;		/* exponent for frac values */
    double	expD;
    int		expWidth;	/* width needed for exponent field */
    int		excess;		/* number of low order digits which
				    won't fit into the field */
    char	tempText[100];	/* temp for fractional conversions */
    int		roomFor;
    int		minusWidth;	/* amount of room for - sign--0 or 1 */

/*-----------------------------------------------------------------------------
*    special cases
*----------------------------------------------------------------------------*/
    if (value == 0.) {
	(void)strcpy(text, "0");
	return;
    }
    else if (value == 1.) {
	(void)strcpy(text, "1");
	return;
    }
    if (width == 1) {
	if (value >= 0)
	    strcpy(text, "+");
	else
	    strcpy(text, "-");
	return;
    }
    else if (width == 2 && value <= -1.) {
	if (value == -1.)
	    strcpy(text, "-1");
	else
	    strcpy(text, "-");
	return;
    }

    valAbs = value>0. ? value : -value;
    logVal = log10(valAbs);
    if (logVal < 0.) {
/*-----------------------------------------------------------------------------
*    numbers with only a fractional part
*----------------------------------------------------------------------------*/
	if (width == 2) {
	    if (value > 0.)
		strcpy(tempText, "0E-");
	    else
		strcpy(tempText, "0-.");
	}
	else if (width == 3 && value < 0)
	    strcpy(tempText, "0-.");
	else {
	    if (value < 0.)
		minusWidth = 1;
	    else
		minusWidth = 0;
	    expI = -1 * (int)logVal;
	    if (expI < 9)
		expWidth = 3;		/* need E-n */
	    else if (expI < 99)
		expWidth = 4;		/* need E-nn */
	    else if (expI < 999)
		expWidth = 5;		/* need E-nnn */
	    else
		expWidth = 6;		/* need E-nnnn */
	    /* figure out how many sig digit can appear if print "as is" */
	    /* expI is the number of leading zeros */
	    roomFor = width - expI - 1 - minusWidth;	/* -1 for the dot */
	    if (roomFor >= 1) {
		decPlaces = expI + decPl;
		if (decPlaces > width -1 - minusWidth)
		    decPlaces = roomFor + expI;
		(void)sprintf(tempText, "%.*f", decPlaces, value);
		if (value < 0.)
		    tempText[1] = '-';
	    }
	    else {
		expD = expI;
		value *= exp10(expD);
		if (value > 0.)
		    sprintf(tempText, "0.E-%d", expI);
		else
		    sprintf(tempText, "--.E-%d", expI);
	    }
	}

	strncpy(text, &tempText[1], width);
	text[width] = '\0';
	return;
    }

/*-----------------------------------------------------------------------------
*    numbers with both an integer and a fractional part
*
*	find out how many columns are required to represent the integer part
*	of the value.  A - is counted as a column;  the . isn't.
*----------------------------------------------------------------------------*/
    wholeNdig = 1 + (int)logVal;
    if (value < 0.)
	wholeNdig++;
    if (wholeNdig < width-1) {
/*-----------------------------------------------------------------------------
*    the integer part fits well within the field.  Find out how many
*    decimal places can be printed (honoring caller's decPl limit).
*----------------------------------------------------------------------------*/
	decPlaces = width - wholeNdig - 1;
	if (decPl < decPlaces)
	    decPlaces = decPl;
	if (decPl > 0)
	    (void)sprintf(text, "%.*f", decPlaces, value);
	else
	    (void)sprintf(text, "%d", nint(value));
    }
    else if (wholeNdig == width || wholeNdig == width-1) {
/*-----------------------------------------------------------------------------
*    The integer part just fits within the field.  Print the value as an
*    integer, without printing the superfluous decimal point.
*----------------------------------------------------------------------------*/
	(void)sprintf(text, "%d", nint(value));
    }
    else {
/*-----------------------------------------------------------------------------
*    The integer part is too large to fit within the caller's field.  Print
*    with an abbreviated E notation.
*----------------------------------------------------------------------------*/
	expWidth = 2;				/* assume that En will work */
	excess = wholeNdig - (width - 2);
	if (excess > 999) {
	    expWidth = 5;			/* no! it must be Ennnn */
	    excess += 3;
	}
	else if (excess > 99) {
	    expWidth = 4;			/* no! it must be Ennn */
	    excess += 2;
	}
	else if (excess > 9) {
	    expWidth = 3;			/* no! it must be Enn */
	    excess += 1;
	}
/*-----------------------------------------------------------------------------
*	Four progressively worse cases, with all or part of exponent fitting
*	into field, but not enough room for any of the value
*		Ennn		positive value; exponent fits
*		-Ennn		negative value; exponent fits
*		+****		positive value; exponent too big
*		-****		negative value; exponent too big
*----------------------------------------------------------------------------*/
	if (value >= 0. && expWidth == width)
	    (void)sprintf(text, "E%d", nint(logVal));
	else if (value < 0. && expWidth == width-1)
	    (void)sprintf(text, "-E%d", nint(logVal));
	else if (value > 0. && expWidth > width)
	    (void)sprintf(text, "%.*s", width, "+*******");
	else if (value < 0. && expWidth > width-1)
	    (void)sprintf(text, "%.*s", width, "-*******");
	else {
/*-----------------------------------------------------------------------------
*	The value can fit, in exponential notation
*----------------------------------------------------------------------------*/
	    (void)sprintf(text, "%dE%d",
			nint(value/exp10((double)excess)), excess);
	}
    }
}

/*+/subr**********************************************************************
* NAME	cvtLngToTxt - convert long to text, being STINGY with space
*
* DESCRIPTION
*	Convert a long value to text.  The main usefulness of this routine
*	is that it maximizes the amount of information presented in a
*	minimum number of characters.  For example, if a 1 column width
*	is specified, only the sign of the value is presented.
*
*	When an exponent is needed to represent the value, for narrow
*	column widths only the exponent appears.  If there isn't room
*	even for the exponent, large positive exponents will appear as E*.
*
*	Negative numbers receive some special treatment.  In narrow
*	columns, very large negative numbers may be represented as -  .
*
*	Some example outputs follow:
*
*	value	printed values for column widths
*	         1   2    3     4      5       6
*
*	    0    0   0    0     0      0       0
*	   -1    -  -1   -1    -1     -1      -1
*      271453    +  E5  2E5  27E4  271E3  271453
*      -22621    -   -  -E4  -2E4  -22E3 - 22621
*
*-*/
cvtLngToTxt(text, width, value)
char	*text;		/* O text representation of value */
int	width;		/* I max width of text string (not counting '\0') */
long	value;		/* I value to print */
{
    char	Text[100];
    int		nDig, expWidth, expVal, sNcol;

    assert(width > 0);

    sprintf(Text, "%d", value);
    if ((nDig = strlen(Text)) <= width) {
	strcpy(text, Text);
	return;
    }
    if (width == 1) {
	if (value > 0)
	    strcpy(text, "+");
	else
	    strcpy(text, "-");
	return;
    }
    if (width == 2 && value < 0) {
	strcpy(text, "-");
	return;
    }
    if (value < 0)
	sNcol = 1;
    else
	sNcol = 0;
    expVal = nDig - width + 2;
    if (expVal <= 9) {
	if (width-sNcol == 2)
	    expVal--;
	sprintf(&Text[width-2], "E%d", expVal);
    }
    else {
	expVal++;
	if (width-sNcol == 2)
	    sprintf(&Text[sNcol], "E*");
	else {
	    if (width-sNcol == 3)
		expVal--;
	    sprintf(&Text[width-3], "E%d", expVal);
	}
    }
    strcpy(text, Text);

    return;
}
