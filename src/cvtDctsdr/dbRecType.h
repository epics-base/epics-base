/* $Id$
 *
 *      Author:          Marty Kraimer
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
 * .01  05-18-92        rcz     removed extern
 * .02  05-18-92        rcz    Changed GET_PRECTYPE to GET_PRECNAME
 * .03  05-18-92        rcz     New database access (removed extern)
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
