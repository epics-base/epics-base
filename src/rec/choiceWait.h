/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author:         Ned Arnold
 *      Date:           4-28-94
 *
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
