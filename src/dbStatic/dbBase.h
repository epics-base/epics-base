/* $Id$
 *
 *      Current Author:         Marty Kraimer
 *      Date:                   03-19-92
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
 * .01  03-19-92        mrk     Original
 * .02  05-18-92        rcz     New database access
 * .03  09-21-92        rcz     removed #include <dbPvd.h>
 */

#ifndef INCdbBaseh
#define INCdbBaseh 1

#include <dbFldTypes.h>
#include <ellLib.h>
#include <dbDefs.h>

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
	float	raw;		/*raw value for beginning of interval	*/
	float	slope;		/*slope for interval			*/
	float	eng;		/*converted value for beginning of interval*/
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

typedef struct dbRecordNode {
	ELLNODE		node;
	void		*precord;
	char		*recordname;
	int		visible;
}dbRecordNode;

typedef struct dbRecordType {
	ELLNODE		node;
	ELLLIST		recList;	/*LIST head of sorted dbRecordNodes*/
	ELLLIST		devList;	/*List of associated device support*/
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
	ELLLIST		bptList;	/*Break Point Table Head*/
	void		*pathPvt;
	void		*ppvd;      /* pointer to process variable directory*/
	void		*pgpHash;	/*General purpose Hash Table*/
	short		ignoreMissingMenus;
}dbBase;
#endif
