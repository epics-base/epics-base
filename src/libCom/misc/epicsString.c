/*epicsString.c*/
/*Authors: Jun-ichi Odagiri and Marty Kraimer*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
int dbTranslateEscape(char *to, const char *from)
{
    const char *pfrom  = from;
    char       *pto = to;
    char        c;
    int         nto=0;

    while( (c = *pfrom++ ) ){
	if(c=='\\') {
          switch( *pfrom ){
          case 'a':  pfrom++;  *pto++ = '\a' ; nto++; break;
          case 'b':  pfrom++;  *pto++ = '\b' ; nto++; break;
          case 'f':  pfrom++;  *pto++ = '\f' ; nto++; break;
          case 'n':  pfrom++;  *pto++ = '\n' ; nto++; break;
          case 'r':  pfrom++;  *pto++ = '\r' ; nto++; break;
          case 't':  pfrom++;  *pto++ = '\t' ; nto++; break;
          case 'v':  pfrom++;  *pto++ = '\v' ; nto++; break;
          case '\\': pfrom++;  *pto++ = '\\' ; nto++; break;
          case '\?': pfrom++;  *pto++ = '\?' ; nto++; break;
          case '\'': pfrom++;  *pto++ = '\'' ; nto++; break;
          case '\"': pfrom++;  *pto++ = '\"' ; nto++; break;
          case '0' :case '1' :case '2' :case '3' :
          case '4' :case '5' :case '6' :case '7' :
            {
		int  i;
		char strval[4] = {0,0,0,0};
		int  ival;
		unsigned char *pchar;

		for(i=0; i<3; i++) {
		    if((*pfrom < '0') || (*pfrom > '7')) break;
		    strval[i] = *pfrom++;
		}
		sscanf(strval,"%o",&ival);
		pchar = (unsigned char *)(pto++); nto++;
		*pchar = (unsigned char)(ival);
            }
            break;
          case 'x' :
            {
		int  i;
		char strval[3] = {0,0,0};
		int  ival;
		unsigned char *pchar;

                pfrom++; /*skip the x*/
		for(i=0; i<2; i++) {
		    if(!isxdigit(*pfrom)) break;
		    strval[i] = *pfrom++;
		}
		sscanf(strval,"%x",&ival);
		pchar = (unsigned char *)(pto++); nto++;
		*pchar = (unsigned char)(ival);
            }
            break;
          default:
            *pto++ = *pfrom++;
          }
        } else {
	    *pto++ = c; nto++;
        }
    }
    *pto = '\0'; /*NOTE that nto does not have to be incremented*/
    return(nto);
}
