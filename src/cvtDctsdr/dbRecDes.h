/* $Id$
 *
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
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
 * .02  05-18-92        rcz     removed extern
 * .03  05-18-92        rcz     changed macro "GET_PRECTYPDES(" to "GET_PRECTYPDES(precDes,"
 * .04  05-18-92        rcz     New database access
 * .05  08-12-92        jba     Removed dbr_field_type from struct fldDes
 * .06  09-15-92        jba     added short pad field to fldDes
 * .07  06-23-93	mrk	Moved pad so that fldname is NULL terminated
 */

#ifndef INCdbRecDesCT
#define INCdbRecDesCT
/* conversion types*/
#define CT_DECIMAL 0
#define CT_HEX	   1
/* lowfl, highfl */
#define CON	0
#define VAR	1
/* access level types */
#define ASL0	0
#define ASL1	1
#endif
#ifndef INCdbRecDesh
#define INCdbRecDesh
#define PROMPT_SZ 24
union fld_types{
	char		char_value;
	unsigned char	uchar_value;
	short		short_value;
	unsigned short	ushort_value;
	long		long_value;
	unsigned long	ulong_value;
	float		float_value;
	double		double_value;
	unsigned short	enum_value;
};
struct range {
	long			fldnum;
	long			pad;
	union	fld_types	value;
};
struct	fldDes{  /* field description */
	char	prompt[PROMPT_SZ]; /*Prompt string for DCT*/
	char	fldname[FLDNAME_SZ];/*field name	*/
	short	fldnamePad;	/*make sure fldname is NULL termated	*/
	short	offset;		/*Offset in bytes from beginning of record*/
	short	size;		/*length in bytes of a field element	*/
	short	special;	/*Special processing requirements	*/
	short	field_type;	/*Field type as defined in dbFldTypes.h */
	short	process_passive;/*should dbPutField process passive	*/
	short	choice_set;	/*index of choiceSet GBLCHOICE & RECCHOICE*/
	short	cvt_type;	/*Conversion type for DCT		*/
	short	promptflag;	/*Does DCT display this field		*/
	short	lowfl;		/*Is range1 CON or VAR			*/
	short	highfl;		/*Is range2 CON or VAR			*/
	short   interest;	/*interest level			*/
	short	as_level;	/*access security level			*/
	short	pad2;
	union fld_types	initial;/*initial value				*/
	struct  range	range1; /*Low value for field (Used by DCT)	*/
	struct  range	range2; /*High value for field (Used by DCT)	*/
};
struct recTypDes{  /* record type description */
	short		rec_size;	/* size of the record		*/
	short		no_fields;	/* number of fields defined	*/
	short		no_prompt;	/* number of fields to configure*/
	short		no_links;	/* number of links		*/
	short		*link_ind;	/* addr of array of ind in apFldDes*/
	unsigned long	*sortFldName;	/* addr of array of sorted fldname*/
	short		*sortFldInd;	/* addr of array of ind in apFldDes*/
	struct fldDes **papFldDes;	/* ptr to array of ptr to fldDes*/
};
struct	recDes{  /* record description */
	long		 number;	/*number of recTypDes*/
	struct recTypDes **papRecTypDes;/*ptr to arr of ptr to recTypDes*/
};

/***************************************************************************
 * Except that some ptr could be NULL the following is a valid reference
 * precDes->papRecTypDes[i]->papFldDes[j]-><field> 
 *
 *  The memory layout is as follows:
 *
 * precDes->recDes
 *	    	     number
 *	             papRecTypDes->[1]
 *		   		   [2]->papFldDes->[0]
 *						   [1]->fldDes
 *							   prompt
 *							   fldname
 *                                                          ...
 *
 * sortFldName[*] is a sorted array of blank padded fieldnames
 *                that can be interpeted as a long which must be 32 bits
 *                Note that FLDNAME_SZ MUST be 4
 * sortFldInd[*]  is an array of indices for sortFldName.
 *                Each element of this array is an index into papFldDes[*].
 *****************************************************************************/

/*****************************************************************************
 * The following macro returns NULL or a ptr to a fldDes structure
 * A typical usage is:
 *
 *	struct fldDes *pfldDes;
 *	if(!(pfldDes=GET_PFLDDES(precTypDes,ind)
 ****************************************************************************/
#define GET_PFLDDES(PRECTYPDES,IND_FLD)\
(\
    (PRECTYPDES)\
    ?(\
	( ((IND_FLD)<0) ||\
	    ((IND_FLD)>=((struct recTypDes *)(PRECTYPDES))->no_fields) )\
	? NULL\
	: (((struct recTypDes *)(PRECTYPDES))->papFldDes[(IND_FLD)])\
    )\
    : NULL\
)
/*****************************************************************************
 * The following macro returns NULL or a ptr to a recTypDes structure
 * A typical usage is:
 *
 *	struct recTypDes *precTypDes;
 *	if(!(precTypDes=GET_PRECTYPDES(precDes,ind);
 ****************************************************************************/
#define GET_PRECTYPDES(PRECDES,IND_REC)\
(\
    (PRECDES)\
    ?(\
	( ((IND_REC)<1) || ((IND_REC)>=(PRECDES)->number) )\
	? NULL\
	: ((PRECDES)->papRecTypDes[(IND_REC)])\
    )\
    : NULL\
)
#endif
