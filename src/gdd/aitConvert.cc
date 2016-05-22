/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

// Author: Jim Kowalkowski
// Date: 2/96
// 

#define AIT_CONVERT_SOURCE 1

#include <stdio.h>
#include <epicsAlgorithm.h>
#include <epicsStdlib.h>
#include <limits.h>
#include "epicsStdio.h"
#include "cvtFast.h"

#define epicsExportSharedSymbols
#include "aitConvert.h"

int aitNoConvert(void* /*dest*/,const void* /*src*/,aitIndex /*count*/, const gddEnumStringTable *) {return -1;}

#ifdef AIT_CONVERT
#undef AIT_CONVERT
#endif
#ifdef AIT_TO_NET_CONVERT
#undef AIT_TO_NET_CONVERT
#endif
#ifdef AIT_FROM_NET_CONVERT
#undef AIT_FROM_NET_CONVERT
#endif

/* put the fixed conversion functions here (ones not generated) */

bool getStringAsDouble ( const char * pString, 
    const gddEnumStringTable * pEST, double & result ) 
{
    if ( ! pString ) {
        return false;
    }
	double ftmp;
    unsigned itmp;
    if ( pEST && pEST->getIndex ( pString, itmp ) ) {
        ftmp = itmp;
    }
    else {
	    int j = epicsScanDouble( pString, &ftmp );
	    if ( j != 1 ) {
		    j = sscanf ( pString,"%x", &itmp );
            if ( j == 1 ) {
                ftmp = itmp;
            }
            else {
                return false;
            }
        }
    }
    result = ftmp;
    return true;
}

bool putDoubleToString ( 
    const double in, const gddEnumStringTable * pEST, 
    char * pString, size_t strSize ) 
{
    if ( strSize <= 1u ) {
        return false;
    }
    if ( pEST && in >= 0 && in <= UINT_MAX ) {
        unsigned utmp = static_cast < unsigned > ( in );
        pEST->getString ( utmp, pString, strSize );
        if ( pString[0] != '\0' ) {
            return true;
        }
    }
#if 1
    bool cvtDoubleToStringInRange = 
        ( in < 1.e4 && in > 1.e-4 ) ||
        ( in > -1.e4 && in < -1.e-4 ) || 
        in == 0.0;
    // conservative
    static const unsigned cvtDoubleToStringSizeMax = 15;
    int nChar;
    if ( cvtDoubleToStringInRange && 
            strSize > cvtDoubleToStringSizeMax ) {
        nChar = cvtDoubleToString ( in, pString, 4 );
    }
    else {
	    nChar = epicsSnprintf ( 
            pString, strSize-1, "%g", in );
    }
    if ( nChar < 1 ) {
        return false;
    }
    assert ( size_t(nChar) < strSize );
#else
	int nChar = epicsSnprintf ( 
        pString, strSize-1, "%g", in );
	if ( nChar <= 0 ) {
        return false;
    }
#endif
    size_t nCharU = static_cast < size_t > ( nChar );
	nChar = epicsMin ( nCharU, strSize-1 ) + 1;
	memset ( &pString[nChar], '\0', strSize - nChar );
    return true;
}

/* ------- extra string conversion functions --------- */
static int aitConvertStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *)
{
	// does not work - need to be fixed
	aitIndex i;
	aitString *in=(aitString*)s, *out=(aitString*)d;

	for(i=0;i<c;i++) out[i]=in[i];
	return 0;
}
#ifdef AIT_NEED_BYTE_SWAP
static int aitConvertToNetStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringString(d,s,c, pEnumStringTable);}
static int aitConvertFromNetStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringString(d,s,c, pEnumStringTable);}
#endif

/* ------ all the fixed string conversion functions ------ */
static int aitConvertFixedStringFixedString(void* d,const void* s,
                       aitIndex c, const gddEnumStringTable *)
{
	aitUint32 len = c*AIT_FIXED_STRING_SIZE;
	memcpy(d,s,len);
	return 0;
}
#ifdef AIT_NEED_BYTE_SWAP
static int aitConvertToNetFixedStringFixedString(void* d,const void* s,
                  aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringFixedString(d,s,c,pEnumStringTable);}
static int aitConvertFromNetFixedStringFixedString(void* d,const void* s,
                  aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringFixedString(d,s,c,pEnumStringTable);}
#endif

static int aitConvertStringFixedString(void* d,const void* s,
                    aitIndex c, const gddEnumStringTable *)
{
	aitIndex i;
	aitString* out = (aitString*)d;
	aitFixedString* in = (aitFixedString*)s;

	for(i=0;i<c;i++) out[i].copy(in[i].fixed_string);
	return 0;
}

static int aitConvertFixedStringString(void* d,const void* s,
                   aitIndex c, const gddEnumStringTable *)
{
	aitIndex i;
	aitString* in = (aitString*)s;
	aitFixedString* out = (aitFixedString*)d;

	//
	// joh - changed this from strcpy() to stncpy() in order to:
	// 1) shut up purify 
	// 2) guarantee that all fixed strings will be terminated
	// 3) guarantee that we will not overflow a fixed string
	//
	for(i=0;i<c;i++){
		strncpy(out[i].fixed_string,in[i].string(),AIT_FIXED_STRING_SIZE);
		out[i].fixed_string[AIT_FIXED_STRING_SIZE-1u] = '\0';
	}
	return 0;
}

#ifdef AIT_NEED_BYTE_SWAP
static int aitConvertToNetStringFixedString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringFixedString(d,s,c,pEnumStringTable); }
static int aitConvertFromNetFixedStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringString(d,s,c,pEnumStringTable); }
static int aitConvertToNetFixedStringString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertStringFixedString(d,s,c,pEnumStringTable); }
static int aitConvertFromNetStringFixedString(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ return aitConvertFixedStringString(d,s,c,pEnumStringTable); }
#endif

static int aitConvertStringEnum16(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status=0;
	aitString* out=(aitString*)d;
	aitEnum16* in=(aitEnum16*)s;
	for (i=0;i<c;i++) {
		if ( pEnumStringTable && in[i] < pEnumStringTable->numberOfStrings() ) {
			unsigned nChar = pEnumStringTable->getStringLength ( in[i] );
    		if ( nChar < static_cast <unsigned> ( INT_MAX - status ) ) {
    		    out[i].copy( pEnumStringTable->getString ( in[i] ), nChar );
    		    status += static_cast <int> ( nChar );;
    		}
    		else {
    		    return -1;
    		}
		}
		else {
		   	char temp[AIT_FIXED_STRING_SIZE];
			int tmpStatus = sprintf ( temp, "%hu", in[i] );
			if ( tmpStatus >= 0 && tmpStatus < INT_MAX - status ) {
			    out[i].copy ( temp, static_cast < unsigned > ( tmpStatus ) );
			    status += tmpStatus;
			}
			else {
			    return -1;
			}
		}
	}
	return status;
}

#ifdef AIT_NEED_BYTE_SWAP
static int aitConvertToNetStringEnum16(void* d,const void* s,
            aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertStringEnum16(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetStringEnum16(void* d,const void* s,
            aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertStringEnum16(d,s,c,pEnumStringTable); 
}
#endif

static int aitConvertFixedStringEnum16(void* d,const void* s,
         aitIndex c, const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status=0;
	aitFixedString* out=(aitFixedString*)d;
	aitEnum16* in=(aitEnum16*)s;
	for (i=0;i<c;i++) {
		if ( pEnumStringTable && in[i] < pEnumStringTable->numberOfStrings() ) {
			unsigned nChar = pEnumStringTable->getStringLength ( in[i] );
			if ( nChar < static_cast < unsigned > ( INT_MAX - status ) ) {
                pEnumStringTable->getString ( 
                    in[i], 
                    out[i].fixed_string, 
                    sizeof( out[i].fixed_string ) );
                status += static_cast < int > ( nChar );
			}
			else {
			    return -1;
			}
		}
		else {
			int tmpStatus = sprintf ( out[i].fixed_string, "%hu", in[i] );
			if ( tmpStatus > 0 && tmpStatus < INT_MAX - status ) {
			    status += tmpStatus;
			}
			else {
			    return -1;
			}
		}
	}
	return status;
}

#ifdef AIT_NEED_BYTE_SWAP
static int aitConvertToNetFixedStringEnum16(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertFixedStringEnum16(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetFixedStringEnum16(void* d,const void* s,
              aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertFixedStringEnum16(d,s,c,pEnumStringTable); 
}
#endif

static int aitConvertEnum16FixedString (void* d,const void* s,aitIndex c, 
                        const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status = 0;
	aitEnum16* out = (aitEnum16*)d;
	aitFixedString* in = (aitFixedString*)s;
    aitEnum16 choice, nChoices;

    //
    // convert only after a range check
    //
    if ( pEnumStringTable ) {
        assert (pEnumStringTable->numberOfStrings()<=0xffff);
        nChoices = static_cast<aitEnum16>(pEnumStringTable->numberOfStrings());
    }
    else {
        nChoices = 0;
    }

	for (i=0;i<c;i++) {
        //
        // look for a string that matches
        //
	    for (choice=0;choice<nChoices;choice++) {
            if (strcmp( pEnumStringTable->getString(choice), in[i].fixed_string)==0) {
                out[i] = choice;
                status += sizeof(out[i]);
                break;
            }
        }
        //
        // if no string matches then look for a numeric match
        //
        if (choice>=nChoices) {
            int temp;
			if ( sscanf ( in[i].fixed_string,"%i", &temp ) == 1 ) {
				if ( temp >= 0 && temp < nChoices ) {
					out[i] = (aitUint16) temp;
                    status += sizeof(out[i]);
				}
				else {
                    //
                    // no match, return an error
                    //
					return -1;
				}
			}
            else {
                return -1;
            }
        }
    }
    return status;
}

#ifdef AIT_NEED_BYTE_SWAP
static int aitConvertToNetEnum16FixedString(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16FixedString(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetEnum16FixedString(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16FixedString(d,s,c,pEnumStringTable); 
}
#endif

static int aitConvertEnum16String (void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{
	aitIndex i;
	int status = 0;
	aitEnum16* out = (aitEnum16*)d;
	aitString* in = (aitString*)s;
    aitEnum16 choice, nChoices;

    //
    // convert only after a range check
    //
    if ( pEnumStringTable ) {
        assert (pEnumStringTable->numberOfStrings()<=0xffff);
        nChoices = static_cast<aitEnum16>(pEnumStringTable->numberOfStrings());
    }
    else {
        nChoices = 0u;
    }

	for (i=0;i<c;i++) {
        //
        // find the choice that matches
        //
	    for (choice=0;choice<nChoices;choice++) {
            if (strcmp(pEnumStringTable->getString(choice), in[i].string())==0) {
                out[i] = choice;
                status += sizeof(out[i]);
                break;
            }
        }
        //
        // if no string matches then look for a numeric match
        //
        if (choice>=nChoices) {
            int temp;
			if ( sscanf ( in[i].string(),"%i", &temp ) == 1 ) {
				if ( temp >= 0 && temp < nChoices ) {
					out[i] = (aitUint16) temp;
                    status += sizeof(out[i]);
				}
				else {
                    //
                    // no match, return an error
                    //
					return -1;
				}
			}
            else {
                return -1;
            }
        }
    }
    return status;
}

#ifdef AIT_NEED_BYTE_SWAP
static int aitConvertToNetEnum16String(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16String(d,s,c,pEnumStringTable); 
}

static int aitConvertFromNetEnum16String(void* d,const void* s,
               aitIndex c, const gddEnumStringTable *pEnumStringTable)
{ 
    return aitConvertEnum16String(d,s,c,pEnumStringTable); 
}
#endif

#define AIT_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_CONVERT

/* include the network byte order functions if needed */
#ifdef AIT_NEED_BYTE_SWAP

#define AIT_TO_NET_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_TO_NET_CONVERT

#define AIT_FROM_NET_CONVERT 1
#include "aitConvertGenerated.cc"
#undef AIT_FROM_NET_CONVERT

#endif

