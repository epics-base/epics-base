/* $Id$
 * Breakpoint Tables
 *
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
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
 * .01  05-18-92        rcz     removed extern
 * .02  05-18-92        rcz     new database access
 * .03  08-19-92        jba     add prototypes for cvtRawToEngBpt,cvtEngToRawBpt
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
