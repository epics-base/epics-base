/* $Id$ */
typedef long            bitMask;
#define bitSet(word, bitnum)	(word[bitnum/NBITS] |= (1<<(bitnum%NBITS)))
#define NBITS           32		/* # bits in bit mask word */
#define	NWRDS		8		/* # words in bit fields */
#define	MAGIC 920505			/* magic number for SPROG */
/* Bit encoding for run-time options */
#define	OPT_DEBUG	(1<<0)
#define	OPT_ASYNC	(1<<1)
#define	OPT_CONN	(1<<2)
#define	OPT_REENT	(1<<3)
#define	OPT_NEWEF	(1<<4)
#define	MAX_NDELAY	20		/* max # delays allowed in each SS */
