/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
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
 *
 * History
 * $Log$
 *
 */


#ifndef includeCasCommonDefH
#define includeCasCommonDefH

#ifndef NULL
#define NULL            0
#endif

#ifndef FALSE
#define FALSE           0
#endif

#ifndef TRUE
#define TRUE            1
#endif

#ifndef OK
#define OK              0
#endif

#ifndef ERROR
#define ERROR           (-1)
#endif

#ifndef NELEMENTS
#define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif

#ifndef LOCAL
#define LOCAL static
#endif

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef max
#define max(A,B) ((A)<(B)?(B):(A))
#endif

#endif /* ifndef includeCasCommonDefH (last line in this file) */

