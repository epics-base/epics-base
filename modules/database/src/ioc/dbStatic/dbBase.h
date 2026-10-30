/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INCdbBaseh
#define INCdbBaseh 1

/** \file   dbBase.h
 *  \author Marty Kraimer
 *  \date   03-19-92
 *  \brief  Base db structures
 */

#include "epicsTypes.h"
#include "dbFldTypes.h"
#include "ellLib.h"
#include "dbDefs.h"
#include "recSup.h"
#include "devSup.h"
#include "drvSup.h"

typedef struct dbMenu {
    ELLNODE         node;
    char            *name;
    int             nChoice;
    char            **papChoiceName;
    char            **papChoiceValue;
}dbMenu;

typedef struct drvSup {
    ELLNODE         node;
    char            *name;
    drvet           *pdrvet;
}drvSup;

typedef struct devSup {
    ELLNODE         node;
    char            *name;
    char            *choice;
    int             link_type;
    /*Following only available on run time system*/
    dset            *pdset;
    struct dsxt     *pdsxt;       /**< Extended device support */
}devSup;

typedef struct linkSup {
    ELLNODE         node;
    char            *name;
    char            *jlif_name;
    struct jlif     *pjlif;
} linkSup;

typedef struct dbDeviceMenu {
    int             nChoice;
    char            **papChoice;
}dbDeviceMenu;

/** \brief conversion types*/
typedef enum {CT_DECIMAL,CT_HEX} ctType;
/** \brief access level types */
typedef enum {ASL0,ASL1} asLevel;

/* Breakpoint Tables */

/** \brief breakpoint interval */
typedef struct brkInt{
    double          raw;            /**< \brief raw value for beginning of interval      */
    double          slope;          /**< \brief slope for interval                       */
    double          eng;            /**< \brief converted value for beginning of interval*/
}brkInt;

/** \brief breakpoint table */
typedef struct brkTable {
    ELLNODE         node;
    char            *name;          /**< \brief breakpoint table name                */
    long            number;         /**< \brief number of brkInt in this table       */
    struct brkInt   *paBrkInt;      /**< \brief ptr to array of brkInts              */
}brkTable;

/** \brief field description */
typedef struct dbFldDes{
    char            *prompt;        /**< \brief Prompt string for DCT                 */
    char            *name;          /**< \brief Field name                            */
    char            *extra;         /**< \brief C def for DBF_NOACCESS                */
    struct dbRecordType *pdbRecordType;
    short           indRecordType;  /**< \brief within dbRecordType.papFldDes */
    short           special;        /**< \brief Special processing requirements       */
    dbfType         field_type;     /**< \brief Field type as defined in dbFldTypes.h */
    unsigned int    process_passive:1;/**< \brief should dbPutField process passive   */
    unsigned int    prop:1;         /**< \brief field is a metadata, post DBE_PROPERTY on change*/
    unsigned int    isDevLink:1;    /**< \brief  true for INP/OUT fields              */
    ctType          base;           /**< \brief base for integer to string conversions*/
    short           promptgroup;    /**< \brief prompt, i.e. gui group                */
    short           interest;       /**< \brief interest level                        */
    asLevel         as_level;       /**< \brief access security level                 */
    char            *initial;       /**< \brief initial value                         */
    /** \brief If (DBF_MENU,DBF_DEVICE) ftPvt is (pdbMenu,pdbDeviceMenu)              */
    void            *ftPvt;
    /* On no runtime following only set for STRING                           */
    short           size;           /**< \brief length in bytes of a field element    */
    /*The following are only available on run time system*/
    unsigned short  offset;         /**< \brief Offset in bytes from beginning of record*/
}dbFldDes;

/** non-field per-record information*/
typedef struct dbInfoNode {
    ELLNODE         node;
    char            *name;
    char            *string;
    void            *pointer;
}dbInfoNode;

#define DBRN_FLAGS_VISIBLE 1
#define DBRN_FLAGS_ISALIAS 2
#define DBRN_FLAGS_HASALIAS 4

typedef struct dbRecordNode {
    ELLNODE         node;
    void            *precord;
    char            *recordname;
    ELLLIST         infoList;       /**< \brief LIST head of info nodes*/
    int             flags;
    /** Parse order of this record()
     *  @since 7.0.8.1
     */
    unsigned        order;
    struct dbRecordNode *aliasedRecnode; /**< \brief NULL unless flags|DBRN_FLAGS_ISALIAS */
}dbRecordNode;

/*dbRecordAttribute is for "pseudo" fields */
/*pdbFldDes is so that other access routines work correctly*/
/*Until base supports char * value MUST be fixed length string*/
typedef struct dbRecordAttribute {
    ELLNODE         node;
    char            *name;
    dbFldDes        *pdbFldDes;
    char            value[MAX_STRING_SIZE];
}dbRecordAttribute;

typedef struct dbText {
    ELLNODE         node;
    char            *text;
}dbText;

typedef struct dbVariableDef {
    ELLNODE         node;
    char            *name;
    char            *type;

}dbVariableDef;

typedef struct dbRecordType {
    ELLNODE         node;
    ELLLIST         attributeList;  /**< \brief LIST head of attributes*/
    ELLLIST         recList;        /**< \brief LIST head of sorted dbRecordNodes*/
    ELLLIST         devList;        /**< \brief List of associated device support*/
    ELLLIST         cdefList;       /**< \brief LIST of Cdef text items*/
    char            *name;
    short           no_fields;      /**< \brief number of fields defined     */
    short           no_prompt;      /**< \brief number of fields to configure*/
    short           no_links;       /**< \brief number of links              */
    short           no_aliases;     /**< \brief number of aliases in recList */
    short           *link_ind;      /**< \brief addr of array of ind in papFldDes*/
    char            **papsortFldName;/**< \brief ptr to array of ptr to fld names*/
    short           *sortFldInd;    /**< \brief * addr of array of ind in papFldDes*/
    dbFldDes        *pvalFldDes;    /**< \brief pointer dbFldDes for VAL field*/
    short           indvalFlddes;   /**< \brief index of VAL field in papFldDes[] */
    dbFldDes        **papFldDes;    /**< \brief ptr to array of ptr to fldDes*/
    /*The following are only available on run time system*/
    rset            *prset;
    int             rec_size;       /**< \brief record size in bytes          */
}dbRecordType;

struct dbPvd;           /* Contents private to dbPvdLib code */
struct gphPvt;          /* Contents private to gpHashLib code */

typedef struct dbBase {
    // struct dbMenu
    ELLLIST         menuList;
    // struct dbRecordType
    ELLLIST         recordTypeList;
    // struct drvSup
    ELLLIST         drvList;
    // struct linkSup
    ELLLIST         linkList;
    // struct dbText
    ELLLIST         registrarList;
    // struct dbText
    ELLLIST         functionList;
    // struct dbVariableDef
    ELLLIST         variableList;
    // struct brkTable
    ELLLIST         bptList;
    // struct chFilterPlugin
    ELLLIST         filterList;
    // struct dbGuiGroup (private)
    ELLLIST         guiGroupList;
    void            *pathPvt;
    struct dbPvd    *ppvd;
    struct gphPvt   *pgpHash;
    short           ignoreMissingMenus;
    short           loadCdefs;
    /** Total number of records.
     *  @since 7.0.8.1
     */
    unsigned        no_records;
}dbBase;
#endif
