
/* drvDvx.c */
/* share/src/drv $Id$ */

/* drvDvx.c -  Driver Support Routines for Dvx */
#include	<vxWorks.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<dvx_driver.h>


/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvDvx={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{
    register int i;

	fprintf(fp,"dvx report not implemented\n");
}

static long init()
{
    int status;

    return(0);
}
