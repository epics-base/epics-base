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
