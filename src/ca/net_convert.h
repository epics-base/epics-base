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
#  define ntohs(SHORT)	(SHORT)
#  define htons(SHORT)  (SHORT)
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
#  define ntohl(LONG)	(LONG)
#  define htonl(LONG)	(LONG)
#endif


/************************************************************************/
/*	So float convert can be performed in line for efficiency	*/
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


  /* passed by ref untill beter alternative found */
  /* (this includes mapping of fringe reals to zero or infinity) */
  /* (byte swaps included in conversion */

# define  htonf(MIT,IEEE)\
  {\
    long  exp,mant,sign;\
    sign = ((struct mitflt *) (MIT))->sign;\
    if( (short)((struct mitflt *) (MIT))->exp < EXPMINIEEE + MIT_SB){\
      exp 	= 0;\
      mant 	= 0;\
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

# define  ntohf(IEEE,MIT)\
  {\
  long  exp,mant2,mant1,sign;\
    *(long *)(IEEE) = htonl(*(long*)(IEEE));\
    sign = ((struct ieeeflt *) (IEEE))->sign;\
    if( (short)((struct ieeeflt *) (IEEE))->exp > EXPMAXMIT + IEEE_SB){\
      exp 	= EXPMAXMIT + MIT_SB;\
      mant2 	= ~0;\
      mant1 	= ~0;\
    }\
    else if( ((struct ieeeflt *) (IEEE))->exp == 0){\
      exp 	= 0;\
      mant2 	= 0;\
      mant1 	= 0;\
    }\
    else{\
      exp = ((struct ieeeflt *) (IEEE))->exp+MIT_SB-IEEE_SB;\
      mant2 = ((struct ieeeflt *) (IEEE))->mant;\
      mant1 = (((struct ieeeflt *) (IEEE))->mant>>(unsigned)16);\
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

#ifdef VAX
/*  cvrt is (array of) (pointer to) (function returning) int */
int	cvrt_sts_string	();
int	cvrt_sts_int	();
int	cvrt_sts_float	();
int	cvrt_sts_enum	();
int	cvrt_gr_int	();
int	cvrt_gr_float	();
int	cvrt_ctrl_int	();
int	cvrt_ctrl_float	();
int	cvrt_ctrl_enum	();

static
int 	(*cvrt[])()
	=
	{
	0,
	0,
	0,
	0,
	cvrt_sts_string,
	cvrt_sts_int,
	cvrt_sts_float,
	cvrt_sts_enum,
	cvrt_gr_int,
	cvrt_gr_float,
	cvrt_ctrl_int,
	cvrt_ctrl_float,
	cvrt_ctrl_enum
	};
#endif
