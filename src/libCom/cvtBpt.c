/* cvtBpt.c */
/* share/libCom/cvtBpt  $Id$ */
  
/* cvtBpt.c - Convert using breakpoint table
/*
 *      Author:          Janet Anderson
 *      Date:            9-19-91
 *
 *     Experimental Physics and Industrial Control System (EPICS)
 *
 *     Copyright 1991, the Regents of the University of California,
 *     and the University of Chicago Board of Governors.
 *
 *     This software was produced under  U.S. Government contracts:
 *     (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *     and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *     Initial development by:
 *          The Controls and Automation Group (AT-8)
 *          Ground Test Accelerator
 *          Accelerator Technology Division
 *          Los Alamos National Laboratory
 *
 *     Co-developed with
 *          The Controls and Computing Group
 *          Accelerator Systems Division
 *          Advanced Photon Source
 *          Argonne National Laboratory
 *
 *
 * Modification Log:
 * -----------------
 * nnn mm-dd-yy          nnn     Comment
 * .02  05-18-92        rcz     New database access
 * .03  12-11-92	mrk	ANSI prototypes
 */
#include     <vxWorks.h>
#include     <types.h>
#include     <stdioLib.h>

#include     <cvtTable.h>
#include     <dbAccess.h>
#include     <dbBase.h>

extern struct dbBase *pdbBase;



long cvtRawToEngBpt(
     double     *pval,
     short      linr,
     short      init,
     caddr_t    *ppbrk,
     short      *plbrk)
{ 
     double           val=*pval;
     long             status=0;
     struct brkTable *pbrkTable;
     struct brkInt   *pInt;     
     struct brkInt   *pnxtInt;
     short            lbrk;
     int              number;
     struct arrBrkTable *pcvtTable;

     if(linr < 2) return(-1);
     if(init==TRUE) { /*must find breakpoint table*/
         if( !(pcvtTable=pdbBase->pcvtTable) || (pcvtTable->number < linr)
         ||  (!(pcvtTable->papBrkTable[linr]))) {
                 errMessage(S_db_badField,"Breakpoint Table not Found");
                 return(S_db_badField);
         }
         *ppbrk = (caddr_t)(pcvtTable->papBrkTable[linr]);
         /* Just start at the beginning */
         *plbrk=0;
     }

     pbrkTable = (struct brkTable *)*ppbrk;
     number = pbrkTable->number;
     lbrk = *plbrk;
     /*make sure we dont go off end of table*/
     if( (lbrk+1) >= number ) lbrk--;
     pInt = pbrkTable->papBrkInt[lbrk];
     pnxtInt = pbrkTable->papBrkInt[lbrk+1];
     /* find entry for increased value */
     while( (pnxtInt->raw) <= val ) {
         lbrk++;
         pInt = pbrkTable->papBrkInt[lbrk];
         if( lbrk >= number-1) {
                status=1;
                break;
         }
         pnxtInt = pbrkTable->papBrkInt[lbrk+1];
     }
     while( (pInt->raw) > val) {
         if(lbrk==0) {
                status=1;
                break;
            }
         lbrk--;
         pInt = pbrkTable->papBrkInt[lbrk];
     }
     *plbrk = lbrk;
     *pval = pInt->eng + (val - pInt->raw) * pInt->slope;
     return(status);
}

long cvtEngToRawBpt(
     double     *pval,
     short      linr,
     short      init,
     caddr_t    *ppbrk,
     short      *plbrk)
{ 
     double           val=*pval;
     long             status=0;
     struct brkTable *pbrkTable;
     struct brkInt   *pInt;     
     struct brkInt   *pnxtInt;
     short         lbrk;
     int         number;
     struct arrBrkTable *pcvtTable;


     if(linr < 2) return(-1);
     if(init==TRUE) { /*must find breakpoint table*/
         if( !(pcvtTable=pdbBase->pcvtTable) || (pcvtTable->number < linr)
         ||  (!(pcvtTable->papBrkTable[linr]))) {
                 errMessage(S_db_badField,"Breakpoint Table not Found");
                 return(S_db_badField);
         }
         *ppbrk = (caddr_t)(pcvtTable->papBrkTable[linr]);
         /* Just start at the beginning */
         *plbrk=0;
     }

     pbrkTable = (struct brkTable *)*ppbrk;
     number = pbrkTable->number;
     lbrk = *plbrk;
     /*make sure we dont go off end of table*/
     if( (lbrk+1) >= number ) lbrk--;
     pInt = pbrkTable->papBrkInt[lbrk];
     pnxtInt = pbrkTable->papBrkInt[lbrk+1];
     /* find entry for increased value */
     while( (pnxtInt->eng) <= val ) {
         lbrk++;
         pInt = pbrkTable->papBrkInt[lbrk];
         if( lbrk >= number-1) {
                status=1;
                break;
         }
         pnxtInt = pbrkTable->papBrkInt[lbrk+1];
     }
     while( (pInt->eng) > val) {
         if(lbrk==0) {
                status=1;
                break;
            }
         lbrk--;
         pInt = pbrkTable->papBrkInt[lbrk];
     }
     if(pInt->slope!=0){
         *plbrk = lbrk;
         *pval = pInt->raw + (val - pInt->eng) / pInt->slope;
     }
     else {
         return(status);
     }
     return(0);
}
