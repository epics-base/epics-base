
/* drvXy566.c */
/* share/src/drv $Id$ */

/* drvXy566.c -  Driver Support Routines for xy566 */
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
} drvXy566={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{
    xy566_io_report();
    return(0);
}

static long init()
{

    return(0);
}
