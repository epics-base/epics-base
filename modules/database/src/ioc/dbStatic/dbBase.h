/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
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

typedef struct dbMenu {
	ELLNODE		node;
	char		*name;
	int		nChoice;
	char		**papChoiceName;
	char		**papChoiceValue;
}dbMenu;

typedef struct drvSup {
	ELLNODE		node;
	char		*name;
	struct drvet	*pdrvet;
}drvSup;

typedef struct devSup {
	ELLNODE		node;
	char		*name;
	char		*choice;
	int		link_type;
	/*Following only available on run time system*/
	struct dset	*pdset;
	struct dsxt	*pdsxt;       /* Extended device support */
}devSup;

typedef struct linkSup {
	ELLNODE		node;
	char 		*name;
	char		*jlif_name;
	struct jlif	*pjlif;
} linkSup;

typedef struct dbDeviceMenu {
	int		nChoice;
	char		**papChoice;
}dbDeviceMenu;

/* conversion types*/
typedef enum {CT_DECIMAL,CT_HEX} ctType;
/* access level types */
typedef enum {ASL0,ASL1} asLevel;

/*Breakpoint Tables */
typedef struct brkInt{ /* breakpoint interval */
	double	raw;		/*raw value for beginning of interval	*/
	double	slope;		/*slope for interval			*/
	double	eng;		/*converted value for beginning of interval*/
}brkInt;

typedef struct brkTable { /* breakpoint table */
	ELLNODE		node;
	char		*name;		/*breakpoint table name	*/
	long		number;		/*number of brkInt in this table*/
	struct brkInt	*paBrkInt;	/* ptr to array of brkInts */
}brkTable;

typedef struct dbFldDes{  /* field description */
    char	*prompt; 	/*Prompt string for DCT*/
    char	*name;		/*Field name*/
    char	*extra;		/*C def for DBF_NOACCESS*/
    struct dbRecordType *pdbRecordType;
    short	indRecordType;	/*within dbRecordType.papFldDes */
    short	special;	/*Special processing requirements	*/
    dbfType	field_type;	/*Field type as defined in dbFldTypes.h */
    unsigned int process_passive:1;/*should dbPutField process passive	*/
    unsigned int prop:1;/*field is a metadata, post DBE_PROPERTY on change*/
    unsigned int isDevLink:1;  /* true for INP/OUT fields */
    ctType	base;		/*base for integer to string conversions*/
    short	promptgroup;	/*prompt, i.e. gui group		*/
    short   interest;	/*interest level			*/
    asLevel	as_level;	/*access security level			*/
    char	*initial;	/*initial value				*/
    /*If (DBF_MENU,DBF_DEVICE) ftPvt is (pdbMenu,pdbDeviceMenu)	*/
    void	*ftPvt;
    /*On no runtime following only set for STRING			*/
    short	size;		/*length in bytes of a field element	*/
    /*The following are only available on run time system*/
    unsigned short	offset;		/*Offset in bytes from beginning of record*/
}dbFldDes;

typedef struct dbInfoNode {	/*non-field per-record information*/
	ELLNODE		node;
	char		*name;
	char		*string;
	void		*pointer;
}dbInfoNode;

#define DBRN_FLAGS_VISIBLE 1
#define DBRN_FLAGS_ISALIAS 2
#define DBRN_FLAGS_HASALIAS 4

typedef struct dbRecordNode {
	ELLNODE		node;
	void		*precord;
	char		*recordname;
	ELLLIST		infoList;	/*LIST head of info nodes*/
	int		flags;
    struct dbRecordNode *aliasedRecnode; /* NULL unless flags|DBRN_FLAGS_ISALIAS */
}dbRecordNode;

/*dbRecordAttribute is for "psuedo" fields */
/*pdbFldDes is so that other access routines work correctly*/
/*Until base supports char * value MUST be fixed length string*/
typedef struct dbRecordAttribute {
	ELLNODE		node;
	char		*name;
	dbFldDes	*pdbFldDes;
	char		value[MAX_STRING_SIZE];
}dbRecordAttribute;

typedef struct dbText {
	ELLNODE		node;
	char		*text;
}dbText;

typedef struct dbVariableDef {
	ELLNODE		node;
	char		*name;
        char            *type;

}dbVariableDef;

typedef struct dbRecordType {
    ELLNODE		node;
    ELLLIST		attributeList;	/*LIST head of attributes*/
    ELLLIST		recList;	/*LIST head of sorted dbRecordNodes*/
    ELLLIST		devList;	/*List of associated device support*/
    ELLLIST		cdefList;	/*LIST of Cdef text items*/
    char		*name;
    short		no_fields;	/* number of fields defined	*/
    short		no_prompt;	/* number of fields to configure*/
    short		no_links;	/* number of links		*/
    short		no_aliases;	/* number of aliases in recList */
    short		*link_ind;	/* addr of array of ind in papFldDes*/
    char		**papsortFldName;/* ptr to array of ptr to fld names*/
    short		*sortFldInd;	/* addr of array of ind in papFldDes*/
    dbFldDes	*pvalFldDes;	/*pointer dbFldDes for VAL field*/
    short		indvalFlddes;	/*ind in papFldDes*/
    dbFldDes 	**papFldDes;	/* ptr to array of ptr to fldDes*/
    /*The following are only available on run time system*/
    rset        *prset;
    int		rec_size;	/*record size in bytes          */
}dbRecordType;

struct dbPvd;           /* Contents private to dbPvdLib code */
struct gphPvt;          /* Contents private to gpHashLib code */

typedef struct dbBase {
	ELLLIST		menuList;
	ELLLIST		recordTypeList;
	ELLLIST		drvList;
	ELLLIST		linkList;
	ELLLIST		registrarList;
	ELLLIST		functionList;
	ELLLIST		variableList;
	ELLLIST		bptList;
    ELLLIST         filterList;
    ELLLIST         guiGroupList;
    void		*pathPvt;
	struct dbPvd	*ppvd;
	struct gphPvt	*pgpHash;
	short		ignoreMissingMenus;
	short		loadCdefs;
}dbBase;
#endif
