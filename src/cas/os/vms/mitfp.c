/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * mitfp.c - routines to convert between VAX float and big endian
 * 		IEEE float
 *
 * Author: Jeffrey O. Hill
 *
 *
 */

/************************************************************************/
/*	float convert							*/
/*      (THIS ASSUMES IEEE IS THE NETWORK FLOATING POINT FORMAT)        */
/************************************************************************/
struct ieeeflt{
  unsigned      mant    :23;
  unsigned      exp     :8;
  unsigned      sign    :1;
};

/*      Exponent sign bias      */
#define IEEE_SB         127

/*      Conversion Range        */
/*      -126<exp<127    with mantissa of form 1.mant                    */
#define EXPMINIEEE      -126            /* min for norm # IEEE exponent */

struct mitflt{
    unsigned    mant1   :7;
    unsigned    exp     :8;
    unsigned    sign    :1;
    unsigned    mant2   :16;
};

/*    Exponent sign bias      */
#define        MIT_SB          129

/*    Conversion Ranges       */
/*    -128<exp<126    with mantissa of form 1.mant                    */
#define        EXPMAXMIT       126     /* max MIT exponent             */
#define        EXPMINMIT       -128    /* min MIT exponent             */

/* (this includes mapping of fringe reals to zero or infinity) */
/* (byte swaps included in conversion */


/*
 * htonf()
 */
float htonf(float mit)
{
	float		ieee;
    	struct mitflt	*pMIT;
	struct ieeeflt	*pIEEE;
    	short		*ptmp;
    	long		exp;
	long		mant;
	long		sign;

	pMIT = (struct mitflt *) &mit;
	pIEEE = (struct ieeeflt *) &ieee;
    	sign = pMIT->sign;
    	if( ((short) pMIT->exp) < (EXPMINIEEE + MIT_SB) ){
      		exp       = 0;
      		mant      = 0;
      		sign      = 0;
    	}
    	else{
      		exp = ((short) pMIT->exp) - (MIT_SB + IEEE_SB);
      		mant = (pMIT->mant1<<16) | pMIT->mant2;
    	}
    	pIEEE->mant   = mant;
    	pIEEE->exp    = exp;
    	pIEEE->sign   = sign;

	ptmp = (short *)pIEEE;
	*ptmp = htons(*ptmp);

	return ieee;
}


/*
 * ntohf()
 *
 * sign must be forced to zero if the exponent is zero to prevent a reserved
 * operand fault- joh 9-13-90
 */
float ntohf(float ieee)
{
	float		mit;
	struct ieeeflt	*pIEEE;
	struct mitflt	*pMIT;
    	short		*ptmp;
  	long		exp;
	long		mant2;
	long		mant1;
	long		sign;

	pMIT = (struct mitflt *) &mit;
	pIEEE = (struct ieeeflt *) &ieee;

	ptmp = (short *)pIEEE;
    	*ptmp = htonl(*ptmp);

    	if( ((short) pIEEE->exp) > EXPMAXMIT + IEEE_SB){
      		sign      = pIEEE->sign;
      		exp       = EXPMAXMIT + MIT_SB;
      		mant2     = ~0;
      		mant1     = ~0;
    	}
    	else if( pIEEE->exp == 0){
      		sign      = 0;
      		exp       = 0;
      		mant2     = 0;
      		mant1     = 0;
    	}
    	else{
      		sign      = pIEEE->sign;
      		exp       = pIEEE->exp+MIT_SB-IEEE_SB;
      		mant2     = pIEEE->mant;
      		mant1     = pIEEE->mant>>(unsigned)16;
    	}
    	pMIT->exp      = exp;
    	pMIT->mant2    = mant2;
    	pMIT->mant1    = mant1;
    	pMIT->sign     = sign;

	return mit;
}


/************************************************************************/
/*	double convert							*/
/*      (THIS ASSUMES IEEE IS THE NETWORK FLOATING POINT FORMAT)        */
/************************************************************************/

/* (this includes mapping of fringe reals to zero or infinity) */
/* (byte swaps included in conversion */

struct ieeedbl{
        unsigned int    mant2 : 32;
        unsigned int    mant1 : 20;
        unsigned int    exp   : 11;
        unsigned int    sign  : 1;
};

#define IEEE_DBL_SB             1023

/*      Conversion Range        */
/*      -1022<exp<1024  with mantissa of form 1.mant                    */
#define DBLEXPMINIEEE   -1022   /* min for norm # IEEE exponent */

struct mitdbl{
        unsigned int    mant1 : 7;
        unsigned int    exp   : 8;
        unsigned int    sign  : 1;
        unsigned int    mant2 : 16;
        unsigned int    mant3 : 16;
        unsigned int    mant4 : 16;
};

/*    Exponent sign bias      */
#define        MIT_DBL_SB      129

/*    Conversion Ranges       */
/*    -128<exp<126    with mantissa of form 1.mant                    */
#define        DBLEXPMAXMIT    126     /* max MIT exponent             */
#define        DBLEXPMINMIT    -128    /* min MIT exponent             */



/*
 * htond()
 */
double htond(double mit)
{
	double		ieee;
	struct mitdbl 	*pMIT;
	struct ieeedbl 	*pIEEE;
    	long		*ptmp;
    	long        	tmp;

	pMIT = (struct mitdbl *)&mit;
	pIEEE = (struct ieeedbl *)&ieee;

    	if( ((int)pMIT->exp) < (DBLEXPMINMIT+MIT_DBL_SB) ){
      		pIEEE->mant1 = 0;
      		pIEEE->mant2 = 0;
      		pIEEE->exp  = 0;
      		pIEEE->sign = 0;
    	}
    	else{
      		pIEEE->exp  = ((int)pMIT->exp)+(IEEE_DBL_SB-MIT_DBL_SB);
     		pIEEE->mant1 = (pMIT->mant1<<13) | (pMIT->mant2>>3);
      		pIEEE->mant2 = (pMIT->mant2<<29) | (pMIT->mant3<<13) | 
        				(pMIT->mant4>>3); 
      		pIEEE->sign = pMIT->sign;
    	}

    	/*
    	 * byte swap to net order
     	 */
    	ptmp = (long *) pIEEE;
    	tmp = htonl(ptmp[0]);
    	ptmp[0] = htonl(ptmp[1]);
    	ptmp[1] = tmp;

	return ieee;
}


/*
 * sign must be forced to zero if the exponent is zero to prevent a reserved
 * operand fault- joh 9-13-90
 */
double ntohd(double ieee)
{
	double 		mit;
	struct mitdbl 	*pMIT;
	struct ieeedbl 	*pIEEE;
    	long		*ptmp;
    	long        	tmp;

	pMIT = (struct mitdbl *)&mit;
	pIEEE = (struct ieeedbl *)&ieee;

	/*
	 * Byte swap from net order to host order
	 */
    	ptmp = (long *) pIEEE;
    	tmp = htonl(ptmp[0]);
    	ptmp[0] = htonl(ptmp[1]);
    	ptmp[1] = tmp;

    	if( ((int)pIEEE->exp) > (DBLEXPMAXMIT + IEEE_DBL_SB) ){
      		pMIT->sign = pIEEE->sign; 
      		pMIT->exp = DBLEXPMAXMIT + MIT_DBL_SB; 
      		pMIT->mant1 = ~0; 
      		pMIT->mant2 = ~0; 
      		pMIT->mant3 = ~0; 
      		pMIT->mant4 = ~0; 
    	}
    	else if( ((int)pIEEE->exp) < (DBLEXPMINMIT + IEEE_DBL_SB) ){
      		pMIT->sign = 0; 
      		pMIT->exp = 0; 
      		pMIT->mant1 = 0; 
      		pMIT->mant2 = 0; 
      		pMIT->mant3 = 0; 
      		pMIT->mant4 = 0; 
    	}
    	else{
      		pMIT->sign = pIEEE->sign; 
      		pMIT->exp = ((int)pIEEE->exp)+(MIT_DBL_SB-IEEE_DBL_SB); 
      		pMIT->mant1 = pIEEE->mant1>>13; 
      		pMIT->mant2 = (pIEEE->mant1<<3) | (pIEEE->mant2>>29); 
      		pMIT->mant3 =  pIEEE->mant2>>13; 
      		pMIT->mant4 =  pIEEE->mant2<<3; 
    	}
	return mit;
} 

