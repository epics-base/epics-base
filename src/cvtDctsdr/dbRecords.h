/* $Id$
 *
 *	Record Location
 *
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
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
 * .01  mm-dd-yy        iii     Comment
 * .02  03-26-92        mrk     added pvdPvt private field for pvd routines
 * .03  04-10-92        rcz     changed pvdPvt to pList
 * .04  05-18-92        rcz     removed extern
 * .05  05-18-92        rcz     Changed macro call "GET_PRECLOC(" to "GET_PRECLOC(precHeader,"
 * .06  06-01-92        rcz     Moved struct RECNODE
 * .07  06-08-92        rcz     Changed pList to preclist
 * .08  06-08-92        rcz     New database access
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
