/*dbta.c*/
/* share/src/db $Id$ */
/*
 *
 *     Author:	Marty Kraimer
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
 * .01	07-13-93	mrk	Original version
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <errMdef.h>
#include <dbDefs.h>
#include <dbStaticLib.h>
DBBASE	*pdbbase;

#ifdef __STDC__
void parse_command_line_args(int, char **, int *, int *, char **);
#else
void parse_command_line_args();
#endif /*__STDC__*/

#ifdef __STDC__
int main(int argc,char **argv)
#else
int main(argc,argv)
int argc;
char **argv;
#endif /*__STDC__*/
{
    DBENTRY	*pdbentry;
    FILE	*fp;
    long	status;
    char	*ptime;
    time_t	timeofday;
    char	*rectype;
    int 	verbose;
    int 	oldshortform;
    char 	*dbname;
    char 	*sdr_filename;
    
    parse_command_line_args(argc, argv, &verbose, &oldshortform, &dbname);

    fp = fopen(dbname,"r");
    if(!fp) {
	errMessage(0,"Error opening file");
	exit(-1);
    }
    pdbbase=dbAllocBase();
    pdbentry=dbAllocEntry(pdbbase);
    status=dbRead(pdbbase,fp);
    if(status) {
	errMessage(status,"dbRead");
	exit(-1);
    }
    fclose(fp);
    status = dbFirstRecdes(pdbentry);
    if(status) {
	printf("No record description\n");
	exit(-1);
    }
    if(oldshortform) {
	printf("$$mono\n");
	timeofday = time((time_t *)0);
	ptime = ctime(&timeofday);
	printf("  Time: %s\n",ptime);
    }
    while(!status) {
	rectype = dbGetRecdesName(pdbentry);
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    int generate_nl;

	    printf("\nPV: %s     Type: %s (atdb)",dbGetRecordName(pdbentry),rectype);
	    printf("\n");
	    generate_nl=FALSE;
	    status = dbFirstFielddes(pdbentry,TRUE);
	    if(status) printf("    No Fields\n");
	    while(!status) {
		char *pstr;
		int  lineout;

		if(verbose || !dbIsDefaultValue(pdbentry)) {
		    if(generate_nl) printf("\n");
		    printf("%4s ",dbGetFieldName(pdbentry));
		    pstr = dbGetString(pdbentry);
		    if(pstr) printf("%s",pstr);
		    generate_nl=TRUE;
		}
		status=dbNextFielddes(pdbentry,TRUE);
	    }
	    printf("\f\n");
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecdes(pdbentry);
    }
    if(oldshortform) printf("$$end\n");
    dbFreeEntry(pdbentry);
    dbFreeBase(pdbbase);
    return(0);
}

#ifdef __STDC__
void parse_command_line_args(int argc, char **argv,int *verbose,
	int *oldshortform, char **dbname)
#else
void parse_command_line_args(argc,argv,verbose,oldshortform,dbname)
int argc;
char **argv;
int *verbose;
int *oldshortform;
char **dbname;
#endif /*__STDC__*/
{
    extern char *optarg; /* needed for getopt() */
    extern int optind;   /* needed for getopt() */
    int i;
    int c;
    int input_error;

    /* defaults */
    *verbose = FALSE;
    *oldshortform = FALSE;
    *dbname = (char *) NULL;

    input_error = FALSE;

    while (!input_error && (c = getopt(argc, argv, "vs")) != -1)
    {
        switch (c)
        {
            case 'v': *verbose = TRUE; break;
            case 's': *oldshortform = TRUE; break;
            case '?': input_error = 1; break;
            default: input_error = 1; break;
        } /* end switch() */
    } /* endwhile */

    if (!input_error && optind < argc)
    {
	int	gotperiod=FALSE;
	int	len;

	len = strlen(argv[optind]);
	if(strchr(argv[optind],'.')) gotperiod = TRUE;
	if(!gotperiod) len = len + strlen(".database");
	*dbname = dbCalloc(1,len);
	strcpy(*dbname,argv[optind]);
	if(!gotperiod) strcat(*dbname,".database");
	return;
    }
    fprintf(stderr, "\nusage: dbta [-v] [-s] fn\n");
    fprintf(stderr,"\n\t-v\tVerbose mode, outputs all prompt fields of\n");
    fprintf(stderr,"\t\trecords to standard out.  If not specified,\n");
    fprintf(stderr,"\t\tthen only non default fields will be printed\n");
    fprintf(stderr,"\t-s\tGenerate old DCT short form format file\n");
    fprintf(stderr,"\tfn\tNames the .database file.\n");
    exit(1);
}
