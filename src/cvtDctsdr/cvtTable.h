/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 * Breakpoint Tables
 *
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
 *
 */

#ifndef INCcvtTableh
#define INCcvtTableh	1
#ifndef INCdbDefsh
#include "dbDefs.h"
#endif

struct brkInt{ /* breakpoint interval */
	long	raw;		/*raw value for beginning of interval	*/
	float	slope;		/*slope for interval			*/
	float	eng;		/*converted value for beginning of interval*/
	};

struct brkTable { /* breakpoint table */
	char		*name;		/*breakpoint table name	*/
	long		number;		/*number of brkInt in this table*/
	long		rawLow;		/*lowest raw data value allowed */
	long		rawHigh;	/*highest raw data value allowed*/
	struct brkInt	**papBrkInt;	/* ptr to array of ptr to brkInt */
	};

struct arrBrkTable { /* array of brkTable */
	long		number;		/*number of break tables*/
	struct brkTable	**papBrkTable;	/* ptr to array of ptr to brkTable*/
	};

/***************************************************************************
 * except that some fields could be null the following is a valid reference
 * pcvtTable->papBrkTable[i]->papBrkInt[j]-><field> 
 *
 *  The memory layout is as follows:
 *
 *     pcvtTable->arrBrkTable
 *	    	     number
 *	             papBrkTable->[0]=NULL
 *                                [1]=NULL
 *		     	          [2]->brkTable
 *				          *name
 *				          number
 *				          papBrkInt->[0]
 *					             [1]->brkInt
 *						             raw
 *							     slope
 *							     eng
 *****************************************************************************/

/* Global Routines*/
#ifdef __STDC__
#define cvtTableUseProto
#endif

#ifdef __cplusplus
#ifndef cvtTableUseProto
#define cvtTableUseProto
#endif
#endif

#ifdef cvtTableUseProto
long cvtEngToRawBpt(double *pval,short linr,short init,
     void **ppbrk,short *plbrk);

long cvtRawToEngBpt(double *pval,short linr,short init,
     void **ppbrk, short *plbrk);

#else
long cvtEngToRawBpt();
long cvtRawToEngBpt();

#endif /*cvtTableUseProto*/

#endif
