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
 *
 */

static char *net_converthSccsId = "@(#)net_convert.h	1.5\t7/27/92";

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

#ifdef VAX
int	cvrt_string();
int	cvrt_short();
int	cvrt_float();
int	cvrt_enum();
int	cvrt_char();
int	cvrt_long();
int	cvrt_double();

int	cvrt_sts_string();
int	cvrt_sts_short();
int	cvrt_sts_float();
int	cvrt_sts_enum();
int	cvrt_sts_char();
int	cvrt_sts_long();
int	cvrt_sts_double();

int	cvrt_time_string();
int	cvrt_time_short();
int	cvrt_time_float();
int	cvrt_time_enum();
int	cvrt_time_char();
int	cvrt_time_long();
int	cvrt_time_double();

int	cvrt_gr_string();
int	cvrt_gr_short();
int	cvrt_gr_float();
int	cvrt_gr_enum();
int	cvrt_gr_char();
int	cvrt_gr_long();
int	cvrt_gr_double();
int	cvrt_gr_string();

int	cvrt_ctrl_string();
int	cvrt_ctrl_short();
int	cvrt_ctrl_float();
int	cvrt_ctrl_enum();
int	cvrt_ctrl_char();
int	cvrt_ctrl_long();
int	cvrt_ctrl_double();

/*  cvrt is (array of) (pointer to) (function returning) int */
static
int 	(*cvrt[])()
	=
	{
	cvrt_string,
	cvrt_short,
	cvrt_float,
	cvrt_enum,
	cvrt_char,
	cvrt_long,
	cvrt_double,

	cvrt_sts_string,
	cvrt_sts_short,
	cvrt_sts_float,
	cvrt_sts_enum,
	cvrt_sts_char,
	cvrt_sts_long,
	cvrt_sts_double,

	cvrt_time_string,
	cvrt_time_short,
	cvrt_time_float,
	cvrt_time_enum,
	cvrt_time_char,
	cvrt_time_long,
	cvrt_time_double,

	cvrt_gr_string,
	cvrt_gr_short,
	cvrt_gr_float,
	cvrt_gr_enum,
	cvrt_gr_char,
	cvrt_gr_long,
	cvrt_gr_double,

	cvrt_ctrl_string,
	cvrt_ctrl_short,
	cvrt_ctrl_float,
	cvrt_ctrl_enum,
	cvrt_ctrl_char,
	cvrt_ctrl_long,
	cvrt_ctrl_double
	};
#endif
