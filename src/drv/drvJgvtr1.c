
/* drvJgvtr1.c */
/* share/src/drv $Id$ */

/* drvJgvtr1.c -  Driver Support Routines for Jgvtr1 */
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
} drvJgvtr1={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{
    register int i;

    jgvtr1_io_report();
    return(0);
}

static long init()
{
    int status;

    return(0);
}
