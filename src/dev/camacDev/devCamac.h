/*************************************************************************\
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devCamac.h	Camac Device Support		*/
/*
 *      Author:          Johnny Tang
 *      Date:            4-14-94
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
	int		ext;
	int		mask;
        int             fsd; /*full scale for ai and ao*/
};
