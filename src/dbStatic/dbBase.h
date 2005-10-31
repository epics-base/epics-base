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
 *      Current Author:         Marty Kraimer
 *      Date:                   03-19-92
 */

#ifndef INCdbBaseh
#define INCdbBaseh 1

#include "dbFldTypes.h"
#include "ellLib.h"
#include "dbDefs.h"

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
	struct brkInt	**papBrkInt;	/* ptr to array of ptr to brkInt */
}brkTable;

typedef struct dbFldDes{  /* field description */
	char	*prompt; 	/*Prompt string for DCT*/
	char	*name;		/*Field name*/
	char	*extra;		/*C def for DBF_NOACCESS*/
	struct dbRecordType *pdbRecordType;
	short	indRecordType;	/*within dbRecordType.papFldDes */
	short	special;	/*Special processing requirements	*/
	dbfType	field_type;	/*Field type as defined in dbFldTypes.h */
	short	process_passive;/*should dbPutField process passive	*/
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
	short	offset;		/*Offset in bytes from beginning of record*/
}dbFldDes;

typedef struct dbInfoNode {	/*non-field per-record information*/
	ELLNODE		node;
	char		*name;
	char		*string;
	void		*pointer;
}dbInfoNode;

typedef struct dbRecordNode {
	ELLNODE		node;
	void		*precord;
	char		*recordname;
	ELLLIST		infoList;	/*LIST head of info nodes*/
	int		visible;
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
	short		*link_ind;	/* addr of array of ind in papFldDes*/
	char		**papsortFldName;/* ptr to array of ptr to fld names*/
	short		*sortFldInd;	/* addr of array of ind in papFldDes*/
	dbFldDes	*pvalFldDes;	/*pointer dbFldDes for VAL field*/
	short		indvalFlddes;	/*ind in papFldDes*/
	dbFldDes 	**papFldDes;	/* ptr to array of ptr to fldDes*/
	/*The following are only available on run time system*/
	struct	rset	*prset;
	int		rec_size;	/*record size in bytes          */
}dbRecordType;

typedef struct dbBase {
	ELLLIST		menuList;
	ELLLIST		recordTypeList;
	ELLLIST		drvList;
	ELLLIST		registrarList;
	ELLLIST		functionList;
	ELLLIST		variableList;
	ELLLIST		bptList;	/*Break Point Table Head*/
	void		*pathPvt;
	void		*ppvd;      /* pointer to process variable directory*/
	void		*pgpHash;	/*General purpose Hash Table*/
	short		ignoreMissingMenus;
	short		loadCdefs;
}dbBase;
#endif
