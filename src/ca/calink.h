/*
 *	$Id$
 */
#ifndef INCcalinkh
#define INCcalinkh

#ifndef INCerrMdefh
#include <errMdef.h>
#endif
#define S_dbCa_nullarg        (M_dbCa | 1) /*dbCa_nullarg*/
#define S_dbCa_failedmalloc   (M_dbCa | 3) /*dbCa_failedmalloc*/
#define S_dbCa_foundnull      (M_dbCa | 5) /*dbCa_foundnull*/
#define S_dbCa_ECA_EVDISALLOW (M_dbCa | 7) /*dbCa_ECA_EVDISALLOW*/
#define S_dbCa_ECA_BADCHID    (M_dbCa | 9) /*dbCa_ECA_BADCHID*/
#define S_dbCa_ECA_BADTYPE    (M_dbCa | 11) /*dbCa_ECA_BADTYPE*/
#define S_dbCa_ECA_ALLOCMEM   (M_dbCa | 13) /*dbCa_ECA_ALLOCMEM*/
#define S_dbCa_ECA_ADDFAIL    (M_dbCa | 15) /*dbCa_ECA_ADDFAIL*/
#define S_dbCa_ECA_STRTOBIG   (M_dbCa | 17) /*dbCa_ECA_STRTOBIG*/
#define S_dbCa_ECA_GETFAIL    (M_dbCa | 19) /*dbCa_ECA_GETFAIL*/
#define S_dbCa_ECA_BADCOUNT   (M_dbCa | 21) /*dbCa_ECA_BADCOUNT*/
#define S_dbCa_ECA_PUTFAIL    (M_dbCa | 23) /*dbCa_ECA_PUTFAIL*/
#define S_dbCa_unknownECA     (M_dbCa | 25) /*dbCa_unknownECA*/
#define S_dbCa_dbfailure      (M_dbCa | 27) /*dbCa_dbfailure*/
#define S_dbCa_cafailure      (M_dbCa | 29) /*dbCa_cafailure*/
#endif /* INCcalinkh */
