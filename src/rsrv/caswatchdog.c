/*      
 *      Author: Jeffrey O. Hill
 *              hill@atdiv.lanl.gov
 *              (505) 665 1831
 *      Date:   5-92
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
 *      Modification Log:
 *      -----------------
 *
 *      Improvements
 *      ------------
 *
 *
 *
 */

static char *sccsId = "@(#)caswatchdog.c	1.3\t7/28/92";


#define CA_WD_DELAY 10

#include <ellLib.h>

void		ca_watchdog_check();
static
unsigned long	clk_rate;
static
unsigned long	wd_delay;

#define		MAX_TICK	(~((unsigned long)0))

int
ca_watchdog_task()
{
	clk_rate = sysClkRateGet();
	wd_delay = clk_rate * CA_WD_DELAY;

	while(TRUE){
		taskDelay(clk_rate);
		ca_watchdog_check();
	}
}

/*
 *
 *	!!!!!!! This will hang if there is a hung client
 *
 *	!!!!!!! cant leave the client queue lock on while
 *		doing the send !!!!!!
 *
 *
 */
static void
ca_watchdog_check()
{
	struct client 	*pc;
	unsigned long	current;

	current = tickGet();

        LOCK_CLIENTQ;
	pc = (struct client *) ellNext(&clientQ);
        while (pc) {

		if(pc->ticks_at_last_io > current){
			elapsed = (MAX_TICK - pc->ticks_at_last_io) + current;
		}

		if(elapsed > wd_delay){
        		LOCK_CLIENT(pc);
			cac_send_heartbeat(pc);
			cas_send_msg(pc, FALSE);
        		UNLOCK_CLIENT(pc);
		}

                pc = (struct client *) ellNext(pc);
        }
        UNLOCK_CLIENTQ;
}







}
/*      
 *      Author: Jeffrey O. Hill
 *              hill@atdiv.lanl.gov
 *              (505) 665 1831
 *      Date:   5-92
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
 *      Modification Log:
 *      -----------------
 *
 *      Improvements
 *      ------------
 *
 *
 *
 */


#define CA_WD_DELAY 10

void		ca_watchdog_check();
static
unsigned	clk_rate;
static
unsigned	wd_delay;

#define		MAX_TICK	(~((unsigned long)0))

int
ca_watchdog_task()
{
	clk_rate = sysClkRateGet();
	wd_delay = clk_rate * CA_WD_DELAY;

	while(TRUE){
		taskDelay(clk_rate);
		ca_watchdog_check();
	}
}

/*
 *
 *	!!!!!!! This will hang if there is a hung client
 *
 *	!!!!!!! cant leave the client queue lock on while
 *		doing the send !!!!!!
 *
 *
 */
static void
ca_watchdog_check()
{
	struct client 	*pc;
	unsigned	current;

	current = tickGet();

        LOCK_CLIENTQ;
	pc = (struct client *) ellNext(&clientQ);
        while (pc) {
		if(pc->ticks_at_last_io < current){
			elapsed = pc->ticks_at_last_io + 
				(-(int)current);
		}
		if(current - pc->ticks_at_last_io > wd_delay){
        		LOCK_CLIENT(pc);
			cac_send_heartbeat(pc);
			cas_send_msg(pc, FALSE);
        		UNLOCK_CLIENT(pc);
		}

                pc = (struct client *) ellNext(pc);
        }
        UNLOCK_CLIENTQ;
}







}
