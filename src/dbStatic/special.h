/* special.h */
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
 * .01  06-07-91        mrk     Cleaned up
 * .02  03-04-92        jba     Added special Hardware Link
 * .03  07-27-93	mrk	Added SPC_ALARMACK
 * .04  02-09-94	mrk	Added SPC_AS
 */

#ifndef INCspecialh
#define INCspecialh 1

/*NOTE  Do NOT add aditional definitions with out modifying dbLexRoutines.c */
/* types 1-99 are global. Record specific must start with 100 */
#define SPC_NOMOD	1	/*Field must not be modified			*/
#define SPC_DBADDR	2	/*db_name_to_addr must call cvt_dbaddr	*/
#define SPC_SCAN	3	/*A scan related field is being changed		*/
#define SPC_ALARMACK	5	/*Special Alarm Acknowledgement*/
#define SPC_AS		6	/* Access Security*/
/* useful when record support must be notified of a field changing value*/
#define SPC_MOD		100
/* used by all records that support a reset field		*/
#define SPC_RESET	101	/*The res field is being modified*/
/* Specific to conversion (Currently only ai */
#define SPC_LINCONV	102	/*A linear conversion field is being changed*/
/* Specific to calculation records */
#define SPC_CALC	103	/*The CALC field is being changed*/


#define SPC_NTYPES 9
typedef struct mapspcType{
	char	*strvalue;
	int	value;
}mapspcType;

#ifndef SPECIAL_GBLSOURCE
extern mapspcType pamapspcType[];
#else
mapspcType pamapspcType[SPC_NTYPES] = {
	{"SPC_NOMOD",SPC_NOMOD},
	{"SPC_DBADDR",SPC_DBADDR},
	{"SPC_SCAN",SPC_SCAN},
	{"SPC_ALARMACK",SPC_ALARMACK},
	{"SPC_AS",SPC_AS},
	{"SPC_MOD",SPC_MOD},
	{"SPC_RESET",SPC_RESET},
	{"SPC_LINCONV",SPC_LINCONV},
	{"SPC_CALC",SPC_CALC}
};
#endif /*SPECIAL_GBLSOURCE*/

#endif /*INCspecialh*/
