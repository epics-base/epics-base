/*
 *	C O N V E R T . C
 *
 *	Author: D. Kersteins
 *
 *
 * 	NOTES: 
 *	1) converts arrays without subroutine call
 *	for improved efficiency -joh
 *
 *	2) These routines only do VAX conversions for now - joh
 *
 *	3) All routines in this file have an encode argument which
 *	determines if we are converting from the standard format to
 *	the local format or vise versa. To date only float and double data 
 *	types must be converted differently depending on the encode
 *	argument - joh
 *
 *	History
 *	joh	10-21-90	Moved atomic types here to avoid
 *				maintenance problems in service.c
 *				This also provides for a cleaner seperation
 *				of function.
 *	joh	10-21-90	Added new DBR_XXXX types for release two
 *	joh	03-15-94	Added many missing types	
 *
 *
 */

static char	*sccsId = "$Id$";

#include	<string.h>

#include	<db_access.h>
#include	<net_convert.h>

#ifdef __STDC__

/*
 * if hton is true then it is a host to network conversion
 * otherwise vise-versa
 *
 * net format: big endian and IEEE float
 * 
 */
typedef LOCAL void CACVRTFUNC(
		void *pSource, 
		void *pDestination, 
		int hton, 
		unsigned long count);
#else /*__STDC__*/
typedef LOCAL void CACVRTFUNC();
#endif /*__STDC__*/

CACVRTFUNC	cvrt_string;
CACVRTFUNC	cvrt_short;
CACVRTFUNC	cvrt_float;
CACVRTFUNC	cvrt_enum;
CACVRTFUNC	cvrt_char;
CACVRTFUNC	cvrt_long;
CACVRTFUNC	cvrt_double;

CACVRTFUNC	cvrt_sts_string;
CACVRTFUNC	cvrt_sts_short;
CACVRTFUNC	cvrt_sts_float;
CACVRTFUNC	cvrt_sts_enum;
CACVRTFUNC	cvrt_sts_char;	
CACVRTFUNC	cvrt_sts_long;
CACVRTFUNC	cvrt_sts_double;

CACVRTFUNC	cvrt_time_string;
CACVRTFUNC	cvrt_time_short;
CACVRTFUNC	cvrt_time_float;
CACVRTFUNC	cvrt_time_enum;
CACVRTFUNC	cvrt_time_char;
CACVRTFUNC	cvrt_time_long;
CACVRTFUNC	cvrt_time_double;

CACVRTFUNC	cvrt_gr_short;
CACVRTFUNC	cvrt_gr_float;
CACVRTFUNC	cvrt_gr_enum;
CACVRTFUNC	cvrt_gr_char;
CACVRTFUNC	cvrt_gr_long;
CACVRTFUNC	cvrt_gr_double;

CACVRTFUNC	cvrt_ctrl_short;
CACVRTFUNC	cvrt_ctrl_float;
CACVRTFUNC	cvrt_ctrl_enum;
CACVRTFUNC	cvrt_ctrl_char;
CACVRTFUNC	cvrt_ctrl_long;
CACVRTFUNC	cvrt_ctrl_double;

/*  cvrt is (array of) (pointer to) (function returning) int */
LOCAL CACVRTFUNC *cvrt[]
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

	cvrt_sts_string, /* DBR_GR_STRING identical to dbr_sts_string */
	cvrt_gr_short,
	cvrt_gr_float,
	cvrt_gr_enum,
	cvrt_gr_char,
	cvrt_gr_long,
	cvrt_gr_double,

	cvrt_sts_string, /* DBR_CTRL_STRING identical to dbr_sts_string */
	cvrt_ctrl_short,
	cvrt_ctrl_float,
	cvrt_ctrl_enum,
	cvrt_ctrl_char,
	cvrt_ctrl_long,
	cvrt_ctrl_double
	};

static void no_cvrt();

/*
 * Native types may not match EPICS types
 */
typedef dbr_short_t	short;
typedef dbr_float_t	float;
typedef dbr_enum_t	enum;
typedef dbr_char_t	char;
typedef dbr_long_t	long;
typedef dbr_double_t	double;
#define dbr_ntohs(A)	ntohs(A)
#define dbr_ntohl(A)	ntohl(A)
#define dbr_htons(A)	htons(A)
#define dbr_htonl(A)	htonl(A)



/*
 *	CVRT_STRING()
 *
 *
 *
 *
 */
#ifdef __STDC__
void cvrt_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_short(s,d,encode,num)
void		*s;			/* source			*/
void		*d;			/* destination			*/
int 		encode;			/* cvrt VAX to IEEE if T	*/
unsigned long	num;			/* number of values		*/
#endif
{
	dbr_char_t	*pSrc = s;
	dbr_char_t	*pDest = d;

	if(num == 1){
		strcpy(pDest, pSrc);
	}
	else{
		memcpy(pDest, pSrc, num*MAXSTRINGSIZE);
	}
}


/*
 *	CVRT_SHORT()
 *
 *
 *
 *
 */
#ifdef __STDC__
void cvrt_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_short(s,d,encode,num)
void		*s;			/* source			*/
void		*d;			/* destination			*/
int 		encode;			/* cvrt VAX to IEEE if T	*/
unsigned long	num;			/* number of values		*/
#endif
{
	dbr_short_t	*pSrc = s;
	dbr_short_t	*pDest = d;
      	unsigned long	i;

      	for(i=0; i<num; i++){
      	  	*pDest++ = dbr_ntohs( *pSrc++ );
	}
}


/*
 *	CVRT_CHAR()
 *
 *
 *
 *
 */
#ifdef __STDC__
void cvrt_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_char(
void		*s;			/* source			*/
void		*d;			/* destination			*/
int 		encode;			/* cvrt VAX to IEEE if T	*/
unsigned long	num;			/* number of values		*/
#endif
{
      	unsigned int	i;
	dbr_char_t	*pSrc = s;
	dbr_char_t	*pDest = d;

      	for(i=0; i<num; i++){
      	  	*pDest++ = *pSrc++;
	}
}


/*
 *	CVRT_LONG()
 *
 *
 *
 *
 */
#ifdef __STDC__
void cvrt_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_long(s,d,encode,num)
void		*s;			/* source			*/
void		*d;			/* destination			*/
int 		encode;			/* cvrt VAX to IEEE if T	*/
unsigned long	num;			/* number of values		*/
#endif
{
      	unsigned long	i;
	dbr_long_t	*pSrc = s;
	dbr_long_t	*pDest = d;

      	for(i=0; i<num; i++){
      	  	*pDest++ = dbr_ntohl( *pSrc++ );
	}
}


/*
 *	CVRT_ENUM()
 *
 *
 *
 *
 */
#ifdef __STDC__
void cvrt_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_enum(s,d,encode,num)
short		*s;			/* source			*/
short		*d;			/* destination			*/
int 		encode;			/* cvrt VAX to IEEE if T	*/
unsigned long	num;			/* number of values		*/
#endif
{
      	unsigned int	i;
	dbr_enum_t	*pSrc = s;
	dbr_enum_t	*pDest = d;

      	for(i=0; i<num; i++){
      	  	*pDest++ = dbr_ntohs(*pSrc++);
	}
}


/*
 *	CVRT_FLOAT()
 *
 *
 *	NOTES:
 *	placing encode outside the loop results in more 
 * 	code but better performance.
 *
 */
#ifdef __STDC__
void cvrt_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
#else
void cvrt_float(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
      	unsigned long	i;
	dbr_float_t	*pSrc = s;
	dbr_float_t	*pDest = d;

      	for(i=0; i<num; i++){
		if(encode){
	  		dbr_htonf(pSrc, pDest);
		}
		else{
	  		dbr_ntohf(pSrc, pDest);
		}
		/*
		 * incrementing these inside the MACRO could be unhealthy
		 */
		pSrc++;
		pDest++;
	}
}


/*
 *	CVRT_DOUBLE()
 *
 *
 *
 *
 */
#ifdef __STDC__
void cvrt_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_double(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
      	unsigned int	i;

	if(encode){
      		for(i=0; i<num; i++){
			htond(s,d);
		}
	}
	else{
      		for(i=0; i<num; i++){
			ntohd(s,d;
		}
	}
}





/****************************************************************************
**	cvrt_sts_string(s,d)
**		struct dbr_sts_string *s  	pointer to source struct
**		struct dbr_sts_string *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**	      
**	converts fields of struct in VAX format to IEEE format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
void cvrt_sts_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_sts_string(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_sts_string	*pSrc = s;
	struct dbr_sts_string 	*pDest = d;
			
    	/* convert ieee to vax format or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
    	if (num == 1)	/* if single value */
		strcpy(pDest->value, pSrc->value);
    	else
		memcpy(pDest->value, pSrc->value, (MAX_STRING_SIZE * num));

}


/****************************************************************************
**	cvrt_sts_short(s,d)
**		struct dbr_sts_int *s  	pointer to source struct
**		struct dbr_sts_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

#ifdef __STDC__
void cvrt_sts_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_sts_short(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_sts_int	*pSrc = s;
	struct dbr_sts_int	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_short(&pSrc->value, &pDest->value, encode, num);
    	}
}

/****************************************************************************
**	cvrt_sts_float(s,d)
**		struct dbr_sts_float *s  	pointer to source struct
**		struct dbr_sts_float *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
void cvrt_sts_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
void cvrt_sts_float(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_sts_float	*pSrc = s;
	struct dbr_sts_float	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

	cvrt_float(&pSrc->value, &pDest->value, encode, num);
}


/****************************************************************************
**	cvrt_sts_double(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_sts_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_sts_double(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_sts_double	*pSrc = s;
	struct dbr_sts_double	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

	cvrt_double(&pSrc->value, &pDest->value, encode, num);
}


/****************************************************************************
**	cvrt_sts_enum(s,d)
**		struct dbr_sts_enum *s  	pointer to source struct
**		struct dbr_sts_enum *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_sts_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_sts_enum(s,d,encode,num)
void		*s;			/* source			*/
void		*d;			/* destination			*/
int 		encode;			/* cvrt VAX to IEEE if T	*/
unsigned long	num;			/* number of values		*/
#endif
{
	struct dbr_sts_enum	*pSrc = s;
	struct dbr_sts_enum	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	memcpy(&pDest->strs, &pSrc->strs, sizeof(pDest->strs));
    	if (num == 1)
        	d->value	= dbr_ntohs(s->value);
    	else {
		cvrt_enum(&pSrc->value,&pDest->value,encode,num)
    	}
}


/****************************************************************************
**	cvrt_gr_short()
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_gr_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_gr_short(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_gr_int	*pSrc = s;
	struct dbr_gr_int	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohs(pSrc->upper_disp_limit);
	pDest->lower_disp_limit 	= dbr_ntohs(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohs(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohs(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohs(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohs(pSrc->lower_warning_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohs(pSrc->value);
    	else {
		cvrt_short(&pSrc->value, &pDest->value, encode,num);
	}
}


/****************************************************************************
**	cvrt_gr_char()
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_gr_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else /*__STDC__*/
LOCAL void cvrt_gr_char(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif /*__STDC__*/
{
	struct dbr_gr_char	*pSrc = s;
	struct dbr_gr_char	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= pSrc->upper_disp_limit;
	pDest->lower_disp_limit 	= pSrc->lower_disp_limit;
    	pDest->upper_alarm_limit 	= pSrc->upper_alarm_limit;
    	pDest->upper_warning_limit 	= pSrc->upper_warning_limit;
    	pDest->lower_alarm_limit 	= pSrc->lower_alarm_limit;
    	pDest->lower_warning_limit 	= pSrc->lower_warning_limit;

    	if (num == 1)
    		pDest->value = pSrc->value;
    	else {
		memcpy(&pDest->value, &pSrc->value, num);
	}
}


/****************************************************************************
**	cvrt_gr_long()
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_gr_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_gr_long(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_gr_long	*pSrc = s;
	struct dbr_gr_long	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohl(pSrc->upper_disp_limit);
	pDest->lower_disp_limit 	= dbr_ntohl(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohl(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohl(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohl(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohl(pSrc->lower_warning_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohl(pSrc->value);
    	else {
		cvrt_long(&pSrc->value, &pDest->value, encode, num);
	}
}


/****************************************************************************
**	cvrt_gr_enum(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_gr_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_gr_enum(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_gr_enum	*pSrc = s;
	struct dbr_gr_enum	*pDest = d;

    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->no_str			= dbr_ntohs(pSrc->no_str);
    	memcpy(pDest->strs,pSrc->strs,sizeof(pSrc->strs));

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_enum(&(pSrc->value), &(pDest->value), encode, num);
	}
}


/****************************************************************************
**	cvrt_gr_double(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_gr_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_gr_double(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_gr_double	*pSrc = s;
	struct dbr_gr_double	*pDest = d;

    	/* these are same for vax to ieee or ieee to vax */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->precision		= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
		    htond(&s->value, &d->value);
		}
		else {
		    cvrt_double(&pSrc->value, &pDest->value, encode,num);
		}
    		htond(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		htond(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		htond(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		htond(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		htond(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		htond(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
		    ntohd(&pSrc->value, &pDest->value);
		}
		else {
		    cvrt_double(&pSrc->value, &pDest->value, encode,num);
		}
    		ntohd(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		ntohd(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		ntohd(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		ntohd(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
   	 	ntohd(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		ntohd(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
}



/****************************************************************************
**	cvrt_gr_float(s,d)
**		struct dbr_gr_float *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_gr_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_gr_float(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_gr_float     *pSrc = s;
	struct dbr_gr_float     *pDest = d;

    	/* these are same for vax to ieee or ieee to vax */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->precision		= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
		    htonf(&s->value, &d->value);
		}
		else {
		    cvrt_float(&pSrc->value, &pDest->value, encode,num);
		}
    		htonf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		htonf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		htonf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		htonf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		htonf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		htonf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
		    ntohf(&pSrc->value, &pDest->value);
		}
		else {
		    cvrt_float(&pSrc->value, &pDest->value, encode,num);
		}
    		ntohf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		ntohf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		ntohf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		ntohf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
   	 	ntohf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		ntohf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
}




/****************************************************************************
**	cvrt_ctrl_short(s,d)
**		struct dbr_gr_int *s  	pointer to source struct
**		struct dbr_gr_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_ctrl_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_ctrl_short(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_ctrl_int	*pSrc = s;
	struct dbr_ctrl_int	*pDest = d;

    	/* vax to ieee or ieee to vax -- same code */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohs(pSrc->upper_disp_limit);
    	pDest->lower_disp_limit 	= dbr_ntohs(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohs(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohs(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohs(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohs(pSrc->lower_warning_limit);
    	pDest->lower_ctrl_limit 	= dbr_ntohs(pSrc->lower_ctrl_limit);
    	pDest->upper_ctrl_limit 	= dbr_ntohs(pSrc->upper_ctrl_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohs(pSrc->value);
    	else {
		cvrtshort(&pSrc->value, &pDest->value, encode, num);
    	}
}

/****************************************************************************
**	cvrt_ctrl_long(s,d)
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_ctrl_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_ctrl_long(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_ctrl_long	*pSrc = s;
	struct dbr_ctrl_long	*pDest = d;

    	/* vax to ieee or ieee to vax -- same code */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohl(pSrc->upper_disp_limit);
    	pDest->lower_disp_limit 	= dbr_ntohl(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohl(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohl(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohl(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohl(pSrc->lower_warning_limit);
    	pDest->lower_ctrl_limit 	= dbr_ntohl(pSrc->lower_ctrl_limit);
    	pDest->upper_ctrl_limit 	= dbr_ntohl(pSrc->upper_ctrl_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohl(pSrc->value);
    	else {
		cvrtlong(&pSrc->value, &pDest->value, encode, num);
    	}
}

/****************************************************************************
**	cvrt_ctrl_short(s,d)
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_ctrl_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_ctrl_char(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_ctrl_char	*pSrc = s;
	struct dbr_ctrl_char	*pDest = d;

    	/* vax to ieee or ieee to vax -- same code */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);

    	pDest->upper_disp_limit 	= pSrc->upper_disp_limit;
    	pDest->lower_disp_limit 	= pSrc->lower_disp_limit;
    	pDest->upper_alarm_limit 	= pSrc->upper_alarm_limit;
    	pDest->upper_warning_limit 	= pSrc->upper_warning_limit;
    	pDest->lower_ctrl_limit 	= pSrc->lower_ctrl_limit;
    	pDest->upper_ctrl_limit 	= pSrc->upper_ctrl_limit;

    	if (num == 1)
    		pDest->value = pSrc->value;
    	else {
		memcpy(&pDest->value, &pSrc->value, num);
    	}
}


/****************************************************************************
**	cvrt_ctrl_double(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_ctrl_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_ctrl_double(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_ctrl_double	*pSrc = s;
	struct dbr_ctrl_double	*pDest = d;

	    /* these are the same for ieee to vaax or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
    	pDest->precision	= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
	    		htond(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_double(&pSrc->value, &pDest->value, encode, num);
		}
    		htond(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		htond(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		htond(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		htond(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		htond(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		htond(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		htond(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		htond(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
	    		ntohd(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_double(&pSrc->value, &pDest->value, encode, num);
		}
    		ntohd(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		ntohd(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		ntohd(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		ntohd(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		ntohd(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		ntohd(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		ntohd(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}

}




/****************************************************************************
**	cvrt_ctrl_float(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_ctrl_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_ctrl_float(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_ctrl_float	*pSrc = s;
	struct dbr_ctrl_float	*pDest = d;

	    /* these are the same for ieee to vaax or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
    	pDest->precision		= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
	    		htonf(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_float(&pSrc->value, &pDest->value, encode, num);
		}
    		htonf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		htonf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		htonf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		htonf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		htonf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		htonf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		htonf(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		htonf(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
	    		ntohf(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_float(&pSrc->value, &pDest->value, encode, num);
		}
    		ntohf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		ntohf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		ntohf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		ntohf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		ntohf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		ntohf(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		ntohf(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}

}



/****************************************************************************
**	cvrt_ctrl_enum(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_ctrl_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_ctrl_enum(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_ctrl_enum	*pSrc = s;
	struct dbr_ctrl_enum	*pDest = d;

    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->no_str			= dbr_ntohs(pSrc->no_str);
    	memcpy(pDest->strs,pSrc->strs,sizeof(pSrc->strs));

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_enum(&(pSrc->value), &(pDest->value), encode, num);
	}
}


/****************************************************************************
**	cvrt_sts_char(s,d)
**		struct dbr_sts_int *s  	pointer to source struct
**		struct dbr_sts_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_sts_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_sts_char(s,d,encode,num)
void			*s;		/* source		*/
void			*d;		/* destination		*/
int 			encode;		/* cvrt VAX to IEEE if T*/
unsigned long		num;		/* number of values	*/
#endif
{
	struct dbr_sts_char	*pSrc = s;
	struct dbr_sts_char	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

    	if (num == 1)	/* single value */
		pDest->value = pSrc->value;
    	else		/* array chan-- multiple pts */
    	{
		memcpy(&pDest->value, &pSrc->value, num);
    	}
}


/****************************************************************************
**	cvrt_sts_long(s,d)
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_sts_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_sts_long(s,d,encode,num)
void			*s;		/* source		*/
void			*d;		/* destination		*/
int 			encode;		/* cvrt VAX to IEEE if T*/
unsigned long		num;		/* number of values	*/
#endif
{
	struct dbr_sts_long	*pSrc = s;
	struct dbr_sts_long	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohl(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_long(&pDest->value, &pSrc->value, encode, num);
    	}
}



/****************************************************************************
**	cvrt_time_string(s,d)
**	      
**	converts fields of struct in VAX format to IEEE format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_time_string(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_time_string	*pSrc = s;
	struct dbr_time_string 	*pDest = d;
			
    	/* convert ieee to vax format or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* if single value */
		strcpy(pDest->value, pSrc->value);
    	else
		memcpy(pDest->value, pSrc->value, (MAX_STRING_SIZE * num));

}


/****************************************************************************
**	cvrt_time_short(s,d)
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_time_short(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_time_int	*pSrc = s;
	struct dbr_time_int	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_short(&pSrc->value, &pDest->value, encode, num);
    	}
}


/****************************************************************************
**	cvrt_time_float(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
cvrt_time_float(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_time_float	*pSrc = s;
	struct dbr_time_float	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

	cvrt_float(&pSrc->value, &pDest->value, encode, num);
}


/****************************************************************************
**	cvrt_time_double(s,d)
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
cvrt_time_double(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_time_double	*pSrc = s;
	struct dbr_time_double	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

	cvrt_double(&pSrc->value, &pDest->value, encode, num);
}



/****************************************************************************
**	cvrt_time_string(s,d)
**	      
**	converts fields of struct in VAX format to IEEE format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_time_string(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_time_string	*pSrc = s;
	struct dbr_time_string 	*pDest = d;
			
    	/* convert ieee to vax format or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);
    	if (num == 1)	/* if single value */
		strcpy(pDest->value, pSrc->value);
    	else
		memcpy(pDest->value, pSrc->value, MAX_STRING_SIZE*num);

}


/****************************************************************************
**	cvrt_time_short(s,d)
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_time_short(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif
{
	struct dbr_time_int	*pSrc = s;
	struct dbr_time_int	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_short(&pSrc->value, &pDest->value, encode, num);
    	}
}



/****************************************************************************
**	cvrt_time_enum(s,d)
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else /*__STDC__*/
LOCAL void cvrt_time_enum(s,d,encode,num)
void			*s;			/* source		*/
void			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
unsigned long		num;			/* number of values	*/
#endif /*__STDC__*/
{
	struct dbr_time_enum	*pSrc = s;
	struct dbr_time_enum	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);
    	if (num == 1)
        	d->value	= dbr_ntohs(s->value);
    	else {
		cvrt_enum(&pSrc->value,&pDest->value,encode,num)
    	}
}


/****************************************************************************
**	cvrt_sts_char(s,d)
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_time_char(s,d,encode,num)
void			*s;		/* source		*/
void			*d;		/* destination		*/
int 			encode;		/* cvrt VAX to IEEE if T*/
unsigned long		num;		/* number of values	*/
#endif
{
	struct dbr_time_char	*pSrc = s;
	struct dbr_time_char	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* single value */
		pDest->value = pSrc->value;
    	else		/* array chan-- multiple pts */
    	{
		memcpy(&pDest->value, &pSrc->value, num);
    	}
}

/****************************************************************************
**	cvrt_time_long(s,d)
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

#ifdef __STDC__
LOCAL void cvrt_time_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt VAX to IEEE if T	*/
unsigned long	num			/* number of values		*/
)
#else
LOCAL void cvrt_time_long(s,d,encode,num)
void			*s;		/* source		*/
void			*d;		/* destination		*/
int 			encode;		/* cvrt VAX to IEEE if T*/
unsigned long		num;		/* number of values	*/
#endif
{
	struct dbr_time_long	*pSrc = s;
	struct dbr_time_long	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohl(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_long(&pDest->value, &pSrc->value, encode, num);
    	}
}


