
/* drvCompuSm.c */
/* share/src/drv $Id$ */

/* drvCompuSm.c -  Driver Support Routines for CompuSm */
#include	<vxWorks.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<compu_sm_driver.h>

extern struct compumotor        *pcompu_motors[];  /* stepper motor - CM1830 */

/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvCompuSm={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{
    register int i;

        for (i = 0; i < MAX_COMPU_MOTORS; i++)
                if (pcompu_motors[i])   fprintf(fp,"SM: CM1830:     card %d\n",i);
}

static long init()
{
    int status;

    return(0);
}
