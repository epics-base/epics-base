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
 *
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 *
 */

#ifndef INCdbRecTypeh
#define INCdbRecTypeh 1

struct recType {
	long	number;		/*number of types		*/
	char	**papName;	/*ptr to arr of ptr to name	*/
    };

#define S_rectype_noRecs     (M_recType| 3) /*No record types are defined*/

/*************************************************************************
 * Except that some entries may be null the following is valid access
 * 
 * precType->papName[i]
 *
 * The memory layout is
 *
 * precType->papName[1]->"ai"
 *		     [2]->"ao"
 **************************************************************************/

/**************************************************************************
 * The following macro returns NULL or a ptr to the rectype string
 * A typical usage is:
 *
 *	char *pstr;
 *	if(!(pstr=GET_PRECNAME(precType,type))) {<null>}
 **************************************************************************/
#define GET_PRECNAME(PRECTYPE,REC_TYPE)\
(\
    (PRECTYPE)\
	?(\
		( ((REC_TYPE)<1) || ((REC_TYPE)>=(PRECTYPE)->number) )\
		? NULL\
		: ((PRECTYPE)->papName[(REC_TYPE)])\
	)\
	: NULL\
)
#endif
