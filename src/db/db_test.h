/* base/include/db_test.h */

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#ifndef INCLdb_testh
#define INCLdb_testh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc int epicsShareAPI gft(char *pname);
epicsShareFunc int epicsShareAPI pft(char *pname,char *pvalue);
epicsShareFunc int epicsShareAPI tpn(char     *pname,char *pvalue);
#ifdef __cplusplus
}
#endif

#endif /* INCLdb_testh */
