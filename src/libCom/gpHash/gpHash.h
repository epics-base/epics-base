/* gpHash.h */
/* share/epicsH $Id$ */
/* Author:  Marty Kraimer Date:    04-07-94 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *
 * Modification Log:
 * -----------------
 * .01  04-07-94	mrk	Initial Implementation
 */

/* gph provides a general purpose directory accessed via a hash table*/
#ifndef INCgpHashh
#define INCgpHashh 1

#include <ellLib.h>
#ifdef vxWorks
#include <fast_lock.h>
#endif

typedef struct{
    ELLNODE	node;
    char	*name;		/*address of name placed in directory*/
    void	*pvtid;		/*private name for subsystem user*/
    void	*userPvt;	/*private for user*/
} GPHENTRY;

/*tableSize must be power of 2 in range 256 to 65536*/
void	gphInitPvt(void **ppvt,int tableSize);
GPHENTRY *gphFind(void *pvt,char *name,void *pvtid);
GPHENTRY *gphAdd(void *pvt,char *name,void *pvtid);
void gphDelete(void *pvt,char *name,void *pvtid);
void gphFreeMem(void *pvt);
void gphDump(void *pvt);
#endif /*INCgpHashh*/
