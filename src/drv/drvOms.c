
/* drvOms.c */
/* share/src/drv $Id$ */

/* drvOms.c -  Driver Support Routines for Oms */
#include	<vxWorks.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<oms_sm_driver.h>

extern struct vmex_motor        *oms_motor_present[];   /* stepper motor - OMS */

/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvOms={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{
    register int i;

        for (i = 0; i < MAX_OMS_CARDS; i++)
                if (oms_motor_present[i])       printf("SM: OMS:     card %d\n",i);
}

static long init()
{
    int status;

    return(0);
}
