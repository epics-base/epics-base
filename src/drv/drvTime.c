
/* drvTime.c */
/* share/src/drv $Id$ */

/* drvTime.c -  Driver Support Routines for Time */
#include	<vxWorks.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>


/* If any of the following does not exist replace it with #define <> NULL */
int report();
int init(); 

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init
} drvTime={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{
    register int i;

    time_io_report();
    return(0);
}

static long init()
{
    int status;

    time_driver_init();
    return(0);
}
