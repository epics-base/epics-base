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
 *	Record Location
 *
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCdbRecordsh
#define INCdbRecordsh 1
#include <ellLib.h>

struct recLoc{	/* record location*/
	long	no_records;	/*OBSOLETE DO NOT USE*/
	long	no_used;	/*OBSOLETE DO NOT USE*/
	long	rec_size;	/*record size in bytes		*/
	long	record_type;	/*record type			*/
	void	*pFirst;	/*OBSOLETE DO NOT USE*/
	ELLLIST	*preclist; 	/*LIST head of sorted RECNODEs	*/
};
struct recHeader{ /*record header*/
	long		number;		/*number of record types*/
	struct recLoc	**papRecLoc;	/*ptr to arr of ptr to recLoc*/
};

#define S_record_noRecords   (M_record| 1) /*No records */

/***********************************************************************
 * Except that some ptr could be null the following is a valid reference
 *
 * precHeader->papRecLoc[i]->preclist
 *
 * The memory layout is:
 *
 * precHeader->recHeader
 *		 number
 *		 papRecLoc->[1]
 *			    [2]->recLoc
 *				    rec_type
 *				    pRECLIST-----> LIST---->RECNODE ...
 ************************************************************************/

/************************************************************************
 * In the self defining record the following appears
 *
 *	sdrHeader
 *	recLoc
 *	record 1
 *	...
 *	record n (n = recLoc.no_records
 **************************************************************************/

/************************************************************************
 * The following macro returns NULL or a ptr to struct recLoc
 * A typical usage is:
 *
 *	struct recLoc *precLoc
 *	if(!(precLoc=GET_PRECLOC(precHeader,type))) {<not found>}
 ***********************************************************************/
#define GET_PRECLOC(PRECHEADER,REC_TYPE)\
(\
    (PRECHEADER)\
    ?(\
	( ((REC_TYPE)<1) || ((REC_TYPE)>=(PRECHEADER)->number) )\
	? NULL\
	: ((PRECHEADER)->papRecLoc[(REC_TYPE)])\
    )\
    : NULL\
)

#endif
