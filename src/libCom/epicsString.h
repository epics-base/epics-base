/*epicsString.h*/
/*Authors: Jun-ichi Odagiri and Marty Kraimer*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/* int dbTranslateEscape(char *s,const char *ct);
 *
 * copies ct to s while substituting escape sequences
 * returns the length of the resultant string (may contain nulls)
*/

#ifdef __cplusplus
extern "C" {
#endif
int dbTranslateEscape(char *s,const char *ct);

#ifdef __cplusplus
}
#endif

