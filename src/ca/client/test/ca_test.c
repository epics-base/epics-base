/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author:	Jeff Hill
 *	Date:	07-01-91
 */

/*
 * ANSI
 */
#include <string.h>
#include <stdio.h>
#include <epicsStdlib.h>

#include "cadef.h"
#include "epicsTime.h"

int ca_test(char *pname, char *pvalue);
static int cagft(char *pname);
static void printit(struct  event_handler_args args);
static int capft(char *pname, char *pvalue);
static void verify_value(chid chan_id, chtype type);

static unsigned long outstanding;


/*
 *  	ca_test
 *
 *	find channel, write a value if supplied, and 
 *	read back the current value
 *
 */
int ca_test(
char	*pname,
char	*pvalue
)
{
    int status;
	if(pvalue){
		status = capft(pname,pvalue);
	}
	else{
		status = cagft(pname);
	}
    ca_task_exit();
    return status;
}



/*
 * 	cagft()
 *
 *	ca get field test
 *
 *	test ca get over the range of CA data types
 */
static int cagft(char *pname)
{	
	const unsigned maxTries = 1000ul;
	unsigned ntries = 0u;
	chid chan_id;
	int status;
	int i;

	/* 
	 *	convert name to chan id 
	 */
	status = ca_search(pname, &chan_id);
	SEVCHK(status,NULL);
	status = ca_pend_io(5.0);
	if(status != ECA_NORMAL){
        SEVCHK(ca_clear_channel(chan_id),NULL);
		printf("Not Found %s\n", pname);
		return -1;
	}

	printf("name:\t%s\n", 
        ca_name(chan_id));
	printf("native type:\t%s\n", 
        dbr_type_to_text(ca_field_type(chan_id)));
	printf("native count:\t%lu\n", 
        ca_element_count(chan_id));


	/* 
 	 * fetch as each type 
	 */
	for(i=0; i<=LAST_BUFFER_TYPE; i++){
		if(ca_field_type(chan_id)==DBR_STRING) {
			if( (i!=DBR_STRING)
			  && (i!=DBR_STS_STRING)
			  && (i!=DBR_TIME_STRING)
			  && (i!=DBR_GR_STRING)
              && (i!=DBR_CTRL_STRING)) {
                  continue;
            }
        }
        /* ignore write only types */
        if ( 
            i == DBR_PUT_ACKT || 
            i == DBR_PUT_ACKS ) {
                continue;
        }

		status = ca_array_get_callback(
				i, 
				ca_element_count(chan_id),
				chan_id, 
				printit, 
				NULL);
		SEVCHK(status, NULL);

		outstanding++;
	}

	/*
	 * wait for the operation to complete
	 * before returning 
	 */
	while ( ntries < maxTries ) {
		unsigned long oldOut;

		oldOut = outstanding;
		ca_pend_event ( 0.05 );

		if ( ! outstanding ) {
            SEVCHK ( ca_clear_channel ( chan_id ), NULL );
			printf ( "\n\n" );
			return 0;
		}

		if ( outstanding == oldOut ) {
			ntries++;
		}
	}

    SEVCHK ( ca_clear_channel ( chan_id ), NULL );
	return -1;
}


/*
 *	PRINTIT()
 */
static void printit ( struct event_handler_args args )
{
	if ( args.status == ECA_NORMAL ) {
		ca_dump_dbr ( args.type, args.count, args.dbr );
	}
	else {
        printf ( "%s\t%s\n", dbr_text[args.type], ca_message(args.status) );
	}

	outstanding--;
}

/*
 *	capft
 *
 *	test ca_put() over a range of data types
 *	
 */
static int capft(
char		*pname,
char		*pvalue
)
{
	dbr_short_t			shortvalue;
	dbr_long_t			longvalue;
	dbr_float_t			floatvalue;
	dbr_char_t			charvalue;
	dbr_double_t		doublevalue;
	unsigned long		ntries = 10ul;
	int					status;
	chid				chan_id;

	if (((*pname < ' ') || (*pname > 'z'))
	  || ((*pvalue < ' ') || (*pvalue > 'z'))){
		printf("\nusage \"pv name\",\"value\"\n");
		return -1;
	}

	/* 
	 *	convert name to chan id 
	 */
	status = ca_search(pname, &chan_id);
	SEVCHK(status,NULL);
	status = ca_pend_io(5.0);
	if(status != ECA_NORMAL){
        SEVCHK(ca_clear_channel(chan_id),NULL);
		printf("Not Found %s\n", pname);
		return -1;
	}

	printf("name:\t%s\n", ca_name(chan_id));
	printf("native type:\t%d\n", ca_field_type(chan_id));
	printf("native count:\t%lu\n", ca_element_count(chan_id));

	/*
	 *  string value ca_put
	 */
	status = ca_put(
			DBR_STRING, 
			chan_id, 
			pvalue);
	SEVCHK(status, NULL);
	verify_value(chan_id, DBR_STRING);

	if(ca_field_type(chan_id)==0)goto skip_rest;

	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
		/*
		 * short integer ca_put
		 */
		status = ca_put(
				DBR_SHORT, 
				chan_id, 
				&shortvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_SHORT);
		status = ca_put(
				DBR_ENUM, 
				chan_id, 
				&shortvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_ENUM);
		charvalue=(dbr_char_t)shortvalue;
		status = ca_put(
				DBR_CHAR, 
				chan_id, 
				&charvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_CHAR);
	}
	if(sscanf(pvalue,"%d",&longvalue)==1) {
		/*
		 * long integer ca_put
		 */
		status = ca_put(
				DBR_LONG, 
				chan_id, 
				&longvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_LONG);
	}
	if(epicsScanFloat(pvalue, &floatvalue)==1) {
		/*
		 * single precision float ca_put
		 */
		status = ca_put(
				DBR_FLOAT, 
				chan_id, 
				&floatvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_FLOAT);
	}
	if(epicsScanDouble(pvalue, &doublevalue)==1) {
		/*
		 * double precision float ca_put
		 */
		status = ca_put(
				DBR_DOUBLE, 
				chan_id, 
				&doublevalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_DOUBLE);
	}

skip_rest:

	/*
	 * wait for the operation to complete
	 * (outstabnding decrements to zero)
	 */
	while(ntries){
		ca_pend_event(1.0);

		if(!outstanding){
            SEVCHK(ca_clear_channel(chan_id),NULL);
			printf("\n\n");
			return 0;
		}

		ntries--;
	}

    SEVCHK(ca_clear_channel(chan_id),NULL);
	return -1;
}


/*
 * VERIFY_VALUE
 *
 * initiate print out the values in a database access interface structure
 */
static void verify_value(chid chan_id, chtype type)
{
	int status;

	/*
	 * issue a get which calls back `printit'
	 * upon completion
	 */
	status = ca_array_get_callback(
			type, 
			ca_element_count(chan_id),
			chan_id, 
			printit, 
			NULL);
	SEVCHK(status, NULL);

	outstanding++;
}
