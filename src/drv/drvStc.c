
/* drvStc.c */
/* share/src/drv $Id$ */

/* drvStc.c -  Driver Support Routines for Stc */
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
} drvStc={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{

    return(0);
}

static long init()
{
    int status;

    return(0);
}
