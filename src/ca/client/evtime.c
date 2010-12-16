/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>

#include "dbDefs.h"
#include "epicsTime.h"
#include "cadef.h"

void event_handler (struct event_handler_args args);
int evtime (char *pname);

static unsigned 	iteration_count;
static epicsUInt32	last_time;

#ifndef iocCore
int main(int argc, char **argv)
{
        char    *pname;

        if(argc == 2){
                pname = argv[1];
                evtime(pname);
        }
        else{
                printf("usage: %s <channel name>", argv[0]);
        }
	return(0);
}
#endif

/*
 * evtime()
 */
int evtime(char *pname)
{
	chid	chan;
	int	status;

	status = ca_search(pname,  &chan);
	SEVCHK(status, NULL);

	status = ca_pend_io(10.0);
	if(status != ECA_NORMAL){
		printf("%s not found\n", pname);
		return 0;
	}

	status = ca_add_event(
			DBR_FLOAT,
			chan,
			event_handler,
			NULL,
			NULL);
	SEVCHK(status, __FILE__);

	status = ca_pend_event(0.0);
	SEVCHK(status, NULL);
}


/*
 * event_handler()
 *
 */
void event_handler(struct event_handler_args args)
{
	epicsUInt32		current_time;
#	define 			COUNT	0x8000
	double			interval;
	double			delay;
        epicsTimeStamp		ts;

	if(iteration_count%COUNT == 0){
		epicsTimeGetCurrent(&ts);
		current_time = ts.secPastEpoch;
		if(last_time != 0){
			interval = current_time - last_time;
			delay = interval/COUNT;
			printf("Delay = %f sec per event\n",
				delay);
		}
		last_time = current_time;
	}

	iteration_count++;
}

