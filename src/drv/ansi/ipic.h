/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    ipic.h

Description:
    IndustryPack Interface Controller ASIC header file, giving the register 
    layout and programming model for the IPIC chip used on the MVME162.

Author:
    Andrew Johnson
Created:
    6 July 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/


#ifndef INCipicH
#define INCipicH

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Chip Registers */

#define IPIC_CHIP_ID 0x23
#define IPIC_CHIP_REVISION 0x00


/* Interrupt Control Register bits */

#define IPIC_INT_LEVEL 0x07
#define IPIC_INT_ICLR 0x08
#define IPIC_INT_IEN 0x10
#define IPIC_INT_INT 0x20
#define IPIC_INT_EDGE 0x40
#define IPIC_INT_PLTY 0x80


/* General Control Registers bits */

#define IPIC_GEN_MEN 0x01
#define IPIC_GEN_WIDTH 0x0c
#define IPIC_GEN_WIDTH_8 0x04
#define IPIC_GEN_WIDTH_16 0x08
#define IPIC_GEN_WIDTH_32 0x00
#define IPIC_GEN_RT 0x30
#define IPIC_GEN_RT_0 0x00
#define IPIC_GEN_RT_2 0x10
#define IPIC_GEN_RT_4 0x20
#define IPIC_GEN_RT_8 0x30
#define IPIC_GEN_ERR 0x80


/* IP Reset register bits */

#define IPIC_IP_RESET 0x01


/* Chip Structure */

typedef struct {
    uchar_t chipId;
    uchar_t chipRevision;
    uchar_t reserved1[2];
    ushort_t memBase[4];
    uchar_t memSize[4];
    uchar_t intCtrl[4][2];
    uchar_t genCtrl[4];
    uchar_t reserved2[3];
    uchar_t ipReset;
} ipic_t;


#ifdef __cplusplus
}
#endif

#endif /* INCipicH */

