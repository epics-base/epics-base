/* $Id$
 *
 *      Author:          Bob Dalesio
 *      Date:            4-4-88
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
 *	Modification Log:
 *	-----------------
 */
#ifndef INC_db_addrh
#define INC_db_addrh
/* 
 *database access address structure 
 */
struct db_addr{
        char    *precord;       /* record number of specified type */
        char    *pfield;        /* offset from the record origin */
        char    *pad0;          /* not used by old              */
	void	*asPvt;		/*Access Security Private	*/
        short   pad1;           /*not used by old               */
        short   no_elements;    /* number of elements in arrays of data */
        short   new_field_type; /* field type for new database access*/
        short   field_size;     /* size of the field being accessed */
                                /* from database for values of waveforms */
        short   special;        /* special processing                   */
        short   field_type; /* field type as seen by database request*/
                                 /*DBR_STRING,...,DBR_ENUM,DBR_NOACCESS*/
};
#endif
