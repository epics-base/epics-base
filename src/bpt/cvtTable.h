/* $Id$
 * Breakpoint Tables
 *
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
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
 * .01  05-18-92        rcz     removed extern
 * .02  05-18-92        rcz     new database access
 * .03  08-19-92        jba     add prototypes for cvtRawToEngBpt,cvtEngToRawBpt
 */

#ifndef INCcvtTableh
#define INCcvtTableh	1

/* Global Routines*/
long cvtEngToRawBpt(double *pval,short linr,short init,
     void **ppbrk,short *plbrk);

long cvtRawToEngBpt(double *pval,short linr,short init,
     void **ppbrk, short *plbrk);

#endif
