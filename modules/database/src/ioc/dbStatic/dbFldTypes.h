/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

/** @file dbFldTypes.h
 *  @brief Defines possible field types
 *
 *  A field's type is perhaps its most important attribute. 
 *  Changing the possible field types is a fundamental change 
 *  to the IOC software, because many IOC software components 
 *  are aware of the field types. Field types of `DBF_STRING...
 *  DBF_DOUBLE` can be a scalar or an array and corresopond to 
 *  standard C data types
 *
 *  `DBR_*` are database request types that correspond exactly to 
 *  the database field types (`DBF_*`). When `dbPutField` or 
 *  `dbGetField` are called one of the arguments is a database 
 *  request type.
 *
 *  The field types `DBF_ENUM`, `DBF_MENU`, and `DBF_DEVICE` all 
 *  have an associated set of ASCII strings defining the choices. 
 *  For a `DBF_ENUM`, the record support module supplies values and 
 *  thus are not available for static database access. The database 
 *  access routines locate the choice strings for the other types.
 *
 * `DBF_INLINK` and `DBF_OUTLINK` specify link ﬁelds. A link ﬁield 
 * can refer to a signal located in a hardware module, to a ﬁeld 
 * located in a database record in the same IOC, or to a ﬁeld located 
 * in a record in another IOC. A `DBF_FWDLINK` can only refer to a record 
 * in the same IOC. Link ﬁelds are described in a later chapter.
 *
 * `DBF_INLINK` (input), `DBF_OUTLINK` (output), and `DBF_FWDLINK` 
 * (forward) specify that the ﬁeld is a link structure as deﬁned in link.h. 
 * There are three classes of links:
 *
 *  1. Constant - The value associated with the ﬁeld is a ﬂoating point 
 *  value initialized with a constant value. This is somewhat of a misnomer 
 *  because constant link ﬁelds can be modiﬁed via `dbPutField` or `dbPutLink`.
 *  2. Hardware links - The link contains a data structure which describes a 
 *  signal connected to a particular hardware bus. See link.h for a description 
 *  of the bus types currently supported.
 *  3.  Process Variable Links - This is one of three types:

 *      - PV_LINK: The process variable name.
 *      - DB_LINK: A reference to a process variable in the same IOC.
 *      - CA_LINK: A reference to a variable located in another IOC. 

 * When ﬁrst loaded the ﬁeld is always creates as a PV_LINK. When the IOC is 
 * initialized each PV_LINK is converted either to a DB_LINK or a CA_LINK.
 *
 * `DBF_NOACCESS` ﬁelds are for private use by record processing routines. 
 */

#ifndef INCdbFldTypesh
#define INCdbFldTypesh 1

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/* field types */
typedef enum {
    DBF_STRING, /*!< 40 character, NULL terminated ASCII string */
    DBF_CHAR, /*!< Signed character */
    DBF_UCHAR, /*!< Unsigned character */
    DBF_SHORT, /*!< Signed short */
    DBF_USHORT, /*!< Unsigned short */
    DBF_LONG, /*!< Signed long */
    DBF_ULONG, /*!< Unsigned long */
    DBF_INT64, /*!< 64 bit signed integer */
    DBF_UINT64, /*!< 64 bit unsigned integer */
    DBF_FLOAT, /*!< Floating point number */
    DBF_DOUBLE, /*!< Double precision float */
    DBF_ENUM, /*!< An enumerated field, analgous to C language enumeration */
    DBF_MENU, /*!< A meu choice field */
    DBF_DEVICE, /*!< A device choice field */
    DBF_INLINK, /*!< Input link */
    DBF_OUTLINK, /*!< Output link */
    DBF_FWDLINK, /*!< Forward link */
    DBF_NOACCESS /*!< A private field for use by record access routines*/
}dbfType;
#define DBF_NTYPES DBF_NOACCESS+1

typedef struct mapdbfType{
    char    *strvalue;
    dbfType value;
}mapdbfType;

DBCORE_API extern mapdbfType pamapdbfType[];
#ifdef DBFLDTYPES_GBLSOURCE
mapdbfType pamapdbfType[DBF_NTYPES] = {
    {"DBF_STRING",DBF_STRING},
    {"DBF_CHAR",DBF_CHAR},
    {"DBF_UCHAR",DBF_UCHAR},
    {"DBF_SHORT",DBF_SHORT},
    {"DBF_USHORT",DBF_USHORT},
    {"DBF_LONG",DBF_LONG},
    {"DBF_ULONG",DBF_ULONG},
    {"DBF_INT64",DBF_INT64},
    {"DBF_UINT64",DBF_UINT64},
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

#define DBR_STRING      DBF_STRING
#define DBR_CHAR        DBF_CHAR
#define DBR_UCHAR       DBF_UCHAR
#define DBR_SHORT       DBF_SHORT
#define DBR_USHORT      DBF_USHORT
#define DBR_LONG        DBF_LONG
#define DBR_ULONG       DBF_ULONG
#define DBR_INT64       DBF_INT64
#define DBR_UINT64      DBF_UINT64
#define DBR_FLOAT       DBF_FLOAT
#define DBR_DOUBLE      DBF_DOUBLE
#define DBR_ENUM        DBF_ENUM
#define DBR_PUT_ACKT    DBR_ENUM+1 
/*!< Value is an unsigned short for setting the ACKT (transient alarm 
 * acknowledgment)*/
#define DBR_PUT_ACKS    DBR_PUT_ACKT+1 
/*!< Value is an unsigned short for ACKS (global alarm acknowledgment)*/
#define DBR_NOACCESS    DBF_NOACCESS
#define VALID_DB_REQ(x) ((x >= 0) && (x <= DBR_ENUM))
#define INVALID_DB_REQ(x)       ((x < 0) || (x > DBR_ENUM))

#ifdef __cplusplus
}
#endif

#endif /*INCdbFldTypesh*/
