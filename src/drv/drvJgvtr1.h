/* drvJgvtr1.h */
/* share/src/drv $Id$ */
/*
 *      Author:      Jeff Hill
 *      Date:        5-89
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



typedef long    jgvtr1Stat;

#define JGVTR1_SUCCESS  0

jgvtr1Stat jgvtr1_driver(
        unsigned        card,
        unsigned        *pcbroutine,
        unsigned        *parg   /* number of values read */
);

