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

#ifndef INC_SCANchoice_h
#define INC_SCANchoice 1

#define SPC_SC_S                111   /* start position   */
#define SPC_SC_I                112   /* step increment   */
#define SPC_SC_E                113   /* end position     */
#define SPC_SC_C                114   /* center position  */
#define SPC_SC_W                115   /* width            */
#define SPC_SC_N                116   /* # of steps       */
#define SPC_SC_FFO              117   /* FrzFlag Override */
#define SPC_SC_F                118   /* FrzFlag Changing */
#define SPC_SC_DPT              119   /* Desired Pt       */
#define SPC_SC_MO               120   /* scan mode        */

#define REC_SC_P1               130
#define REC_SC_P2               131
#define REC_SC_P3               132
#define REC_SC_P4               133

#define REC_SC_R1               134
#define REC_SC_R2               135
#define REC_SC_R3               136
#define REC_SC_R4               137

#define REC_SC_D1               138
#define REC_SC_D2               139
#define REC_SC_D3               140
#define REC_SC_D4               141
#define REC_SC_D5               142
#define REC_SC_D6               143
#define REC_SC_D7               144
#define REC_SC_D8               145
#define REC_SC_D9               146
#define REC_SC_D10              147
#define REC_SC_D11              148
#define REC_SC_D12              149
#define REC_SC_D13              150
#define REC_SC_D14              151
#define REC_SC_D15              152

#define REC_SC_T1               153
#define REC_SC_T2               154

#define REC_SC_BS               155
#define REC_SC_AS               156

#define REC_SCAN_MO		0
#define REC_SCAN_MO_LIN   	0
#define REC_SCAN_MO_TAB 	1
#define REC_SCAN_MO_OTF  	2

#define REC_SCAN_FRZ            1
#define REC_SCAN_FRZ_NO   	0
#define REC_SCAN_FRZ_YES	1

#define REC_SCAN_FFO            2
#define REC_SCAN_FFO_NO   	0
#define REC_SCAN_FFO_YES	1

#define REC_SCAN_AS             3
#define REC_SCAN_AS_NO          0
#define REC_SCAN_AS_ST          1
#define REC_SCAN_AS_BS          2

#define REC_SCAN_AR             4
#define REC_SCAN_AR_ABS         0
#define REC_SCAN_AR_REL         1

#define REC_SCAN_DYNL           5
#define REC_SCAN_DYNL_OK        0
#define REC_SCAN_DYNL_NC        1
#define REC_SCAN_DYNL_NO_PV     2

#endif
