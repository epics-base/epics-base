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
 * .01  mm-dd-yy        iii     Comment
 */
#ifndef INCdbFldTypesh
#define INCdbFldTypesh 1

/* field types */
#define DBF_STRING	0
#define	DBF_CHAR	1
#define	DBF_UCHAR	2
#define	DBF_SHORT	3
#define	DBF_USHORT	4
#define	DBF_LONG	5
#define	DBF_ULONG	6
#define	DBF_FLOAT	7
#define	DBF_DOUBLE	8
#define	DBF_ENUM	9
#define	DBF_GBLCHOICE	10
#define	DBF_CVTCHOICE	11
#define	DBF_RECCHOICE	12
#define	DBF_DEVCHOICE	13
#define	DBF_INLINK	14
#define	DBF_OUTLINK	15
#define	DBF_FWDLINK	16
#define	DBF_NOACCESS	17
#define DBF_NTYPES	DBF_NOACCESS+1

/* data request buffer types */
#define DBR_STRING      DBF_STRING
#define DBR_CHAR        DBF_CHAR
#define DBR_UCHAR       DBF_UCHAR
#define DBR_SHORT       DBF_SHORT
#define DBR_USHORT      DBF_USHORT
#define DBR_LONG        DBF_LONG
#define DBR_ULONG       DBF_ULONG
#define DBR_FLOAT       DBF_FLOAT
#define DBR_DOUBLE      DBF_DOUBLE
#define DBR_ENUM        DBF_ENUM
#define DBR_PUT_ACKT	DBR_ENUM+1
#define DBR_PUT_ACKS	DBR_PUT_ACKT+1
#define DBR_NOACCESS    DBF_NOACCESS
#define VALID_DB_REQ(x) ((x >= 0) && (x <= DBR_PUT_ACKS))
#define INVALID_DB_REQ(x)       ((x < 0) || (x > DBR_PUT_ACKS))

#endif /*INCdbFldTypesh*/
