
#include <stdio.h>
#include "cadef.h"
#include "dbDefs.h"
#include "osiClock.h"

void event_handler(struct event_handler_args args);
int evtime(char *pname);

static unsigned 	iteration_count;
static unsigned		last_time;
static double		rate;

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
		return OK;
	}

	rate = clockGetRate();

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
	unsigned		current_time;
#	define 			COUNT	0x8000
	double			interval;
	double			delay;

	if(iteration_count%COUNT == 0){
		current_time = clockGetCurrentTick();
		if(last_time != 0){
			interval = current_time - last_time;
			delay = interval/(rate*COUNT);
			printf("Delay = %f sec for 1 event\n",
				delay,
				COUNT);
		}
		last_time = current_time;
	}

	iteration_count++;
}

