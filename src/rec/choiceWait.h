/*
 *      Author:         Ned Arnold
 *      Date:           4-28-94
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
 * .01  mm-dd-yy        iii     Comment
 */

#ifndef INC_WAITchoice_h
#define INC_WAITchoice 1

#define REC_WAIT_OUT_OPT               0
#define REC_WAIT_OUT_OPT_EVERY         0
#define REC_WAIT_OUT_OPT_CHANGE        1
#define REC_WAIT_OUT_OPT_WHEN_ZERO     2
#define REC_WAIT_OUT_OPT_WHEN_NZERO    3
#define REC_WAIT_OUT_OPT_CHG_TO_ZERO   4
#define REC_WAIT_OUT_OPT_CHG_TO_NZERO  5

#define REC_WAIT_DATA_OPT               1
#define REC_WAIT_DATA_OPT_VAL           0
#define REC_WAIT_DATA_OPT_DOL           1

#define REC_WAIT_INPP                   2
#define REC_WAIT_INPP_NPROC             0
#define REC_WAIT_INPP_PROC              1

#define REC_WAIT_DYNL                   3
#define REC_WAIT_DYNL_OK                0
#define REC_WAIT_DYNL_NC                1
#define REC_WAIT_DYNL_NO_PV             2

/* DON'T CHANGE THESE DEFINITIONS !!!!    */ 
/* Indexes  used to process dynamic links */
/* Must start above 103, because SPC_CALC = 103 */
#define REC_WAIT_A                       110
#define REC_WAIT_B                       111
#define REC_WAIT_C                       112
#define REC_WAIT_D                       113
#define REC_WAIT_E                       114
#define REC_WAIT_F                       115
#define REC_WAIT_G                       116
#define REC_WAIT_H                       117
#define REC_WAIT_I                       118
#define REC_WAIT_J                       119
#define REC_WAIT_K                       120
#define REC_WAIT_L                       121
#define REC_WAIT_O                       122
#define REC_WAIT_P                       123

#endif
