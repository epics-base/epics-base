/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/drv $Id$ */

/*
 *
 *      driver for at5 designed VXI modules
 *
 *      Author:      Jeff Hill
 *      Date:        11-89
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

