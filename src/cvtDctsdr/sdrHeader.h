/* sdrHeader.h - header for self defining record		*/
/* share/epicsH $Id$ */

/*
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
 * .02  05-03-92        rcz     New database access (removed extern)
 */

#ifndef INCsdrHeader
#define INCsdrHeader	1
struct sdrHeader {
    long		magic;		/* magic number		*/
    long		nbytes;		/* number of bytes of data*/
    short		type;		/* sdr record type	*/
    short		pad;		/* padding		*/
    long		create_date;	/* creation date	*/
};

#define	DBMAGIC	0x494f4300	/* In ascii this is IOC */

/*The following types are defined*/
#define SDR_DB_RECTYPE	0	/*Record types				*/
#define	SDR_DB_RECORDS	1	/*database records. index is type	*/
#define	SDR_DB_RECDES	2	/*Database Run Time Record Description	*/
#define	SDR_DB_PVD	3	/*Process Variable Directory		*/
#define	SDR_CHOICEGBL	4	/*Choice table. Global			*/
#define	SDR_CHOICECVT	5	/*Choice table. Convert			*/
#define	SDR_CHOICEREC	6	/*Choice table.	Record Specific		*/
#define	SDR_CHOICEDEV	7	/*Choice table.	Device Specific		*/
#define	SDR_DEVSUP	8	/*device support tables			*/
#define	SDR_CVTTABLE	9	/*breakpoint tables			*/
#define	SDR_DRVSUP	10	/*driver support tables			*/
#define	SDR_RECSUP	11	/*record support tables			*/
#define SDR_ALLSUMS	12	/*special sum structure*/
#define	SDR_NTYPES	SDR_ALLSUMS+1

/************************************************************************/
#define S_sdr_notLoaded (M_sdr| 1) /*dbRecType.sdr not loaded*/
#define S_sdr_noOpen	(M_sdr|	3) /*Can't open file*/
#define S_sdr_noRead	(M_sdr| 5) /*read failure*/
#define S_sdr_noMagic	(M_sdr| 7) /*wrong magic number*/
#define S_sdr_noRecDef	(M_sdr| 9) /*undefined record type*/
#define S_sdr_noAlloc	(M_sdr|11) /*malloc error*/
#define S_sdr_noReplace (M_sdr|13) /*Can't replace loaded SDR file*/
#define S_sdr_seqLoad   (M_sdr|15) /*dbRecType.sdr not first*/
#define S_sdr_noSdrType (M_sdr|17) /*undefined sdr type*/
#define S_sdr_sumError (M_sdr|19) /*sdrSum Error*/

#define SUM_LEN 50
struct sdrSum {
    char allSdrSums[SUM_LEN];
};

#endif /*INCsdrHeader*/
