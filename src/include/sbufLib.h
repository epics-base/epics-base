/*
 *	$Id$
 *
 *	Shared Buffer Library Header File
 *
 *      Author: Jeffrey O. Hill
 *              johill@lanl.gov
 *              (505) 665 1831
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
 *      Modification Log:
 *      -----------------
 *	$Log$
 * Revision 1.1  1995/09/29  21:42:26  jhill
 * installed into CVS
 *
 *
 *	NOTES:
 */

#ifndef INCsbufLibh
#define INCsbufLibh 

#include <errMdef.h>

#ifndef READONLY
#define READONLY const
#endif

typedef unsigned long	sbufIndex;
typedef int 		sbufStatus;
typedef void 		userFreeFunc(void *pUserArg);
typedef	READONLY void 	*SBUFID;
typedef	READONLY void 	*SSEGID;

#ifdef __STDC__

SBUFID 		sbufCreate (void);
sbufStatus	sbufDelete (SBUFID id);
void		sbufShowAll (void);
void		sbufShow (SBUFID id);
sbufIndex 	sbufSize (SBUFID id); 

/*
 *
 * To avoid redundant copying of large buffers
 * the buffer provider is required to supply a
 * function pointer (pFree) which is called when
 * consumer is finished with the buffer.
 * The provider must _not_ change or free the
 * buffer until the pFree call back is called.
 */
/*
 * data segment is added to the end of the sbuf
 */
sbufStatus	sbufAppend (SBUFID id, READONLY void *pData, 
	sbufIndex dataSize, userFreeFunc *pFree, READONLY void *pFreeArg);
/*
 * data segment is inserted at the beginning of the sbuf
 */
sbufStatus	sbufPrefix (SBUFID id, READONLY void *pData, 
	sbufIndex dataSize, userFreeFunc *pFree, READONLY void *pFreeArg);

/*
 * A shared segment is removed from the beginning of a shared buffer
 * (it is the application's responsability to dispose of the
 * segment when it is finished with it)
 */
sbufStatus 	sbufPopPrefix(SBUFID sBufId, SSEGID *pSSegId, 
			READONLY void **ppData, sbufIndex *pDataSize);
/*
 * A shared segment is removed from the end of a shared buffer
 * (it is the application's responsability to dispose of the
 * segment when it is finished with it)
 */
sbufStatus 	sbufPopAppendix(SBUFID sBufId, SSEGID *pSSegId, 
			READONLY void **ppData, sbufIndex *pDataSize);

/*
 * Delete a segment 
 *
 * (free routine is called)
 */
sbufStatus	sbufSegDelete (SSEGID id);

#else /*__STDC__*/
SBUFID 		sbufCreate ();
sbufStatus	sbufDelete ();
sbufStatus	sbufShowAll ();
sbufStatus	sbufShow ();
sbufIndex 	sbufSize (); 
sbufStatus	sbufAppend ();
sbufStatus	sbufPrefix ();
sbufStatus	sbufPopPrefix ();
sbufStatus	sbufPopAppendix ();
#endif /*__STDC__*/

/*
 * Status returned by sbufLib functions
 */
#define S_sbuf_success	0
#define S_sbuf_noMemory	(M_sbuf | 1) 	/*Memory allocation failed*/
#define S_sbuf_uknId	(M_sbuf | 2) 	/*Unknown identifier*/
#define S_sbuf_empty	(M_sbuf | 3) 	/*shared buffer is empty*/

#endif /*INCsbufLibh*/

