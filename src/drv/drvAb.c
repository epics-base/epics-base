
/* drvAb.c */
/* share/src/drv $Id$ */

/* drvAb.c -  Driver Support Routines for Alleb Bradley */
#include	<vxWorks.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<ab_driver.h>
		/* AllenBradley serial link and ai,ao,bi and bo cards */

extern struct ab_region		*p6008s[];   /* AllenBradley serial io link */

extern short	ab_link_to[AB_MAX_LINKS];
extern short	ab_comm_to[AB_MAX_LINKS];
extern short	ab_bad_response[AB_MAX_LINKS];
extern short   ab_or_scaling_error[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
extern short	ab_scaling_error[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
extern short	ab_data_to[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
extern short	ab_cmd_to[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
/* Allen-Bradley cards which have been addressed */
extern unsigned short	ab_config[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];

/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvAb={
	2,
	report,
	init};



static long report(fp)
    FILE	*fp;
{
	register short i,card,adapter;

	/* report all of the Allen-Bradley Serial Links present */
	for (i = 0; i < AB_MAX_LINKS; i++){
	    if (p6008s[i])
		printf("AB-6008SV: Card %d\tcto: %d lto: %d badres: %d\n"
		  ,i,ab_comm_to[i],ab_link_to[i],ab_bad_response[i]);
	    else continue;

	    /* report all cards to which the database has interfaced */
	    /* as there is no way to poll the Allen-Bradley IO to      */
	    /* determine which card is there we assume that any interface */
	    /* which is successful implies the card type is correct       */
	    /* since all binaries succeed and some analog inputs will     */
	    /* succeed for either type this is a shakey basis             */
	    for (adapter = 0; adapter < AB_MAX_ADAPTERS; adapter++){
		for (card = 0; card < AB_MAX_CARDS; card++){
		    switch (ab_config[i][adapter][card] & AB_INTERFACE_TYPE){
		    case (AB_BI_INTERFACE):
			printf("\tAdapter %d Card %d:\tBI",adapter,card);
			break;
		    case (AB_BO_INTERFACE):
			    printf("\tAdapter %d Card %d:\tBO",adapter,card);
			    break;
		    case (AB_AI_INTERFACE):
			if ((ab_config[i][adapter][card]&AB_CARD_TYPE)==AB1771IXE)
			    printf("\tAdapter %d Card %d:\tAB1771IXE\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
			else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IL)
			    printf("\tAdapter %d Card %d:\tAB1771IL\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
			else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IFE_SE)
			    printf("\tAdapter %d Card %d:\tAB1771IFE_SE\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
			else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IFE_4to20MA)
			    printf("\tAdapter %d Card %d:\tAB1771IFE_4to20MA\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
			else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IFE)
			    printf("\tAdapter %d Card %d:\tAB1771IFE\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
			break;
		    case (AB_AO_INTERFACE):
			printf("\tAdapter %d Card %d:\tAB1771OFE\tcto: %d dto: %d",
			  adapter,card,ab_cmd_to[i][adapter][card],
			  ab_data_to[i][adapter][card]);
			break;
		    default:
			continue;
		    }
		    if ((ab_config[i][adapter][card] & AB_INIT_BIT) == 0)
			printf(" NOT INITIALIZED\n");
		    else printf("\n");
		}
	    }
	}

}

/* forward reference for ioc_reboot */
int ioc_reboot();

static long init()
{
    int status;

    rebootHookAdd(ioc_reboot);
    ab_driver_init();
    return(0);
}

/* ioc_reboot - routine to call when IOC is rebooted with a control-x */

extern int abScanId;
short	   ab_disable=0;

int ioc_reboot(boot_type)
int	boot_type;
{
	short i;
	static char	wait_msg[] = {"I Hate to WAIT"};
	register char	*pmsg = &wait_msg[0];

	/* Stop communication to the Allen-Bradley Scanner Cards */
	if (abScanId != 0){
		/* flag the analog output and binary IO routines to stop */ 
		ab_disable = 1;

		/* delete the scan task stops analog input communication */
		taskDelete(abScanId);

		/* this seems to be necessary for the AB card to stop talking */
		printf("\nReboot: delay ");
		for(i = 0; i <= 14; i++){
			printf("%c",*(pmsg+i));
			taskDelay(20);
		}
	}
}
