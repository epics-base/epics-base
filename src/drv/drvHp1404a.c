/*
 *
 * HP E1404A VXI bus slot zero translator
 * device dependent routines
 *
 * 	Author Jeffrey O. Hill
 * 	Date		030692
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 *
 *
 *
 */

#include <epvxiLib.h>

#define		TLTRIG(N) (1<<(N))
#define		ECLTRIG(N) (1<<((N)+8))

#define LOCAL static

#define NULL 0
#define TRUE 1
#define FALSE 0

/*
 * enable int when signal register is written
 */
#define HP1404A_INT_ENABLE 0x0008



/*
 *
 *	hpE1404Init
 *
 */
int
hpE1404Init(la)
unsigned        la;
{
	return hpE1404SetMsgSelfTestPassed(la);
}


/*
 *
 *	hpE1404SetSelfTestPassed
 *
 * 	set the self test status passed for the message based device
 *	
 *	
 */
LOCAL int
hpE1404SetMsgSelfTestPassed(la)
unsigned        la;             /* register based device's la */
{
        struct vxi_csr  *pcsr;

	pcsr = VXIBASE(la);

	pcsr->dir.w.dd.reg_s0.ddx1e = VXIPASS<<2;

	/*
	 * enable int when signal register is written
 	 */
	pcsr->dir.w.dd.reg.ddx1a = HP1404A_INT_ENABLE;

	return VXI_SUCCESS;
}


/*
 *
 *	hpE1404RouteTriggerECL
 *
 */
hpE1404RouteTriggerECL(la, enable_map, io_map)
unsigned        la;             /* slot zero device logical address     */
unsigned        enable_map;    /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map;         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
{
	struct vxi_csr	*pcsr;

	pcsr = VXIBASE(la);

	pcsr->dir.w.dd.reg_s0.ddx2a = (io_map&enable_map)<<8;
	pcsr->dir.w.dd.reg_s0.ddx22 = ((~io_map)&enable_map)<<8;

	return VXI_SUCCESS;
}


/*
 *
 *
 *	hpE1404RouteTriggerTTL
 *
 *
 */
hpE1404RouteTriggerTTL(la, enable_map, io_map)
unsigned        la;             /* slot zero device logical address     */
unsigned        enable_map;    /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map;         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
{
        struct vxi_csr  *pcsr;

        pcsr = VXIBASE(la);

        pcsr->dir.w.dd.reg_s0.ddx2a = io_map&enable_map;
        pcsr->dir.w.dd.reg_s0.ddx22 = (~io_map)&enable_map;

	return VXI_SUCCESS;
}
