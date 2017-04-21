/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* error.h - errMessage symbol table header */

/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */


#ifndef INCerrorh
#define INCerrorh 1

#define NELEMENTS(array)		/* number of elements in an array */ \
		(sizeof (array) / sizeof ((array) [0]))

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /*INCerrorh*/
