/*
 *
 *	N E T _ C O N V E R T . H
 *	MACROS for rapid conversion between VAX data formats and those used
 *	by the IOCs
 *
 *
 *	joh 	09-13-90	force MIT sign to zero if exponent is zero
 *				to prevent a reseved operand fault.
 *
 *	joh	03-16-94	Added double fp
 *
 *
 */

/************************************************************************/
/*	So byte swapping can be performed in line for efficiency	*/
/*	(WINTCP has library routines with the same same functionality	*/
/*	but more than twice the delay) 	JH				*/
/*	WARNING: WILL NOT WORK ON TYPES FLOAT OR DOUBLE			*/
/*	(use *(long *)& to perform type convert without reformat)	*/
/************************************************************************/

#ifdef VAX
#  define ntohs(SHORT)\
   ((unsigned short)(((short)(SHORT))<<8 | ((unsigned short)(SHORT))>>8 ))
#  define htons(SHORT)\
   ((unsigned short)(((short)(SHORT))<<8 | ((unsigned short)(SHORT))>>8 ))
#else
#  ifndef ntohs
#    define ntohs(SHORT)	(SHORT)
#  endif
#  ifndef htons 
#    define htons(SHORT)	(SHORT)
#  endif
#endif


#ifdef VAX
#  define ntohl(LONG)\
  (\
          	 ((long)(LONG))<<24 |\
   	((unsigned long)(LONG))>>24 |\
         	(((long)(LONG))&0x00ff0000)>>8 |\
         	(((long)(LONG))&0x0000ff00)<<8\
  )
#  define htonl(LONG)\
  (\
          	 ((long)(LONG))<<24 |\
   	((unsigned long)(LONG))>>24 |\
         	(((long)(LONG))&0x00ff0000)>>8 |\
         	(((long)(LONG))&0x0000ff00)<<8\
  )
#else 
#  ifndef ntohl
#    define ntohl(LONG)	(LONG)
#  endif
#  ifndef htonl
#    define htonl(LONG)	(LONG)
#  endif
#endif


/************************************************************************/
/*	So float convert can be performed in line 			*/
/*	(THIS ASSUMES IEEE IS THE NETWORK FLOATING POINT FORMAT)	*/
/************************************************************************/
struct ieeeflt{
  unsigned	mant	:23;
  unsigned	exp	:8;
  unsigned	sign	:1;
};

/*	Exponent sign bias	*/
#define	IEEE_SB		127

/*	Conversion Range	*/
/*	-126<exp<127	with mantissa of form 1.mant			*/
#define	EXPMINIEEE	-126		/* min for norm # IEEE exponent	*/

#ifdef VAX

  struct mitflt{
    unsigned	mant1	:7;
    unsigned	exp	:8;
    unsigned	sign	:1;
    unsigned	mant2	:16;
  };

  /*	Exponent sign bias	*/
# define	MIT_SB		129

  /*	Conversion Ranges	*/
  /*	-128<exp<126	with mantissa of form 1.mant			*/
# define	EXPMAXMIT	126	/* max MIT exponent 		*/
# define 	EXPMINMIT	-128	/* min MIT exponent		*/

  /* passed by ref until beter alternative found */
  /* (this includes mapping of fringe reals to zero or infinity) */
  /* (byte swaps included in conversion */

# define  htonf(MIT,IEEE)\
  {\
    long  exp,mant,sign;\
    sign = ((struct mitflt *) (MIT))->sign;\
    if( (short)((struct mitflt *) (MIT))->exp < EXPMINIEEE + MIT_SB){\
      exp 	= 0;\
      mant 	= 0;\
      sign      = 0;\
    }\
    else{\
      exp = (short)((struct mitflt *) (MIT))->exp-MIT_SB+IEEE_SB;\
      mant = (((struct mitflt *) (MIT))->mant1<<16) |\
	   ((struct mitflt *) (MIT))->mant2;\
    }\
    ((struct ieeeflt *) (IEEE))->mant 	= mant;\
    ((struct ieeeflt *) (IEEE))->exp 	= exp;\
    ((struct ieeeflt *) (IEEE))->sign 	= sign;\
    *(long *)(IEEE) 			= ntohl(*(long*)(IEEE));\
  }

/*
 * sign must be forced to zero if the exponent is zero to prevent a reserved
 * operand fault- joh 9-13-90
 */
# define  ntohf(IEEE,MIT)\
  {\
  long  exp,mant2,mant1,sign;\
    *(long *)(IEEE) = htonl(*(long*)(IEEE));\
    if( (short)((struct ieeeflt *) (IEEE))->exp > EXPMAXMIT + IEEE_SB){\
      sign 	= ((struct ieeeflt *) (IEEE))->sign;\
      exp 	= EXPMAXMIT + MIT_SB;\
      mant2 	= ~0;\
      mant1 	= ~0;\
    }\
    else if( ((struct ieeeflt *) (IEEE))->exp == 0){\
      sign	= 0;\
      exp 	= 0;\
      mant2 	= 0;\
      mant1 	= 0;\
    }\
    else{\
      sign 	= ((struct ieeeflt *) (IEEE))->sign;\
      exp 	= ((struct ieeeflt *) (IEEE))->exp+MIT_SB-IEEE_SB;\
      mant2 	= ((struct ieeeflt *) (IEEE))->mant;\
      mant1 	= ((struct ieeeflt *) (IEEE))->mant>>(unsigned)16;\
    }\
    ((struct mitflt *) (MIT))->exp 	= exp;\
    ((struct mitflt *) (MIT))->mant2 	= mant2;\
    ((struct mitflt *) (MIT))->mant1 	= mant1;\
    ((struct mitflt *) (MIT))->sign 	= sign;\
  }


#else
# define ntohf(NET,HOST)  {*(float *)(HOST) = *(float *)(NET);}
# define htonf(HOST,NET)  {*(float *)(NET) = *(float *)(HOST);}
#endif

/************************************************************************/
/*	So double convert can be performed in line 			*/
/*	(THIS ASSUMES IEEE IS THE NETWORK FLOATING POINT FORMAT)	*/
/************************************************************************/
struct ieeedbl{
        unsigned int    mant1 : 20;
        unsigned int    exp   : 11;
        unsigned int    sign  : 1;
        unsigned int    mant2 : 32;
};

#define	IEEE_DBL_SB		1023	

/*	Conversion Range	*/
/*	-1022<exp<1024	with mantissa of form 1.mant			*/
#define	DBLEXPMINIEEE	-1022	/* min for norm # IEEE exponent	*/

#ifdef VAX

  struct mitdbl{
        unsigned int    mant1 : 7;
        unsigned int    exp   : 8;
        unsigned int    sign  : 1;
        unsigned int    mant2 : 16;
        unsigned int    mant3 : 16;
        unsigned int    mant4 : 16;
  };

  /*	Exponent sign bias	*/
# define	MIT_DBL_SB	129	

  /*	Conversion Ranges	*/
  /*	-128<exp<126	with mantissa of form 1.mant			*/
# define	DBLEXPMAXMIT	126	/* max MIT exponent 		*/
# define	DBLEXPMINMIT	-128	/* min MIT exponent 		*/


  /* passed by ref until beter alternative found */
  /* (this includes mapping of fringe reals to zero or infinity) */
  /* (byte swaps included in conversion */

# define  htond(MIT,IEEE)\
  {\
    long	tmp;\
    if( (short)((struct mitdbl *) (MIT))->exp < DBLEXPMINMIT + MIT_DBL_SB){\
      ((struct ieeedbl *) (IEEE))->mant1 = 0;\
      ((struct ieeedbl *) (IEEE))->mant2 = 0;\
      ((struct ieeedbl *) (IEEE))->exp 	= 0;\
      ((struct ieeedbl *) (IEEE))->sign	= 0;\
    }\
    else{\
      ((struct ieeedbl *) (IEEE))->exp 	= \
        (short)((struct mitdbl *) (MIT))->exp-MIT_DBL_SB+IEEE_DBL_SB;\
      ((struct ieeedbl *) (IEEE))->mant1 = 
	(((struct mitdbl *) (MIT))->mant1<<13) | \
	(((struct mitdbl *) (MIT))->mant2>>3)
      ((struct ieeedbl *) (IEEE))->mant2 = \
	(((struct mitdbl *) (MIT))->mant2<<29) | \
	(((struct mitdbl *) (MIT))->mant3<<13 | \
	(((struct mitdbl *) (MIT))->mant4>>3;
        ((struct ieeedbl *) (IEEE))->sign = \
		((struct mitdbl *) (MIT))->sign;\
    }\
    tmp = htonl(*(long*)(IEEE));\
    *(long *)(IEEE) = htonl(*((long*)(IEEE)+1));\
    *((long *)(IEEE)+1) = tmp;\
  }

/*
 * sign must be forced to zero if the exponent is zero to prevent a reserved
 * operand fault- joh 9-13-90
 */
# define  ntohd(IEEE,MIT)\
  {\
    long	tmp;\
    tmp = htonl(*(long*)(IEEE));\
    *(long *)(IEEE) = htonl(*((long*)(IEEE)+1));\
    *((long *)(IEEE)+1) = tmp;\
    if( (short)((struct ieeedbl *) (IEEE))->exp > DBLEXPMAXMIT + IEEE_DBL_SB){\
      ((struct mitdbl *) (MIT))->sign = ((struct ieeedbl *) (IEEE))->sign; \
      ((struct mitdbl *) (MIT))->exp = DBLEXPMAXMIT + MIT_DBL_SB; \
      ((struct mitdbl *) (MIT))->mant1 = ~0; \
      ((struct mitdbl *) (MIT))->mant2 = ~0; \
      ((struct mitdbl *) (MIT))->mant3 = ~0; \
      ((struct mitdbl *) (MIT))->mant4 = ~0; \
    }\
    else if( ((struct ieeedbl *) (IEEE))->exp < DBLEXPMINMIT + IEEE_DBL_SB){\
      ((struct mitdbl *) (MIT))->sign = 0; \
      ((struct mitdbl *) (MIT))->exp = 0; \
      ((struct mitdbl *) (MIT))->mant1 = 0; \
      ((struct mitdbl *) (MIT))->mant2 = 0; \
      ((struct mitdbl *) (MIT))->mant3 = 0; \
      ((struct mitdbl *) (MIT))->mant4 = 0; \
    }\
    else{\
      ((struct mitdbl *) (MIT))->sign = 
		((struct ieeedbl *) (IEEE))->sign; \
      ((struct mitdbl *) (MIT))->exp = 
		((struct ieeedbl *) (IEEE))->exp+MIT_DBL_SB-IEEE_DBL_SB; \
      ((struct mitdbl *) (MIT))->mant1 = 
		((struct ieeedbl *) (IEEE))->mant1>>13; \
      ((struct mitdbl *) (MIT))->mant2 = 
		(((struct ieeedbl *) (IEEE))->mant1<<3) |
		(((struct ieeedbl *) (IEEE))->mant2>>29); \
      ((struct mitdbl *) (MIT))->mant3 = 
		((struct ieeedbl *) (IEEE))->mant2>>13; \
      ((struct mitdbl *) (MIT))->mant4 = 
		((struct ieeedbl *) (IEEE))->mant2<<3; \
    }\
  }


#else
# define ntohd(NET,HOST)  {*(double *)(HOST) = *(double *)(NET);}
# define htond(HOST,NET)  {*(double *)(NET) = *(double *)(HOST);}
#endif

