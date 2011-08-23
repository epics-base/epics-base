
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef caServerDefsh
#define caServerDefsh

#ifndef NULL
#   define NULL 0
#endif

#ifndef NELEMENTS
#   define NELEMENTS(array) (sizeof(array)/sizeof((array)[0]))
#endif

#define invalidResID ( static_cast < ca_uint32_t > ( ~0UL ) )

void casVerifyFunc ( const char * pFile, 
    unsigned line, const char * pExp );
void serverToolDebugFunc ( const char * pFile, 
    unsigned line, const char * pComment );
#define serverToolDebug(COMMENT) \
{ serverToolDebugFunc(__FILE__, __LINE__, COMMENT); } 
#define casVerify(EXP) \
{ if ((EXP)==0) casVerifyFunc(__FILE__, __LINE__, #EXP); } 

#endif // caServerDefsh
