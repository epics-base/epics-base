#ifndef GDD_ERROR_CODES_H
#define GDD_ERROR_CODES_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 *
 * *Revision 1.2  1996/06/13 21:32:00  jbk
 * *Various fixes and correction - including ref_cnt change to unsigned short
 * *Revision 1.1  1996/05/31 13:15:30  jbk
 * *add new stuff
 *
 */

typedef long gddStatus;

#define gddErrorTypeMismatch	-1
#define gddErrorNotAllowed		-2
#define gddErrorAlreadyDefined	-3
#define gddErrorNewFailed		-4
#define gddErrorOutOfBounds		-5
#define gddErrorAtLimit			-6
#define gddErrorNotDefined		-7
#define gddErrorNotSupported	-8
#define gddErrorOverflow		-9
#define gddErrorUnderflow		-10

#endif
