/* error.h - errMessage symbol table header */
/* share/epicsH $Id$ */

/*
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
 * .01  mm-dd-yy        iii     Comment
 */


#ifndef INCerrorh
#define INCerrorh 1

#define LOCAL   static
#define NELEMENTS(array)		/* number of elements in an array */ \
		(sizeof (array) / sizeof ((array) [0]))


typedef struct		/* ERRSYMBOL - entry in symbol table */
    {
    char *name;		/* pointer to symbol name */
    long errNum;	/* errMessage symbol number */
    } ERRSYMBOL;
typedef struct		/* ERRSYMTAB - symbol table */
    {
    short nsymbols;	/* current number of symbols in table */
    ERRSYMBOL *symbols;	/* ptr to array of symbol entries */
    } ERRSYMTAB;
typedef ERRSYMTAB *ERRSYMTAB_ID;

#ifdef vxWorks
#define MYERRNO	(errnoGet()&0xffff)
#else
#define MYERRNO	errno
#endif


/*************************************************************/
struct errSet {			/* This defines one module error set */
    long            number;	/* dimension of err strings */
    char          **papName;	/* ptr to arr of ptr to error string	 */
};
struct errDes {			/* An array of error sets for modules */
    long            number;	/* number of err modules */
    struct errSet **papErrSet;	/* ptr to arr of ptr to errSet */
};
extern struct errDes *dbErrDes;
/*************************************************************/

#endif /*INCerrorh*/
