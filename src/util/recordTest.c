/* recordTest.c */
/* share/src/util   $Id$ */

/* recordTest.c - Record Test Program  
 *
 *	Author:		Janet Anderson
 *	Date:		02-05-92
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	02-10-92	jba	initial version
 *
Need to do:

	1. Change to DBR_STS_STRING ???
	2. Terminate connections only when test for a new rec type are encountered
	3. Add handling of elementCount > 1
	5. Add epics release info to output file
	6. Add date and IOC info to output file
 */

#ifdef vxWorks
#include        <vxWorks.h>
#include        <stdioLib.h>
#else
#include        <stdio.h>
#endif

#include        <cadef.h>
#include        <string.h>

#ifndef ERROR
#define ERROR -1
#endif
#ifndef OK
#define OK 0
#endif

#define PUT 0
#define GET 1
#define FIELDS 4
#define MAXLINE 150
#define MAX_ELEMENTS 150
#define MAX_STRING_SIZE  40

char		errmsg[MAXLINE];

void recordTest();
int connectChan();
int disconnectChan();
int doTestLine();
int getPV();
int putPV();

#ifndef vxWorks
main(argc,argv)
int     argc;
char    *argv[];
{


        if(argc > 2){
                printf("usage: recordTest <record_type>\n");
                printf("the following arguments were received\n");
                while(argc>0) {
                        printf("%s\n",argv[0]);
                        argv++; argc--;
                }
                return ERROR;
        }
	if ( argc == 2 ){
		printf("\n\n\n\n\n\nTest of record types:");
		while(--argc>0) printf(" %s",*++argv);
	} else {
		 printf("Test of ALL record types");
	}

	printf("\n");

	recordTest(argv);

}
#endif

void recordTest(argv)
char *argv[];
{
	char str[MAXLINE];

	char ioType[MAX_STRING_SIZE];
	char name[MAX_STRING_SIZE];
	char value[MAX_STRING_SIZE];
	char verify[MAX_STRING_SIZE];
	chid chanId;
	int  count;
	long status;

	/* get test line */
	while ( gets(str)) {

		/* print and skip null lines and comment lines */
		if ( str[0]==NULL || str[0] == '!' ) {
			printf("%s\n",str);
			continue;
		}

		/* parse input line for fields */
		count=sscanf(str,"%s %s \"%[^\"]\" %s",ioType,name,value,verify);

		/* check field count */
		if ( count != FIELDS ) {
			printf("\n%s\n",str);
			printf ("***** ERROR ***** field count %d is not equal to %d\n",count,FIELDS);
			continue;
		}

		/* check 1st char of name */
		if (((*name < ' ') || (*name > 'z'))){
			printf("\n%s\n",str);
			printf ("***** ERROR ***** pv name check error ");
			continue;
		}

		/* check for blank string value */
		if ( *value == ' ' ) *value = '\0';

		/* connect to a channel */
		status = connectChan(name,&chanId);
		if( status ){
			printf("\n%s\n",str);
			printf ("***** ERROR ***** %s \n",errmsg);
			continue;
		}

		/* execute test line */
		status=doTestLine(name,ioType,chanId,value,verify);
		if ( status ){
			printf("\n%s\n",str);
			printf ("***** ERROR ***** status=%d %s\n",status,errmsg);
		}

		/* disconnect channel */
		status = disconnectChan(&chanId);
		if( status ){
			printf("\n%s\n",str);
			printf ("***** ERROR ***** %s \n",errmsg);
			continue;
		}

	}
	printf ("\nrecordTest ALL DONE\n\n\n");
}

int connectChan(name,pchanId)
char name[];
chid *pchanId;
{
	int  status=0;

	/* connect to a channel */
	status = ca_search(name, pchanId);
	if(status != ECA_NORMAL){
		strcpy(errmsg,"ca_search");
		return ERROR;
	}

	/* wait for connection */
	status = ca_pend_io(2.0);
	if(status != ECA_NORMAL){
		strcpy(errmsg,"ca_pend_io");
		return ERROR;
	}
	return OK;
}

int disconnectChan(pchanId)
chid *pchanId;
{

	/* terminate connection */
/*
	if(pchanId){
		status=ca_clear_channel(*pchanId);
		if(status != ECA_NORMAL){
			strcpy(errmsg,"ca_clear_channel");
			return ERROR;
		}
	}
*/
	return OK;
}

int doTestLine(name,ioType,chanId,value,verify)
char name[];
char *ioType;
chid chanId;
char value[];
char verify[];
{
	int  status=0;

	if ( *ioType == 'P'){
		status=putPV(name,chanId,value,verify);
	}
	else if ( *ioType == 'G'){
		status=getPV(name,chanId,value,verify);
	}
	else {
		status=ERROR;
		strcpy(errmsg,"doTestLine");
	}

	return status;
}



/* Read from a channel */

int getPV(name,chanId,value,verify)
char name[];
chid chanId;
char value[];
char verify[];
{	
	int		status;
	long		elementCount;
	char		valueChan[MAX_ELEMENTS] [MAX_STRING_SIZE];

	
	/*  fetch element count */
	elementCount=ca_element_count(chanId);

	/*  fetch value */
	status = ca_array_get(
				DBR_STRING, 
				elementCount,
				chanId, 
				valueChan);
	if(status != ECA_NORMAL){
		strcpy(errmsg,"ca_array_get");
		return ERROR;
	}

	status=ca_pend_io(2.0);
	if(status != ECA_NORMAL){
		strcpy(errmsg,"ca_pend_io");
		return ERROR;
	}

	/* verify channel value*/
	if (*verify == 'V'){

		status=strcmp(value,valueChan[0]);
		if (status) {
			sprintf(errmsg,"strcmp failed: valueChan=%s",valueChan);
			return ERROR;
		}
	}
	else printf(" %s=%s\n",name,valueChan);

	return OK;
}


/* Write to a process variable */
int putPV(name,chanId,value,verify)
char name[];
chid chanId;
char value[];
char verify[];
{
	char			valueChan[MAX_ELEMENTS] [MAX_STRING_SIZE];
	int			status;
	long 			elementCount;

	/*  fetch element count */
	elementCount=ca_element_count(chanId);


	/*  write value */
	status = ca_array_put( DBR_STRING, elementCount, chanId, value);
	if(status != ECA_NORMAL){
		strcpy(errmsg,"ca_array_put");
		return ERROR;
	}

	status=ca_pend_io(2.0);
	if(status != ECA_NORMAL){
		strcpy(errmsg,"ca_pend_io");
		return ERROR;
	}

	/* verify write */
	if (*verify == 'V'){

		status = ca_array_get( DBR_STRING, elementCount, chanId, valueChan);
		if(status != ECA_NORMAL){
			strcpy(errmsg,"ca_put");
			return ERROR;
		}
	
		status=ca_pend_io(2.0);
		if(status != ECA_NORMAL){
			strcpy(errmsg,"ca_pend_io failed");
			return ERROR;
		}
	
		/* compare channel value with value */
		status=strcmp(value,valueChan);
		if (status) {
			sprintf(errmsg,"strcmp failed: valueChan=%s",valueChan);
			return	ERROR;
		}
	}

	return	OK;
}

