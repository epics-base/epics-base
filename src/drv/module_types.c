/* module_types.c */
/* base/src/drv $Id$ */
/*
 * 	Author:      Marty Kraimer
 * 	Date:        08-23-93
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	08-23-93	mrk	Initial Version
 */

#include <module_types.h>

module_types()
{

ai_num_cards[AB1771IL]		= 12;
ai_num_cards[AB1771IFE]		= 12;
ai_num_cards[AB1771IXE]		= 12;
ai_num_cards[XY566SE]		= 2;
ai_num_cards[XY566DI]		= 2;
ai_num_cards[XY566DIL]		= 2;
ai_num_cards[VXI_AT5_AI]	= 32;
ai_num_cards[AB1771IFE_SE]	= 12;
ai_num_cards[AB1771IFE_4to20MA]	= 12;
ai_num_cards[DVX2502]		= 1;
ai_num_cards[AB1771IFE_0to5V]	= 12;
ai_num_cards[KSCV215]		= 32;

ai_num_channels[AB1771IL]		= 8;
ai_num_channels[AB1771IFE]		= 8;
ai_num_channels[AB1771IXE]		= 8;
ai_num_channels[XY566SE]		= 32;
ai_num_channels[XY566DI]		= 16;
ai_num_channels[XY566DIL]		= 16;
ai_num_channels[VXI_AT5_AI]		= 8;
ai_num_channels[AB1771IFE_SE]		= 16;
ai_num_channels[AB1771IFE_4to20MA]	= 8;
ai_num_channels[DVX2502]		= 127;
ai_num_channels[AB1771IFE_0to5V]	= 8;
ai_num_channels[KSCV215]		= 32;

ai_addrs[AB1771IL]		= 0;
ai_addrs[AB1771IFE]		= 0;
ai_addrs[AB1771IXE]		= 0;
ai_addrs[XY566SE]		= 0x6000;
ai_addrs[XY566DI]		= 0x7000;
ai_addrs[XY566DIL]		= 0x7800;
ai_addrs[VXI_AT5_AI]		= 0xc014;
ai_addrs[AB1771IFE_SE]		= 0;
ai_addrs[AB1771IFE_4to20MA]	= 0;
ai_addrs[DVX2502]		= 0xff00;
ai_addrs[AB1771IFE_0to5V]	= 0;
ai_addrs[KSCV215]		= 0;

ai_memaddrs[AB1771IL]		= 0;
ai_memaddrs[AB1771IFE]		= 0;
ai_memaddrs[AB1771IXE]		= 0;
ai_memaddrs[XY566SE]		= 0x000000;
ai_memaddrs[XY566DI]		= 0x040000;
ai_memaddrs[XY566DIL]		= 0x0c0000;
ai_memaddrs[VXI_AT5_AI]		= 0;
ai_memaddrs[AB1771IFE_SE]	= 0;
ai_memaddrs[AB1771IFE_4to20MA]	= 0;
ai_memaddrs[DVX2502]		= 0x100000;
ai_memaddrs[AB1771IFE_0to5V]	= 0;
ai_memaddrs[KSCV215]		= 0;

ao_num_cards[AB1771OFE]		= 12;
ao_num_cards[VMI4100]		= 4;
ao_num_cards[ZIO085]		= 1;
ao_num_cards[VXI_AT5_AO]	= 32;

ao_num_channels[AB1771OFE]	= 4;
ao_num_channels[VMI4100]	= 16;
ao_num_channels[ZIO085]		= 32;
ao_num_channels[VXI_AT5_AO]	= 16;

ao_addrs[AB1771OFE]		= 0;
ao_addrs[VMI4100]		= 0x4100;
ao_addrs[ZIO085]		= 0x0800;
ao_addrs[VXI_AT5_AO]		= 0xc000;

bi_num_cards[ABBI_08_BIT]	= 12;
bi_num_cards[ABBI_16_BIT]	= 12;
bi_num_cards[BB910]		= 4;
bi_num_cards[XY210]		= 2;
bi_num_cards[VXI_AT5_BI]	= 32;
bi_num_cards[HPE1368A_BI]	= 32;
bi_num_cards[AT8_FP10S_BI]	= 8;
bi_num_cards[XY240_BI]		= 2;

bi_num_channels[ABBI_08_BIT]	= 8;
bi_num_channels[ABBI_16_BIT]	= 16;
bi_num_channels[BB910]		= 32;
bi_num_channels[XY210]		= 32;
bi_num_channels[VXI_AT5_BI]	= 32;
bi_num_channels[HPE1368A_BI]	= 16;
bi_num_channels[AT8_FP10S_BI]	= 32;
bi_num_channels[XY240_BI]	= 32;

bi_addrs[ABBI_08_BIT]	= 0;
bi_addrs[ABBI_16_BIT]	= 0;
bi_addrs[BB910]		= 0xb800;
bi_addrs[XY210]		= 0xa000;
bi_addrs[VXI_AT5_BI]	= 0xc000;
bi_addrs[HPE1368A_BI]	= 0xc000;
bi_addrs[AT8_FP10S_BI]	= 0x0e00;
bi_addrs[XY240_BI]	= 0x3000;

bo_num_cards[ABBO_08_BIT]	= 12;
bo_num_cards[ABBO_16_BIT]	= 12;
bo_num_cards[BB902]		= 4;
bo_num_cards[XY220]		= 1;
bo_num_cards[VXI_AT5_BO]	= 32;
bo_num_cards[HPE1368A_BO]	= 32;
bo_num_cards[AT8_FP10M_BO]	= 2;
bo_num_cards[XY240_BO]		= 2;

bo_num_channels[ABBO_08_BIT]	= 8;
bo_num_channels[ABBO_16_BIT]	= 16;
bo_num_channels[BB902]		= 32;
bo_num_channels[XY220]		= 32;
bo_num_channels[VXI_AT5_BO]	= 32;
bo_num_channels[HPE1368A_BO]	= 16;
bo_num_channels[AT8_FP10M_BO]	= 32;
bo_num_channels[XY240_BO]	= 32;

bo_addrs[ABBO_08_BIT]	= 0;
bo_addrs[ABBO_16_BIT]	= 0;
bo_addrs[BB902]		= 0x0400;
bo_addrs[XY220]		= 0xa800;
bo_addrs[VXI_AT5_BO]	= 0xc000;
bo_addrs[HPE1368A_BO]	= 0xc000;
bo_addrs[AT8_FP10M_BO]	= 0xc000;
bo_addrs[XY240_BO]	= 0x3000;

sm_num_cards[CM57_83E]	= 4;
sm_num_cards[OMS_6AXIS]	= 4;

sm_num_channels[CM57_83E]	= 1;
sm_num_channels[OMS_6AXIS]	= 6;

sm_addrs[CM57_83E]	= 0x8000;
sm_addrs[OMS_6AXIS]	= 0x4000;

wf_num_cards[XY566WF]		= 2;
wf_num_cards[CAMAC_THING]	= 4;
wf_num_cards[JGVTR1]		= 4;
wf_num_cards[COMET]		= 4;

wf_num_channels[XY566WF]	= 1;
wf_num_channels[CAMAC_THING]	= 1;
wf_num_channels[JGVTR1]		= 1;
wf_num_channels[COMET]		= 4;

wf_addrs[XY566WF]	= 0x9000;
wf_addrs[CAMAC_THING]	= 0;
wf_addrs[JGVTR1]	= 0xB000;
wf_addrs[COMET]		= 0xbc00;

wf_armaddrs[XY566WF]	= 0x5400;
wf_armaddrs[CAMAC_THING]= 0;
wf_armaddrs[JGVTR1]	= 0;
wf_armaddrs[COMET]	= 0;

wf_memaddrs[XY566WF]	= 0x080000;
wf_memaddrs[CAMAC_THING]= 0;
wf_memaddrs[JGVTR1]	= 0xb80000;
wf_memaddrs[COMET]	= 0xe0000000;

tm_num_cards[MZ8310]		= 4;
tm_num_cards[DG535]		= 1;
tm_num_cards[VXI_AT5_TIME]	= 32;

tm_num_channels[MZ8310]		= 10;
tm_num_channels[DG535]		= 1;
tm_num_channels[VXI_AT5_TIME]	= 10;

tm_addrs[MZ8310]	= 0x1000;
tm_addrs[DG535]		= 0;
tm_addrs[VXI_AT5_TIME]	= 0xc000;

AT830X_1_addrs		= 0x0400;
AT830X_1_num_cards	= 2;
AT830X_addrs		= 0xaa0000;
AT830X_num_cards	= 2;

xy010ScA16Base		= 0x0000;

EPICS_VXI_LA_COUNT	= 32;
EPICS_VXI_A24_BASE	= (char *) 0x900000;
EPICS_VXI_A24_SIZE	= 0x100000;
EPICS_VXI_A32_BASE	= (char *) 0x90000000;
EPICS_VXI_A32_SIZE	= 0x10000000;


AI566_VNUM	= 0xf8;
DVX_IVEC0	= 0xd0;
MD_INT_BASE	= 0xf0;
MZ8310_INT_VEC_BASE = 0xe8;
AB_VEC_BASE	= 0x60;
JGVTR1_INT_VEC	= 0xe0;
AT830X_1_IVEC0	= 0xd4;
AT830X_IVEC0	= 0xd6;
AT8FP_IVEC_BASE	= 0xa2;
AT8FPM_IVEC_BASE= 0xaa;

BB_SHORT_OFF	= 0x1800;
BB_IVEC_BASE	= 0xa0;
BB_IRQ_LEVEL	= 5;
PEP_BB_SHORT_OFF= 0x1c00;
PEP_BB_IVEC_BASE= 0xe8;

NIGPIB_SHORT_OFF	= 0x5000;
NIGPIB_IVEC_BASE	= 100;
NIGPIB_IRQ_LEVEL	= 5;

return(0);
}
