/* devCamac.h	Camac Device Support		*/
/*
 *      Author:          Johnny Tang
 *      Date:            4-14-94
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
 */

#define ADDRESS_SCAN         -1
#define BLOCK_TRANSFER        1

#define CDEBUG CAMAC_DEBUG
int CAMAC_DEBUG;
#define CONVERT               0
#define DO_NOT_CONVERT        2

/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};
/* DBF_STRING DBF_CHAR DBF_UCHAR DBF_SHORT DBF_USHORT DBF_LONG
   DBF_ULONG DBF_FLOAT DBF_DOUBLE DBF_ENUM
*/


struct dinfo{
	short		f;
	long		ext;
	long		mask;
};
