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
 */

#define __IOCMSG__

/* TCP/UDP port number (bumped each protocol change) */
#define CA_PROTOCOL_VERSION	4
#define	CA_PORT_BASE		IPPORT_USERRESERVED + 56
#define CA_SERVER_PORT		(CA_PORT_BASE+CA_PROTOCOL_VERSION*2)
#define CA_CLIENT_PORT		(CA_PORT_BASE+CA_PROTOCOL_VERSION*2+1)

#define MAX_UDP			1024
#define MAX_TCP			(MAX_UDP*16) /* so waveforms fit */
#define MAX_MSG_SIZE		(MAX_TCP) /* the larger of tcp and udp max */


		/* values for m_cmmd */
#define IOC_NOOP		0	/* do nothing, but verify TCP */
#define IOC_EVENT_ADD		1	/* add an event */
#define IOC_EVENT_CANCEL	2	/* cancel an event */
#define IOC_READ		3	/* read and return a channel value*/
#define IOC_WRITE		4	/* write a channel value */
#define IOC_SNAPSHOT		5	/* snapshot of the system */
#define	IOC_SEARCH		6	/* IOC channel search */
#define	IOC_BUILD		7	/* Conglomerate of IOC_SEARCH */
					/* IOC_READ */
					/* (optimizes OPI network use)	*/
#define IOC_EVENTS_OFF		8	/* flow control */ 
#define IOC_EVENTS_ON		9	/* flow control */ 
#define IOC_READ_SYNC		10	/* purge old reads */ 
#define IOC_ERROR		11	/* an operation failed */ 
#define IOC_CLEAR_CHANNEL	12	/* free chan resources */
#define IOC_RSRV_IS_UP		13	/* CA server has joined the net */
#define IOC_NOT_FOUND		14	/* channel not found */
#define IOC_READ_NOTIFY		15	/* add a one shot event */
#define IOC_READ_BUILD		16	/* read accompanying a build */
#define REPEATER_CONFIRM	17	/* registration confirmation */
#define IOC_CLAIM_CIU		18	/* client claims resource in server */

/*
 * for use with build and search and not_found (if search fails and
 * its not a broadcast tell the client to look elesewhere)
 */
#define DOREPLY		10
#define DONTREPLY	5

/* size of object in bytes rounded up to nearest oct word */
#define	OCT_ROUND(A)	(((A)+7)>>3)
#define	OCT_SIZEOF(A)	(OCT_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest long word */
#define	QUAD_ROUND(A)	(((A)+3)>>2)
#define	QUAD_SIZEOF(A)	(QUAD_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest short word */
#define	BI_ROUND(A)	(((A)+1)>>1)
#define	BI_SIZEOF(A)	(BI_ROUND(sizeof(A)))

/*
 * Required Message Alignment 
 *
 * Determined by the architecture with the most restrictive
 * alignment requirements (currently the SPARC).
 *
 * octal rounding
 *
 * NOTE: all structures declared in this file must have a
 * byte count which is evenly divisible by 8 for the SPARC.
 */
#define CA_MESSAGE_ALIGN(A) (OCT_ROUND(A)<<3)

/*
 * the common part of each message sent/recv by the
 * CA server.
 */
struct	extmsg {
	unsigned short	m_cmmd;		/* operation to be performed */
	unsigned short	m_postsize;	/* size of message extension */	
	unsigned short	m_type;		/* operation data type */ 
	unsigned short 	m_count;	/* operation data count */
	void		*m_pciu;	/* ptr to server channel in use */
	unsigned long	m_available;	/* undefined message location for use
					 * by client processes */
};


/*
 * for  monitor (event) message extension
 */
struct  mon_info{
	float		m_lval;		/* low delta */
	float		m_hval;		/* high delta */ 
	float		m_toval;	/* period btween samples */
	unsigned short	m_mask;		/* event select mask */
	unsigned short	m_pad;		/* extend to 32 bits */
};

struct	monops {			/* monitor req opi to ioc */
	struct extmsg	m_header;
	struct mon_info	m_info;
};

#endif
