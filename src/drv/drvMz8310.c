
/* drvMz8310.c */
/* share/src/drv $Id$ */

/* drvMz8310.c -  Driver Support Routines for Mz8310 */
#include	<vxWorks.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>


/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvMz8310={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{
    int card;

    for(card=0; card<tm_num_cards[MZ8310]; card++) 
	mz8310_io_report(card);
}

static long init()
{
    int status;

    return(0);
}
