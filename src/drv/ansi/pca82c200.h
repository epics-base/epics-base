/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    pca82c200.h

Description:
    Philips Stand-alone CAN-controller chip header file, giving the register
    layout and programming model for the chip used on the TIP810 IP module.

Author:
    Andrew Johnson
Created:
    19 July 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/


#ifndef INCpca82c200H
#define INCpca82c200H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif


/***** Control Segment Bit Patterns *****/

/* Control Register */

#define PCA_CR_TM	0x80	/* Test Mode */
#define PCA_CR_S 	0x40	/* Synch */
#define PCA_CR_OIE	0x10	/* Overrun Interrupt Enable */
#define PCA_CR_EIE	0x08	/* Error Interrupt Enable */
#define PCA_CR_TIE	0x04	/* Transmit Interrupt Enable */
#define PCA_CR_RIE	0x02	/* Receive Interrupt Enable */
#define PCA_CR_RR	0x01	/* Reset Request */


/* Command Register */

#define PCA_CMR_GTS	0x10	/* Goto Sleep */
#define PCA_CMR_COS	0x08	/* Clear Overrun Status */
#define PCA_CMR_RRB	0x04	/* Release Receive Buffer */
#define PCA_CMR_AT	0x02	/* Abort Transmission */
#define PCA_CMR_TR	0x01	/* Transmission Request */


/* Status Register */

#define PCA_SR_BS	0x80	/* Bus Status */
#define PCA_SR_ES	0x40	/* Error Status */
#define PCA_SR_TS	0x20	/* Transmit Status */
#define PCA_SR_RS	0x10	/* Receive Status */
#define PCA_SR_TCS	0x08	/* Transmission Complete Status */
#define PCA_SR_TBS	0x04	/* Transmit Buffer Status */
#define PCA_SR_DO	0x02	/* Data Overrun */
#define PCA_SR_RBS	0x01	/* Receive Buffer Status */


/* Interrupt Register */

#define PCA_IR_WUI	0x10	/* Wake-Up Interrupt */
#define PCA_IR_OI	0x08	/* Overrun Interrupt */
#define PCA_IR_EI	0x04	/* Error Interrupt */
#define PCA_IR_TI	0x02	/* Transmit Interrupt */
#define PCA_IR_RI	0x01	/* Receive Interrupt */


/* Bus Timing Register 0 */

#define PCA_BTR0_1M6	0x00	/* 1.6 Mbits/sec,  20 m */
#define PCA_BTR0_1M0	0x00	/* 1.0 Mbits/sec,  40 m */
#define PCA_BTR0_500K	0x00	/* 500 Kbits/sec, 130 m */
#define PCA_BTR0_250K	0x01	/* 250 Kbits/sec, 270 m */
#define PCA_BTR0_125K	0x03	/* 125 Kbits/sec, 530 m */
#define PCA_BTR0_100K	0x43	/* 100 Kbits/sec, 620 m */
#define PCA_BTR0_50K	0x47	/*  50 Kbits/sec, 1.3 km */
#define PCA_BTR0_20K	0x53	/*  20 Kbits/sec, 3.3 km */
#define PCA_BTR0_10K	0x67	/*  10 Kbits/sec, 6.7 km */
#define PCA_BTR0_5K	0x7f	/*   5 Kbits/sec,  10 km */

#define PCA_KVASER_1M0	0x00	/* 1.0 Mbits/sec,  40 m -- Kvaser standard */
#define PCA_KVASER_500K	0x01	/* 500 Kbits/sec, 130 m -- Kvaser standard */
#define PCA_KVASER_250K	0x03	/* 250 Kbits/sec, 270 m -- Kvaser standard */
#define PCA_KVASER_125K	0x07	/* 125 Kbits/sec, 530 m -- Kvaser standard */


/* Bus Timing Register 1 */

#define PCA_BTR1_1M6	0x11	/* 1.6 Mbits/sec,  20 m */
#define PCA_BTR1_1M0	0x14	/* 1.0 Mbits/sec,  40 m */
#define PCA_BTR1_500K	0x1c	/* 500 Kbits/sec, 130 m */
#define PCA_BTR1_250K	0x1c	/* 250 Kbits/sec, 270 m */
#define PCA_BTR1_125K	0x1c	/* 125 Kbits/sec, 530 m */
#define PCA_BTR1_100K	0x2f	/* 100 Kbits/sec, 620 m */
#define PCA_BTR1_50K	0x2f	/*  50 Kbits/sec, 1.3 km */
#define PCA_BTR1_20K	0x2f	/*  20 Kbits/sec, 3.3 km */
#define PCA_BTR1_10K	0x2f	/*  10 Kbits/sec, 6.7 km */
#define PCA_BTR1_5K	0x7f	/*   5 Kbits/sec,  10 km */

#define PCA_BTR1_KVASER	0x23	/* All speeds -- Kvaser standard */

/* Output Control Register */

#define PCA_OCR_OCM_NORMAL	0x02
#define PCA_OCR_OCM_CLOCK	0x03
#define PCA_OCR_OCM_BIPHASE	0x00
#define PCA_OCR_OCM_TEST	0x01

#define PCA_OCR_OCT1_FLOAT	0x00
#define PCA_OCR_OCT1_PULLDOWN	0x40
#define PCA_OCR_OCT1_PULLUP	0x80
#define PCA_OCR_OCT1_PUSHPULL	0xc0

#define PCA_OCR_OCT0_FLOAT	0x00
#define PCA_OCR_OCT0_PULLDOWN	0x08
#define PCA_OCR_OCT0_PULLUP	0x10
#define PCA_OCR_OCT0_PUSHPULL	0x18

#define PCA_OCR_OCP1_INVERT	0x20
#define PCA_OCR_OCP0_INVERT	0x04


/* Message Buffers */

#define PCA_MSG_ID0_RSHIFT	3
#define PCA_MSG_ID1_LSHIFT	5
#define PCA_MSG_ID1_MASK	0xe0
#define PCA_MSG_RTR		0x10
#define PCA_MSG_DLC_MASK	0x0f


/***** Chip Structure *****/

/* Message Buffers */

typedef struct {
    uchar_t pad0;
    uchar_t descriptor0;
    uchar_t pad1;
    uchar_t descriptor1;
    ushort_t data[8];
} msgBuffer_t;


/* Chip Registers */

typedef volatile struct {
    uchar_t pad00;
    uchar_t control;
    uchar_t pad01;
    uchar_t command;
    uchar_t pad02;
    uchar_t status;
    uchar_t pad03;
    uchar_t interrupt;
    uchar_t pad04;
    uchar_t acceptanceCode;
    uchar_t pad05;
    uchar_t acceptanceMask;
    uchar_t pad06;
    uchar_t busTiming0;
    uchar_t pad07;
    uchar_t busTiming1;
    uchar_t pad08;
    uchar_t outputControl;
    uchar_t pad09;
    uchar_t test;
    msgBuffer_t txBuffer;
    msgBuffer_t rxBuffer;
    uchar_t pad31;
    uchar_t clockDivider;
} pca82c200_t;


#ifdef __cplusplus
}
#endif

#endif /* INCpca82c200H */

