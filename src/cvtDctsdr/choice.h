/* $Id$
 *
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
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
 * .01  05-18-92        rcz     removed extern's
 * .02  05-18-92        rcz     New database access 
 */


#ifndef INCchoiceh
#define INCchoiceh 1
struct choiceSet { /* This defines one set of choices*/
	unsigned long	number;	/*number of choices 			*/
	char		**papChoice;/*ptr to arr of ptr to choice string*/
	};
struct arrChoiceSet{ /*An array of choice sets for particular record type*/
	unsigned long	number;		/*number of choice sets 	*/
	struct choiceSet **papChoiceSet	;/*ptr to arr of ptr to choiceSet*/
	};
struct choiceRec{ /*define choices for each record type*/
	unsigned long	number;		   /*number of arrChoiceSet	*/
	struct arrChoiceSet **papArrChoiceSet;
		/*ptr to arr of ptr to arrChoiceSet*/
	};
/* device choices					*/
struct devChoice{
	long	link_type;	/*link type for this device*/
	char	*pchoice;	/*ptr to choice string	*/
	};
struct devChoiceSet {
	unsigned long	 number;
	struct devChoice **papDevChoice;
	char		**papChoice;
	};
struct devChoiceRec{ /*define device choices for each record type*/
	unsigned long	number;		   /*number of devChoiceSet	*/
	struct devChoiceSet **papDevChoiceSet;
		/*ptr to arr of ptr to devChoiceSet*/
	};
/*NOTE: pchoiceCvt is initialized when cvtTable is loaded		*/
/*      pchoiceDev is initialized when devSup is loaded			*/

/************************************************************************
 * Except that entries could be null the following are valid references
 *   pchoiceCvt->papChoice[i]
 *   pchoiceGbl->papChoiceSet[i]->papChoice[j]
 *   pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k]
 *   pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice->"<value>"
 *
 * The memory layout is
 *
 *   pchoiceCvt->choiceSet
 *		   number
 *		   papChoice->[0]
 *			      [1]->"<choice string>"
 *
 *   pchoiceGbl->arrChoiceSet
 *		   number
 *		   papChoiceSet->[0]
 *                               [1]->choiceSet
 *		  	     		number
 *		  			papChoice->[0]
 *						   [1]->"<choice string>"
 *   pchoiceRec->choiceRec
 *		   number
 *		   papArrChoiceSet->[0]
 * 				    [1]->arrChoiceSet
 *					  number
 *					  papChoiceSet->[0]
 *                                                      [1]->choiceSet
 *						              number
 *						              papChoice->[0]
 *								         [1]->
 *
 *   pchoiceDev->devChoiceRec
 *                 number
 *                 papDevChoiceSet->[0]
 *                                  [1]->devChoiceSet
 *                                          number
 *                                          papDevChoice->[0]
 *                                                        [1]->devChoice
 *                                                                link_type
 *                                                                pchoice->"val"
 *****************************************************************************/

/*************************************************************************
 *The following macro returns NULL or a ptr to a string		
 * A typical usage is:
 *
 *	char *pchoice;
 *	if(!(pchoice=GET_CHOICE(pchoiceSet,ind))){ <no string found>}
 ***************************************************************************/
#define GET_CHOICE(PCHOICE_SET,IND_CHOICE)\
(\
    (PCHOICE_SET)\
    ?(\
	( ((unsigned)(IND_CHOICE)>=((struct choiceSet*)(PCHOICE_SET))->number) )\
	? NULL\
	: ((PCHOICE_SET)->papChoice[(IND_CHOICE)])\
    )\
    : NULL\
)

/*****************************************************************************
 * The following macro returns NULL or a ptr to a choiceSet structure
 * A typical usage is:
 *
 *	struct choiceSet *pchoiceSet;
 *	if(!(pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,ind))){<null>}
 ***************************************************************************/
#define GET_PCHOICE_SET(PARR_CHOICE_SET,IND_ARR)\
(\
    (PARR_CHOICE_SET)\
    ?(\
	( ((unsigned)(IND_ARR)>=((struct arrChoiceSet*)(PARR_CHOICE_SET))->number) )\
	? NULL\
	: ((PARR_CHOICE_SET)->papChoiceSet[(IND_ARR)])\
    )\
    : NULL\
)

/*****************************************************************************
 * The following macro returns NULL or a ptr to a arrChoiceSet structure
 * A typical usage is:
 *
 *	struct arrChoiceSet *parrChoiceSet;
 *	if(!(parrChoiceSet=GET_PARR_CHOICE_SET(pchoiceRec,ind))){<null>}
 ***************************************************************************/
#define GET_PARR_CHOICE_SET(PCHOICE_REC,IND_REC)\
(\
    (PCHOICE_REC)\
    ?(\
	( ((unsigned)(IND_REC)>=((struct choiceRec*)(PCHOICE_REC))->number) )\
	? NULL\
	: ((PCHOICE_REC)->papArrChoiceSet[(IND_REC)])\
    )\
    : NULL\
)

/*************************************************************************
 *The following macro returns NULL or a ptr to a devChoice structure
 * A typical usage is:
 *
 *	struct devChoice *pdevChoice;
 *	if(!(pdevChoice=GET_DEV_CHOICE(pdevChoiceSet,ind))){ <not found>}
 ***************************************************************************/
#define GET_DEV_CHOICE(PDEV_CHOICE_SET,IND_CHOICE)\
(\
    (PDEV_CHOICE_SET)\
    ?(\
	( ((unsigned)(IND_CHOICE)>=((struct devChoiceSet*)(PDEV_CHOICE_SET))->number) )\
	? NULL\
	: ((PDEV_CHOICE_SET)->papDevChoice[(IND_CHOICE)])\
    )\
    : NULL\
)

/*****************************************************************************
 * The following macro returns NULL or a ptr to a devChoiceSet structure
 * A typical usage is:
 *
 *	struct devChoiceSet *pdevChoiceSet;
 *	if(!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pchoiceDev,ind))){<null>}
 ***************************************************************************/
#define GET_PDEV_CHOICE_SET(PCHOICE_DEV,IND_REC)\
(\
    (PCHOICE_DEV)\
    ?(\
	( ((unsigned)(IND_REC)>=((struct devChoiceRec*)(PCHOICE_DEV))->number) )\
	? NULL\
	: ((PCHOICE_DEV)->papDevChoiceSet[(IND_REC)])\
    )\
    : NULL\
)
#endif
