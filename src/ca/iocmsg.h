#ifndef __IOCMSG__
/*
 *	History
 * 	.01 01xx90 joh	removed status field in favor of a independent m_cmmd- 
 *			saves space on every successful operation
 *
 *	.02 041390 joh	moved server ports to above IPPORT_USERRESERVED
 *			see in.h
 *
 *	.03 060391 joh	Bumped protocol version to 4 to support changes for
 *			SPARC alignment in db_access.h
 *	
 *	.04 071291 joh	New command added - claim channel in use block
 *
 *	.05 011294 joh	New command added - write notify 
 *
 *	.06 020194 joh	New command added for CA V4.1 - client name 
 *
 *	.07 041194 joh	New command added for CA V4.2 - access rights 
 *
 *	.08 050594 joh	New command added for CA V4.3 - echo request
 *
 *	.09 050594 joh	New command added for CA V4.3 - repeater fanout register 
 *
 *	.10 050594 joh	New command added for CA V4.3 - wakeup the server
 */

#define __IOCMSG__

HDRVERSIONID(iocmsgh, "@(#) $Id$ CA version 4.4")

/* TCP/UDP port number (bumped each protocol change) */
#define CA_PROTOCOL_VERSION	4
#define CA_MINOR_VERSION	4
#define CA_UKN_MINOR_VERSION	0 /* unknown minor version */
#if CA_PROTOCOL_VERSION == 4
#define CA_V41(MAJOR,MINOR)	((MINOR)>=1) 
#define CA_V42(MAJOR,MINOR)	((MINOR)>=2)
#define CA_V43(MAJOR,MINOR)	((MINOR)>=3)
#define CA_V44(MAJOR,MINOR)	((MINOR)>=4)
#elif CA_PROTOCOL_VERSION > 4
#define CA_V41(MAJOR,MINOR)	( 1 )
#define CA_V42(MAJOR,MINOR)	( 1 )
#define CA_V43(MAJOR,MINOR)	( 1 )
#define CA_V44(MAJOR,MINOR)	( 1 )
#else
#define CA_V41(MAJOR,MINOR)	( 0 )
#define CA_V42(MAJOR,MINOR)	( 0 )
#define CA_V43(MAJOR,MINOR)	( 0 )
#define CA_V44(MAJOR,MINOR)	( 0 )
#endif 

/*
 * NOTE: These port numbers are only used if the CA repeater and 
 * CA server port numbers cant be obtained from the EPICS 
 * environment variables "EPICS_CA_REPEATER_PORT" and
 * "EPICS_CA_SERVER_PORT"
 */
#define	CA_PORT_BASE		IPPORT_USERRESERVED + 56
#define CA_SERVER_PORT		(CA_PORT_BASE+CA_PROTOCOL_VERSION*2)
#define CA_REPEATER_PORT	(CA_PORT_BASE+CA_PROTOCOL_VERSION*2+1)

#define MAX_UDP			1024
#define MAX_TCP			(MAX_UDP*16) /* so waveforms fit */
#define MAX_MSG_SIZE		(MAX_TCP) /* the larger of tcp and udp max */

/*
 * architecture independent types
 *
 * (so far this works on all archs we have ported to)
 */
typedef unsigned short  ca_uint16_t;
typedef unsigned int	ca_uint32_t;
typedef float           ca_float32_t;


		/* values for m_cmmd */
#define IOC_NOOP		0	/* do nothing, but verify TCP */
#define IOC_EVENT_ADD		1	/* add an event */
#define IOC_EVENT_CANCEL	2	/* cancel an event */
#define IOC_READ		3	/* read and return a channel value*/
#define IOC_WRITE		4	/* write a channel value */
#define IOC_SNAPSHOT		5	/* snapshot of the system */
#define	IOC_SEARCH		6	/* IOC channel search */
#define IOC_BUILD		7	/* build - obsolete */
#define IOC_EVENTS_OFF		8	/* flow control */ 
#define IOC_EVENTS_ON		9	/* flow control */ 
#define IOC_READ_SYNC		10	/* purge old reads */ 
#define IOC_ERROR		11	/* an operation failed */ 
#define IOC_CLEAR_CHANNEL	12	/* free chan resources */
#define IOC_RSRV_IS_UP		13	/* CA server has joined the net */
#define IOC_NOT_FOUND		14	/* channel not found */
#define IOC_READ_NOTIFY		15	/* add a one shot event */
#define IOC_READ_BUILD		16	/* read and build - obsolete */
#define REPEATER_CONFIRM	17	/* registration confirmation */
#define IOC_CLAIM_CIU		18	/* client claims resource in server */
#define IOC_WRITE_NOTIFY	19	/* notify after write chan value */
#define IOC_CLIENT_NAME		20	/* CA V4.1 identify client */
#define IOC_HOST_NAME		21	/* CA V4.1 identify client */
#define IOC_ACCESS_RIGHTS	22	/* CA V4.2 asynch access rights chg */
#define IOC_ECHO		23	/* CA V4.3 connection verify */
#define REPEATER_REGISTER	24	/* registr for repeater fan out */
#define IOC_SIGNAL		25	/* knock the server out of select */

/*
 * for use with search and not_found (if search fails and
 * its not a broadcast tell the client to look elesewhere)
 */
#define DOREPLY		10
#define DONTREPLY	5

/* size of object in bytes rounded up to nearest oct word */
#define	OCT_ROUND(A)	((((unsigned long)A)+7)>>3)
#define	OCT_SIZEOF(A)	(OCT_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest long word */
#define	QUAD_ROUND(A)	(((unsigned long)A)+3)>>2)
#define	QUAD_SIZEOF(A)	(QUAD_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest short word */
#define	BI_ROUND(A)	((((unsigned long)A)+1)>>1)
#define	BI_SIZEOF(A)	(BI_ROUND(sizeof(A)))

/*
 * For communicating access rights to the clients
 *
 * (placed in m_available hdr field of IOC_ACCESS_RIGHTS cmmd
 */
#define CA_ACCESS_RIGHT_READ	(1<<0)
#define CA_ACCESS_RIGHT_WRITE	(1<<1)

/*
 * All structures passed in the protocol must have individual
 * fields aligned on natural boundaries.
 *
 * NOTE: all structures declared in this file must have a
 * byte count which is evenly divisible by 8 matching
 * the largest atomic data type in db_access.h.
 */
#define CA_MESSAGE_ALIGN(A) (OCT_ROUND(A)<<3)

/*
 * the common part of each message sent/recv by the
 * CA server.
 */
typedef struct	extmsg {
	ca_uint16_t	m_cmmd;		/* operation to be performed */
	ca_uint16_t	m_postsize;	/* size of message extension */	
	ca_uint16_t	m_type;		/* operation data type */ 
	ca_uint16_t	m_count;	/* operation data count */
	ca_uint32_t	m_cid;		/* channel identifier */
	ca_uint32_t	m_available;	/* undefined message location for use
					 * by client processes */
}caHdr;

/*
 * for  monitor (event) message extension
 */
struct  mon_info{
	ca_float32_t	m_lval;		/* low delta */
	ca_float32_t	m_hval;		/* high delta */ 
	ca_float32_t	m_toval;	/* period btween samples */
	ca_uint16_t	m_mask;		/* event select mask */
	ca_uint16_t	m_pad;		/* extend to 32 bits */
};

struct	monops {			/* monitor req opi to ioc */
	struct extmsg	m_header;
	struct mon_info	m_info;
};

#endif
