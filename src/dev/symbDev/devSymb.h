/* devSymb.h - Header for vxWorks global var device support routines */

/* $Id$
 *
 * Author:	Andrew Johnson (RGO)
 * Date:	11 October 1996
 */

/* Modification History:
 * $Log$
 * Revision 1.1.1.1  1998/03/16 16:57:06  franksen
 * Imported using tkCVS
 *
 * Revision 1.1  1998/01/21 20:47:44  mrk
 * restructure; new Symb support
 *
 * Revision 1.1  1996/10/24 18:29:20  wlupton
 * Andrew Johnson's changes (upwards-compatible)
 *
 * Revision 1.1  1996/10/22 15:30:24  anj
 * vxWorks Variable support changed to do pointer indirection at
 * run-time.  Also added device support for the Waveform record.
 *
 */

/* This is the device private structure */

struct vxSym {
    void **ppvar;
    void *pvar;
    long index;
};


/* This is the call to create it */

extern int devSymbFind(char *name, struct link *plink, void *pdpvt);
