/* base/src/drv $Id$ */

/*
 *
 *      driver for at5 designed VXI modules
 *
 *      Author:      Jeff Hill
 *      Date:        11-89
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
 *      Modification Log:
 *      -----------------
 */

#include <dbScan.h>

typedef long    at5VxiStatus;

at5VxiStatus    at5vxi_one_shot(
        unsigned	preset,         /* TRUE or COMPLEMENT logic */
        double          edge0_delay,    /* sec */
        double          edge1_delay,    /* set */
        unsigned        card,           /* 0 through ... */
        unsigned        channel,        /* 0 through channels on a card */
        unsigned	int_source,     /* (FALSE)External/(TRUE)Internal source */
        void            (*event_rtn)(void *pParam), /* subroutine to run on events */
        void		*event_rtn_param/* parameter to pass to above routine */
);

at5VxiStatus    at5vxi_one_shot_read(
        unsigned	*preset,        /* TRUE or COMPLEMENT logic */
        double          *edge0_delay,   /* sec */
        double          *edge1_delay,   /* sec */
        unsigned        card,           /* 0 through ... */
        unsigned        channel,        /* 0 through channels on a card */
        unsigned	*int_source     /* (FALSE)External/(TRUE)Internal src */
);

at5VxiStatus    at5vxi_ai_driver(
        unsigned        card,
        unsigned        chan,
        unsigned short  *prval
);

at5VxiStatus    at5vxi_ao_driver(
        unsigned        card,
        unsigned        chan,
        unsigned short  *prval,
        unsigned short  *prbval
);

at5VxiStatus    at5vxi_ao_read(
        unsigned        card,
        unsigned        chan,
        unsigned short  *pval
);

at5VxiStatus    at5vxi_bi_driver(
        unsigned        card,
        unsigned long   mask,
        unsigned long   *prval
);

at5VxiStatus    at5vxi_bo_driver(
        unsigned        card,
        unsigned long   val,
        unsigned long   mask
);

at5VxiStatus at5vxi_getioscanpvt(
        unsigned        card,
        IOSCANPVT       *scanpvt
);

