
/* drvAt5Vxi.c */
/* share/src/drv $Id$ */

/* drvAt5Vxi.c -  Driver Support Routines for At5Vxi */
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
} drvAt5Vxi={
	2,
	report,
	init};


static long report(fp)
    FILE	*fp;
{
    register int i;

	vxi_io_report();
}

static long init()
{
    int status;

    at5vxi_init();
    return(0);
}
