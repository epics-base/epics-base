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
typedef enum {
	DBF_STRING,
	DBF_CHAR,
	DBF_UCHAR,
	DBF_SHORT,
	DBF_USHORT,
	DBF_LONG,
	DBF_ULONG,
	DBF_FLOAT,
	DBF_DOUBLE,
	DBF_ENUM,
	DBF_MENU,
	DBF_DEVICE,
	DBF_INLINK,
	DBF_OUTLINK,
	DBF_FWDLINK,
	DBF_NOACCESS
}dbfType;
#define DBF_NTYPES DBF_NOACCESS+1

typedef struct mapdbfType{
	char	*strvalue;
	dbfType	value;
}mapdbfType;

#ifndef DBFLDTYPES_GBLSOURCE
extern mapdbfType pamapdbfType[];
#else
mapdbfType pamapdbfType[DBF_NTYPES] = {
	{"DBF_STRING",DBF_STRING},
	{"DBF_CHAR",DBF_CHAR},
	{"DBF_UCHAR",DBF_UCHAR},
	{"DBF_SHORT",DBF_SHORT},
	{"DBF_USHORT",DBF_USHORT},
	{"DBF_LONG",DBF_LONG},
	{"DBF_ULONG",DBF_ULONG},
	{"DBF_FLOAT",DBF_FLOAT},
	{"DBF_DOUBLE",DBF_DOUBLE},
	{"DBF_ENUM",DBF_ENUM},
	{"DBF_MENU",DBF_MENU},
	{"DBF_DEVICE",DBF_DEVICE},
	{"DBF_INLINK",DBF_INLINK},
	{"DBF_OUTLINK",DBF_OUTLINK},
	{"DBF_FWDLINK",DBF_FWDLINK},
	{"DBF_NOACCESS",DBF_NOACCESS}
};
#endif /*DBFLDTYPES_GBLSOURCE*/

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
#define VALID_DB_REQ(x) ((x >= 0) && (x <= DBR_ENUM))
#define INVALID_DB_REQ(x)       ((x < 0) || (x > DBR_ENUM))

#endif /*INCdbFldTypesh*/
