/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
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
 *
 * History
 * $Log$
 * Revision 1.3  1997/01/22 21:08:17  jhill
 * removed use of ## for VAXC port
 *
 * Revision 1.2  1996/09/16 16:32:49  jhill
 * added CA version string
 *
 * Revision 1.1  1996/06/20 18:02:22  jhill
 * installed into CVS
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#ifndef __CAPROTO__
/* $Id$ */
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
 *	.09 050594 joh	New command added for CA V4.3 - repeater fanout regis 
 *
 *	.10 050594 joh	New command added for CA V4.3 - wakeup the server
 * $Log$
 * Revision 1.3  1997/01/22 21:08:17  jhill
 * removed use of ## for VAXC port
 *
 * Revision 1.2  1996/09/16 16:32:49  jhill
 * added CA version string
 *
 * Revision 1.1  1996/06/20 18:02:22  jhill
 * installed into CVS
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#define __CAPROTO__ 

/* 
 * CA protocol number
 * TCP/UDP port number (bumped each major protocol change) 
 */
#define CA_PROTOCOL_VERSION	4u
#define CA_MINOR_VERSION	8u
#define CA_VERSION_STRING 	"4.8"
#define CA_UKN_MINOR_VERSION	0u /* unknown minor version */
#if CA_PROTOCOL_VERSION == 4u
#define CA_V41(MAJOR,MINOR)	((MINOR)>=1u) 
#define CA_V42(MAJOR,MINOR)	((MINOR)>=2u)
#define CA_V43(MAJOR,MINOR)	((MINOR)>=3u)
#define CA_V44(MAJOR,MINOR)	((MINOR)>=4u)
#define CA_V45(MAJOR,MINOR)	((MINOR)>=5u)
#define CA_V46(MAJOR,MINOR)	((MINOR)>=6u)
#define CA_V47(MAJOR,MINOR)	((MINOR)>=7u)
#define CA_V48(MAJOR,MINOR)	((MINOR)>=8u)
#elif CA_PROTOCOL_VERSION > 4u
#define CA_V41(MAJOR,MINOR)	( 1u )
#define CA_V42(MAJOR,MINOR)	( 1u )
#define CA_V43(MAJOR,MINOR)	( 1u )
#define CA_V44(MAJOR,MINOR)	( 1u )
#define CA_V45(MAJOR,MINOR)	( 1u )
#define CA_V46(MAJOR,MINOR)	( 1u )
#define CA_V47(MAJOR,MINOR)	( 1u )
#define CA_V48(MAJOR,MINOR)	( 1u )
#else
#define CA_V41(MAJOR,MINOR)	( 0u )
#define CA_V42(MAJOR,MINOR)	( 0u )
#define CA_V43(MAJOR,MINOR)	( 0u )
#define CA_V44(MAJOR,MINOR)	( 0u )
#define CA_V45(MAJOR,MINOR)	( 0u )
#define CA_V46(MAJOR,MINOR)	( 0u )
#define CA_V47(MAJOR,MINOR)	( 0u )
#define CA_V48(MAJOR,MINOR)	( 0u )
#endif 

/*
 * NOTE: These port numbers are only used if the CA repeater and 
 * CA server port numbers cant be obtained from the EPICS 
 * environment variables "EPICS_CA_REPEATER_PORT" and
 * "EPICS_CA_SERVER_PORT"
 */
#define	CA_PORT_BASE		IPPORT_USERRESERVED + 56U
#define CA_SERVER_PORT		(CA_PORT_BASE+CA_PROTOCOL_VERSION*2u)
#define CA_REPEATER_PORT	(CA_PORT_BASE+CA_PROTOCOL_VERSION*2u+1u)

#define MAX_UDP			1024u
#define MAX_TCP			(MAX_UDP*16u) /* so waveforms fit */
#define MAX_MSG_SIZE		(MAX_TCP) /* the larger of tcp and udp max */

/*
 * architecture independent types
 *
 * (so far this works on all archs we have ported to)
 */
typedef unsigned short  ca_uint16_t;
typedef unsigned int	ca_uint32_t;
typedef float           ca_float32_t;
typedef ca_uint32_t	caResId;

		/* values for m_cmmd */
#define CA_PROTO_NOOP		0u	/* do nothing, but verify TCP */
#define CA_PROTO_EVENT_ADD	1u	/* add an event */
#define CA_PROTO_EVENT_CANCEL	2u	/* cancel an event */
#define CA_PROTO_READ		3u	/* read and return a channel value*/
#define CA_PROTO_WRITE		4u	/* write a channel value */
#define CA_PROTO_SNAPSHOT	5u	/* snapshot of the system */
#define	CA_PROTO_SEARCH		6u	/* IOC channel search */
#define CA_PROTO_BUILD		7u	/* build - obsolete */
#define CA_PROTO_EVENTS_OFF	8u	/* flow control */ 
#define CA_PROTO_EVENTS_ON	9u	/* flow control */ 
#define CA_PROTO_READ_SYNC	10u	/* purge old reads */ 
#define CA_PROTO_ERROR		11u	/* an operation failed */ 
#define CA_PROTO_CLEAR_CHANNEL	12u	/* free chan resources */
#define CA_PROTO_RSRV_IS_UP	13u	/* CA server has joined the net */
#define CA_PROTO_NOT_FOUND	14u	/* channel not found */
#define CA_PROTO_READ_NOTIFY	15u	/* add a one shot event */
#define CA_PROTO_READ_BUILD	16u	/* read and build - obsolete */
#define REPEATER_CONFIRM	17u	/* registration confirmation */
#define CA_PROTO_CLAIM_CIU	18u	/* client claims resource in server */
#define CA_PROTO_WRITE_NOTIFY	19u	/* notify after write chan value */
#define CA_PROTO_CLIENT_NAME	20u	/* CA V4.1 identify client */
#define CA_PROTO_HOST_NAME	21u	/* CA V4.1 identify client */
#define CA_PROTO_ACCESS_RIGHTS	22u	/* CA V4.2 asynch access rights chg */
#define CA_PROTO_ECHO		23u	/* CA V4.3 connection verify */
#define REPEATER_REGISTER	24u	/* registr for repeater fan out */
#define CA_PROTO_SIGNAL		25u	/* knock the server out of select */
#define CA_PROTO_CLAIM_CIU_FAILED 26u	/* unable to create chan resource in server */
#define CA_PROTO_SERVER_DISCONN	27u	/* server deletes PV (or channel) */

#define CA_PROTO_LAST_CMMD CA_PROTO_CLAIM_CIU_FAILED

/*
 * for use with search and not_found (if search fails and
 * its not a broadcast tell the client to look elesewhere)
 */
#define DOREPLY		10u
#define DONTREPLY	5u

/* size of object in bytes rounded up to nearest oct word */
#define	OCT_ROUND(A)	((((unsigned long)(A))+7u)>>3u)
#define	OCT_SIZEOF(A)	(OCT_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest long word */
#define	QUAD_ROUND(A)	(((unsigned long)(A))+3u)>>2u)
#define	QUAD_SIZEOF(A)	(QUAD_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest short word */
#define	BI_ROUND(A)	((((unsigned long)(A))+1u)>>1u)
#define	BI_SIZEOF(A)	(BI_ROUND(sizeof(A)))

/*
 * For communicating access rights to the clients
 *
 * (placed in m_available hdr field of CA_PROTO_ACCESS_RIGHTS cmmd
 */
#define CA_PROTO_ACCESS_RIGHT_READ	(1u<<0u)
#define CA_PROTO_ACCESS_RIGHT_WRITE	(1u<<1u)

/*
 * All structures passed in the protocol must have individual
 * fields aligned on natural boundaries.
 *
 * NOTE: all structures declared in this file must have a
 * byte count which is evenly divisible by 8 matching
 * the largest atomic data type in db_access.h.
 */
#define CA_MESSAGE_ALIGN(A) (OCT_ROUND(A)<<3u)

/*
 * the common part of each message sent/recv by the
 * CA server.
 */
typedef struct	ca_hdr {
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
	caHdr		m_header;
	struct mon_info	m_info;
};

/*
 * PV names greater than this length assumed to be invalid
 */
#define unreasonablePVNameSize 500u

#endif /* __CAPROTO__ */

