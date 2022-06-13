/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/** \file dbBase.h
 *
 * \brief Definitions of the structures used to store an EPICS database
 *
 * This file contains the definitions of structures to store an EPICS database
 *
 */

/*
 *      Current Author:         Marty Kraimer
 *      Date:                   03-19-92
 */

#ifndef INCdbBaseh
#define INCdbBaseh 1

#include "epicsTypes.h"
#include "dbFldTypes.h"
#include "ellLib.h"
#include "dbDefs.h"
#include "recSup.h"
#include "devSup.h"

/**Structure for the db menu */
typedef struct dbMenu {
    //**Structure for the doubly-linked list node   */
    ELLNODE         node;
    char            *name;
    int             nChoice;
    char            **papChoiceName;
    char            **papChoiceValue;
}dbMenu;
/**Structure for driver support */
typedef struct drvSup {
    ELLNODE         node;
    char            *name;
    /**Structure for the driver entry table */
    struct drvet    *pdrvet;
}drvSup;
/**Structure for device support */
typedef struct devSup {
    ELLNODE         node;
    char            *name;
    char            *choice;
    int             link_type;
    /*Following only available on run time system*/
    dset            *pdset;
    /**Structure for the extended device support */
    struct dsxt     *pdsxt;       
}devSup;
/**Structure for link support */
typedef struct linkSup {
    ELLNODE         node;
    char            *name;
    char            *jlif_name;
    struct jlif     *pjlif;
} linkSup;
/**Structure for db device menu */
typedef struct dbDeviceMenu {
    int             nChoice;
    char            **papChoice;
}dbDeviceMenu;

/**Conversion enum types*/
typedef enum {CT_DECIMAL,CT_HEX} ctType;
/**Access level enum types*/
typedef enum {ASL0,ASL1} asLevel;

/**Structure for the breakpoint interval */
typedef struct brkInt{ 
    /**Raw value for beginning of interval   */
    double          raw;            
    /**Slope for interval                    */
    double          slope;          
    /**Converted value for beginning of interval*/
    double          eng;            
}brkInt;

/**Structure for the breakpoint table */
typedef struct brkTable { 
    ELLNODE         node;
    /**Breakpoint table name                 */
    char            *name;          
    /**Number of brkInt in this table        */
    long            number;         
    /**Pointer to array of brkInts              */
    struct brkInt   *paBrkInt;      
}brkTable;

/**Structure for the field description */
typedef struct dbFldDes{  
    /**Prompt string for DCT                 */
    char            *prompt;        
    /**Field name                            */
    char            *name;          
    /**C def for DBF_NOACCESS                */
    char            *extra;         
    struct dbRecordType *pdbRecordType;
    /**Within dbRecordType.papFldDes */
    short           indRecordType;  
    /**Special processing requirements       */
    short           special;        
    /**Field type as defined in dbFldTypes.h */
    dbfType         field_type;     
    /**Should dbPutField process passive   */
    unsigned int    process_passive:1;
    /**Field is a metadata, post DBE_PROPERTY on change*/
    unsigned int    prop:1;         
    /**True for INP/OUT fields              */
    unsigned int    isDevLink:1;    
    /**Base for integer to string conversions*/
    ctType          base;           
    /**Prompt, i.e. gui group                */
    short           promptgroup;    
    /**Interest level                        */
    short           interest;       
    /**Access security level                 */
    asLevel         as_level;       
    /**Initial value                         */
    char            *initial;       
    /**If (DBF_MENU,DBF_DEVICE) ftPvt is (pdbMenu,pdbDeviceMenu)             */
    void            *ftPvt;
    /**On no runtime following only set for STRING.                           
     * Length in bytes of a field element.    */
    short           size;           
    /**Only available on run time system.
     * Offset in bytes from beginning of record.*/
    unsigned short  offset;         
}dbFldDes;
/**Structure for the db information node. non-field per-record information*/
typedef struct dbInfoNode {         
    ELLNODE         node;
    char            *name;
    char            *string;
    void            *pointer;
}dbInfoNode;

#define DBRN_FLAGS_VISIBLE 1    /*If db is visible    */
#define DBRN_FLAGS_ISALIAS 2    /*If db is a alias  */
#define DBRN_FLAGS_HASALIAS 4   /*if db has aliases  */
/**Structure for the db record node */
typedef struct dbRecordNode {
    ELLNODE         node;
    void            *precord;
    char            *recordname;
    /**List head of info nodes*/
    ELLLIST         infoList;       
    int             flags;
    /**NULL unless flags|DBRN_FLAGS_ISALIAS */
    struct dbRecordNode *aliasedRecnode; 
}dbRecordNode;

/**dbRecordAttribute is for "psuedo" fields.
 *pdbFldDes is so that other access routines work correctly.
 *Until base supports char * value MUST be fixed length string.*/
typedef struct dbRecordAttribute {
    ELLNODE         node;
    char            *name;
    dbFldDes        *pdbFldDes;
    char            value[MAX_STRING_SIZE];
}dbRecordAttribute;
/**Structure for the db text   */
typedef struct dbText {
    ELLNODE         node;
    char            *text;
}dbText;
/**Structure for the definition of db variable */
typedef struct dbVariableDef {
    ELLNODE         node;
    char            *name;
    char            *type;

}dbVariableDef;
/**Strcuture for the record type of db */
typedef struct dbRecordType {
    ELLNODE         node;
    /**List head of attributes*/
    ELLLIST         attributeList;
    /**List head of sorted dbRecordNodes*/  
    ELLLIST         recList;    
    /**List of associated device support*/    
    ELLLIST         devList;        
    /**List of Cdef text items*/
    ELLLIST         cdefList;       
    char            *name;
    /**Number of fields defined     */
    short           no_fields;      
    /**Number of fields to configure*/
    short           no_prompt;      
    /**Number of links              */
    short           no_links;       
    /**Number of aliases in recList */
    short           no_aliases;     
    /**Addr of array of ind in papFldDes*/
    short           *link_ind;      
    /**Ptr to array of ptr to fld names*/
    char            **papsortFldName;
    /**Addr of array of ind in papFldDes*/
    short           *sortFldInd;    
    /**Pointer dbFldDes for VAL field*/
    dbFldDes        *pvalFldDes;   
    /**Ind in papFldDes*/ 
    short           indvalFlddes;  
    /**Ptr to array of ptr to fldDes*/ 
    dbFldDes        **papFldDes;    
    /* The following are only available on run time system*/
    rset            *prset;
    /**Record size in bytes          */
    int             rec_size;       
}dbRecordType;

/**Contents private to dbPvdLib code */
struct dbPvd;           
/**Contents private to gpHashLib code */
struct gphPvt;          

/**Struct for the list of dbBase */
typedef struct dbBase {
    ELLLIST         menuList;
    ELLLIST         recordTypeList;
    ELLLIST         drvList;
    ELLLIST         linkList;
    ELLLIST         registrarList;
    ELLLIST         functionList;
    ELLLIST         variableList;
    ELLLIST         bptList;
    ELLLIST         filterList;
    ELLLIST         guiGroupList;
    void            *pathPvt;
    struct dbPvd    *ppvd;
    struct gphPvt   *pgpHash;
    short           ignoreMissingMenus;
    short           loadCdefs;
}dbBase;
#endif
